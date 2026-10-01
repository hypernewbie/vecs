#!/usr/bin/env python3
"""X11 input integration test. Run with LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a.
Uses only Python's standard library, libX11, and libXtst. Player saves are never used.
"""
import argparse
import ctypes as c
import os
from pathlib import Path
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser()
parser.add_argument("binary", type=Path)
parser.add_argument("--output", type=Path, default=Path("temp/vexfactory/graphical-tests"))
parser.add_argument("--density", type=int, choices=(1, 2), default=1, help="X11 desktop DPI multiplier")
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
output = Path(tempfile.mkdtemp(prefix="campaign-ui-", dir=args.output))
profile = output / "profile"

x = c.CDLL("libX11.so.6")
t = c.CDLL("libXtst.so.6")
x.XOpenDisplay.restype = c.c_void_p
x.XDefaultRootWindow.argtypes = [c.c_void_p]
x.XDefaultRootWindow.restype = c.c_ulong
x.XQueryTree.argtypes = [c.c_void_p, c.c_ulong, c.POINTER(c.c_ulong), c.POINTER(c.c_ulong), c.POINTER(c.POINTER(c.c_ulong)), c.POINTER(c.c_uint)]
x.XFetchName.argtypes = [c.c_void_p, c.c_ulong, c.POINTER(c.c_char_p)]
x.XFree.argtypes = [c.c_void_p]
x.XStringToKeysym.argtypes = [c.c_char_p]
x.XStringToKeysym.restype = c.c_ulong
x.XKeysymToKeycode.argtypes = [c.c_void_p, c.c_ulong]
x.XKeysymToKeycode.restype = c.c_uint
for name, types in {
    "XFlush": [c.c_void_p],
    "XSetInputFocus": [c.c_void_p, c.c_ulong, c.c_int, c.c_ulong],
    "XResizeWindow": [c.c_void_p, c.c_ulong, c.c_uint, c.c_uint],
    "XMoveWindow": [c.c_void_p, c.c_ulong, c.c_int, c.c_int],
}.items():
    getattr(x, name).argtypes = types
for name, types in {
    "XTestFakeKeyEvent": [c.c_void_p, c.c_uint, c.c_int, c.c_ulong],
    "XTestFakeMotionEvent": [c.c_void_p, c.c_int, c.c_int, c.c_int, c.c_ulong],
    "XTestFakeButtonEvent": [c.c_void_p, c.c_uint, c.c_int, c.c_ulong],
}.items():
    getattr(t, name).argtypes = types

class WindowAttributes(c.Structure):
    _fields_ = [("x", c.c_int), ("y", c.c_int), ("width", c.c_int), ("height", c.c_int), ("border_width", c.c_int), ("depth", c.c_int), ("visual", c.c_void_p), ("root", c.c_ulong), ("window_class", c.c_int), ("bit_gravity", c.c_int), ("win_gravity", c.c_int), ("backing_store", c.c_int), ("backing_planes", c.c_ulong), ("backing_pixel", c.c_ulong), ("save_under", c.c_int), ("colormap", c.c_ulong), ("map_installed", c.c_int), ("map_state", c.c_int), ("all_event_masks", c.c_long), ("your_event_mask", c.c_long), ("do_not_propagate_mask", c.c_long), ("override_redirect", c.c_int), ("screen", c.c_void_p)]

x.XGetWindowAttributes.argtypes = [c.c_void_p, c.c_ulong, c.POINTER(WindowAttributes)]
display = x.XOpenDisplay(None)
assert display, "An X11 display is required. Run this test through xvfb-run."
root = x.XDefaultRootWindow(display)
if args.density == 2:
    subprocess.run(["xrdb", "-merge"], input="Xft.dpi: 192\n", text=True, check=True)
process = None
logs = []


def key_state(name, down):
    code = x.XKeysymToKeycode(display, x.XStringToKeysym(name.encode()))
    assert code, name
    t.XTestFakeKeyEvent(display, code, int(down), 0)
    x.XFlush(display)


def key(name, command=False):
    if command:
        key_state("Control_L", True)
    key_state(name, True)
    time.sleep(.065)
    key_state(name, False)
    if command:
        key_state("Control_L", False)
    time.sleep(.08)


def move(px, py):
    t.XTestFakeMotionEvent(display, -1, int(px * args.density), int(py * args.density), 0)
    x.XFlush(display)
    time.sleep(.08)


def button(down, which=1):
    t.XTestFakeButtonEvent(display, which, int(down), 0)
    x.XFlush(display)
    time.sleep(.08)


def click(px, py):
    move(px, py)
    button(True)
    button(False)


def start(number):
    global process
    log_path = output / ("run-%d.log" % number)
    log = log_path.open("w")
    logs.append(log)
    process = subprocess.Popen([str(args.binary.resolve()), "--size", "960", "640", "--save-dir", str(profile), "--no-audio", "--frames", "3600"], stdout=log, stderr=subprocess.STDOUT)
    window = 0
    deadline = time.monotonic() + 45
    while time.monotonic() < deadline:
        children = c.POINTER(c.c_ulong)()
        count, rr, parent = c.c_uint(), c.c_ulong(), c.c_ulong()
        x.XQueryTree(display, root, c.byref(rr), c.byref(parent), c.byref(children), c.byref(count))
        for i in range(count.value):
            name = c.c_char_p()
            if x.XFetchName(display, children[i], c.byref(name)):
                if name.value and name.value.startswith(b"VexFactory: The Last Freight"):
                    window = children[i]
                x.XFree(name)
        if children:
            x.XFree(children)
        if window:
            break
        assert process.poll() is None, "The game exited before opening its window."
        time.sleep(.05)
    assert window, "No game window found."
    # GLFW assigns the title before it has created the GL context and mapped
    # the window. A fixed delay races cold llvmpipe initialization on CI.
    ready = False
    while time.monotonic() < deadline:
        assert process.poll() is None, "The game exited during initialization."
        attributes = WindowAttributes()
        mapped = x.XGetWindowAttributes(display, window, c.byref(attributes)) and attributes.map_state == 2
        if mapped and b"GAME: Ready" in log_path.read_bytes():
            ready = True
            break
        time.sleep(.05)
    assert ready, "The window or renderer did not become ready within 45 seconds."
    x.XMoveWindow(display, window, 0, 0)
    x.XSetInputFocus(display, window, 1, 0)
    x.XFlush(display)
    time.sleep(.2)
    return window


def snapshot():
    key("s", command=True)
    time.sleep(.1)
    data = (profile / "campaign.sav").read_bytes()
    header, payload = data.split(b"\n", 1)
    magic, version, checksum = header.decode().split()
    assert magic == "VEX_FACTORY_SAVE" and version == "1"
    hash_value = 14695981039346656037
    for b in payload:
        hash_value = ((hash_value ^ b) * 1099511628211) & ((1 << 64) - 1)
    assert hash_value == int(checksum, 16), "Partial save observed."
    tokens = iter(payload.decode().split())
    def integer():
        return int(next(tokens))
    def number():
        return float(next(tokens))
    result = {"tokens": integer(), "scale": number(), "muted": integer()}
    result["stars"], result["times"] = [], []
    for _ in range(10):
        result["stars"].append(integer())
        result["times"].append(number())
    result["upgrades"] = [integer() for _ in range(3)]
    result["active"], result["mission"], result["paused"] = integer(), integer(), integer()
    assert result["active"]
    result["factory_tiers"] = [integer() for _ in range(3)]
    result["phase"] = integer()
    result["produced"], result["scrap"], result["elapsed"] = integer(), integer(), number()
    result["credits"], result["spent"], result["refunds"] = integer(), integer(), integer()
    result["delivered"] = [integer() for _ in range(4)]
    result["ore"] = [integer() for _ in range(384)]
    result["buildings"] = {}
    for _ in range(integer()):
        px, py, direction, tool, locked, paid = [integer() for _ in range(6)]
        cooldown, progress = number(), number()
        stored, plates, mask, working, ready, left, filter_type = [integer() for _ in range(7)]
        result["buildings"][px, py] = (tool, direction, locked, paid, stored, plates)
    result["parcels"] = integer()
    return result


class XImage(c.Structure):
    _fields_ = [("width", c.c_int), ("height", c.c_int), ("xoffset", c.c_int), ("format", c.c_int), ("data", c.c_void_p), ("byte_order", c.c_int), ("bitmap_unit", c.c_int), ("bitmap_bit_order", c.c_int), ("bitmap_pad", c.c_int), ("depth", c.c_int), ("bytes_per_line", c.c_int), ("bits_per_pixel", c.c_int), ("red_mask", c.c_ulong), ("green_mask", c.c_ulong), ("blue_mask", c.c_ulong), ("obdata", c.c_void_p)]
x.XGetImage.argtypes = [c.c_void_p, c.c_ulong, c.c_int, c.c_int, c.c_uint, c.c_uint, c.c_ulong, c.c_int]
x.XGetImage.restype = c.POINTER(XImage)
x.XDestroyImage.argtypes = [c.POINTER(XImage)]


def capture(name, width, height):
    width, height = width * args.density, height * args.density
    image = x.XGetImage(display, root, 0, 0, width, height, c.c_ulong(-1), 2)
    assert image
    im = image.contents
    assert im.bits_per_pixel == 32 and im.red_mask == 0xff0000
    pixels = c.string_at(im.data, im.bytes_per_line * im.height)
    rgb = bytearray()
    for py in range(height):
        row = pixels[py * im.bytes_per_line:py * im.bytes_per_line + width * 4]
        for i in range(0, len(row), 4):
            rgb.extend((row[i + 2], row[i + 1], row[i]))
    (output / (name + ".ppm")).write_bytes(("P6\n%d %d\n255\n" % (width, height)).encode() + rgb)
    x.XDestroyImage(image)


def pixel(px, py):
    image = x.XGetImage(display, root, int(px * args.density), int(py * args.density), 1, 1, c.c_ulong(-1), 2)
    assert image and image.contents.bits_per_pixel == 32
    blue, green, red, _ = c.string_at(image.contents.data, 4)
    x.XDestroyImage(image)
    return red, green, blue


try:
    window = start(1)
    key("Return")  # New campaign briefing.
    key("Return")  # Planning phase, with the free miner and two prefabricated belts.
    before = snapshot()
    assert before["mission"] == 0 and before["phase"] == 1 and before["credits"] == 240
    assert before["elapsed"] == 0
    key("1")
    # The full-width viewport uses readable 40-point cells, not a shrunken map.
    # Same logical positions at 1x and 2x DPI; native input is scaled above.
    def paint_route():
        move(4.5 * 40, -8 + 3.5 * 40)
        button(True)
        move(21.5 * 40, -8 + 3.5 * 40)
        button(False)
    floor_pixel = pixel(12.5 * 40, -8 + 3.5 * 40)
    paint_route()
    belt_pixel = pixel(12.5 * 40, -8 + 3.5 * 40)
    assert belt_pixel != floor_pixel and belt_pixel[2] > floor_pixel[2], "The rendered belt does not match the clicked logical tile."
    capture("minimal-hud-%dx" % args.density, 960, 640)
    painted = snapshot()
    assert (21, 3) in painted["buildings"] and painted["credits"] == 132
    key("z", command=True)
    undone = snapshot()
    assert (4, 3) not in undone["buildings"] and undone["credits"] == 240
    paint_route()
    key("space")
    key("Tab")
    key("Tab")
    time.sleep(1.0)
    running = snapshot()
    assert running["phase"] == 2 and running["produced"] > 0 and 0 < running["elapsed"] < 15
    key("Escape")
    click(752, 601)  # Save and quit from the factory menu.
    assert process.wait(timeout=5) == 0
    # Reload the actual player save, not a test fixture or direct simulation state.
    window = start(2)
    key("Return")
    loaded = snapshot()
    assert loaded["paused"] and loaded["produced"] >= running["produced"]
    elapsed = loaded["elapsed"]
    key("F11")  # Borderless mode must not switch the UI into framebuffer pixels.
    time.sleep(.25)
    fullscreen = snapshot()
    assert fullscreen["elapsed"] == elapsed and fullscreen["scale"] == loaded["scale"]
    capture("borderless-%dx" % args.density, 1600, 1000)
    key("F11")
    time.sleep(.25)
    x.XResizeWindow(display, window, 1280 * args.density, 720 * args.density)
    x.XFlush(display)
    time.sleep(.3)
    key("equal", command=True)
    key("equal", command=True)
    key("Home")
    scaled = snapshot()
    assert abs(scaled["scale"] - 1.2) < .001 and scaled["elapsed"] == elapsed
    move(313, 257)
    key("e")
    inspected = snapshot()
    assert inspected["elapsed"] == elapsed
    capture("native-large-text", 1280, 720)
    key("Escape")  # Close the optional inspector, not the production scene.
    key("o")
    capture("optional-orders", 1280, 720)
    # Clicks through a details overlay must not place or demolish floor parts.
    before_overlay = snapshot()
    click(1050, 310)
    after_overlay = snapshot()
    assert after_overlay["buildings"] == before_overlay["buildings"] and after_overlay["credits"] == before_overlay["credits"]
    key("Escape")
    move(313, 257)
    # World zoom is independent of text size and never advances a paused factory.
    button(True, 4)
    button(False, 4)
    zoomed = snapshot()
    assert zoomed["scale"] == scaled["scale"] and zoomed["elapsed"] == elapsed
    key("space")
    key("Tab")
    key("Tab")
    time.sleep(3)
    won = snapshot()
    assert won["phase"] == 3 and won["stars"][0] == 3 and won["tokens"] == 5
    assert won["delivered"][0] == 12
    capture("campaign-clearance", 1280, 720)
    key("Return")  # Next chapter briefing.
    key("Return")  # Replacing the completed factory requires confirmation.
    key("Escape")  # Cancel must not lose the completed dispatch.
    canceled = snapshot()
    assert canceled["mission"] == 0 and canceled["stars"][0] == 3
    key("Return")
    key("Return")
    next_chapter = snapshot()
    assert next_chapter["mission"] == 1 and next_chapter["phase"] == 1
    assert next_chapter["credits"] == 420 and next_chapter["stars"][0] == 3
    key("Escape")
    click(244, 250)  # Workshop from pause menu at 120% text.
    click(1095, 197)  # Buy tier one conveyor bearings.
    upgraded = snapshot()
    assert upgraded["tokens"] == 3 and upgraded["upgrades"][0] == 1
    assert upgraded["factory_tiers"][0] == 0  # Upgrades apply to the next dispatch.
    key("Escape")  # Back to pause.
    click(1036, 673)  # Save and quit at 1280x720 / 120% text.
    assert process.wait(timeout=5) == 0
    print("Graphical campaign test passed at %dx DPI: minimal HUD, rendered/clicked tile alignment, planning, fast paint, undo, production, atomic save/reload, resize, large text, zoom, victory, next chapter, confirmation and research." % args.density)
    print("Artifacts:", output)
finally:
    if process is not None and process.poll() is None:
        process.terminate()
        process.wait(timeout=5)
    for log in logs:
        log.close()
