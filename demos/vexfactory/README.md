# VexFactory: The Last Freight

A storm cuts the line that supplies the settlements of Lumen.
You are its last working industrial engineer.
Dispatcher Iona and mechanic Vale guide you through ten freight-yard repairs, from the first signal relay to the final evacuation train.

This is a finite campaign, not an endless counter demo.
Each chapter has a briefing, a map, a freight order, constraints, a result screen, and a story report.
The final chapter leads to an ending.
The original free-build sandbox remains available from the title menu.

## The planning game

- **Credits:** parts cost money. Required deliveries pay credits. Surplus cargo does not.
- **Deposits:** miners need finite ore deposits. The campaign has no infinite ore source.
- **Power:** installed machines reserve power. Generators provide twelve units each.
- **Routes:** splitters alternate between forward and left outputs. Sorters separate the selected product from other cargo.
- **Inputs:** assemblers need both gears and plates. A single straight production line is not sufficient.
- **Dispatch:** planning is free. Launch starts the production timer. Pause stops it.
- **Losses:** demolition scraps cargo and refunds only part of the price you paid.
- **Medals:** each clearance earns a star. Optional time and construction targets award two more.
- **Research:** spend earned tokens on conveyor speed, processing rate, or recovery crews.

Every chapter has a tested solution without research upgrades.
Later time medals require better throughput or research, not faster mouse clicks.
A failed dispatch can always be retried with its original budget and deposits.
Completed chapters and research survive a retry.

### Campaign

| Chapter | Dispatch | Main challenge |
| --- | --- | --- |
| 1 | A Light in the Yard | Directional belts and the first ore shipment |
| 2 | Shelter Before Dawn | Plates, a furnace, and a limited power supply |
| 3 | The Stalled Lift | Two-plate gear production |
| 4 | One Furnace, Three Wards | Split one powered line between two orders |
| 5 | A Heart for the Pumps | Feed both assembler inputs |
| 6 | Across the Flood | Choke points and additional generation |
| 7 | The Night Shift | Parallel production and mixed orders |
| 8 | The Last Convoy | A tighter departure allowance |
| 9 | Keep the Lights On | Build the entire power supply |
| 10 | Homebound | Balance all products for the final train |

## Build and run

Run these commands from the Vecs repository root.
The graphical game needs CMake 3.25+, Ninja, a C/C++ compiler, and desktop OpenGL 3.3.

### Windows

Open a Visual Studio developer terminal with clang-cl on the path.

```powershell
cmake -S . -B temp/vexfactory-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DBUILD_VEX_FACTORY=ON
cmake --build temp/vexfactory-build --target vex_factory vex_factory_assets
.\temp\vexfactory-build\demos\vexfactory\vex_factory.exe
```

### macOS

Install Ninja and CMake 3.25+.
Use Apple's Clang compiler:

```sh
cmake -S . -B temp/vexfactory-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DBUILD_VEX_FACTORY=ON
cmake --build temp/vexfactory-build --target vex_factory vex_factory_assets
./temp/vexfactory-build/demos/vexfactory/vex_factory
```

GLFW uses Cocoa on macOS.
The window enables high-DPI support.
The interface uses a system font, baked at the current framebuffer density.
A native Retina playtest is still separate from compilation and software-rendered layout tests.

### Linux

The existing Vecs build uses Clang and libc++.
The graphical game also needs the OpenGL and X11 development packages.
See the [raylib Linux instructions](https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux) for your distribution.

```sh
cmake -S . -B temp/vexfactory-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DBUILD_VEX_FACTORY=ON
cmake --build temp/vexfactory-build --target vex_factory vex_factory_assets
./temp/vexfactory-build/demos/vexfactory/vex_factory
```

## Readable interface

The factory uses the full width of the window.
A compact status strip and a single-row toolbar replace the permanent sidebar and large status cards.
Part information, orders, and inspection open only on request. Esc closes them without opening the pause menu.

Rendering and mouse input share logical window coordinates, including Retina and borderless mode.
The framebuffer resolution determines sharpness, not interface size.
Body text starts at 24 logical pixels. Secondary text starts at 20.
Default factory tiles are at least 40 logical pixels wide.
Home or the Fit button fits the entire floor. The wheel changes factory zoom without changing text size.
Paragraphs wrap, and long menus scroll.

Use the text-size button on the title or pause menu.
You can also use `Ctrl/Cmd` with `+` or `-`, or start with `--ui-scale 1.5`.
The supported text scale is 100-200%. Existing campaign saves remain readable.

The game uses Segoe UI on Windows, Arial on macOS, and DejaVu Sans or Liberation Sans on Linux.
It does not copy or distribute these fonts.
If no suitable system font is present, the game uses raylib's default font.
Use `--font FILE.ttf` to select a different installed font.

## Controls

| Control | Action |
| --- | --- |
| `Enter` | Continue, begin a briefing, or confirm the current dispatch action |
| `1`-`5` | Conveyor, miner, smelter, gear press, assembler |
| `6` | Freight dock in the sandbox. Campaign docks are protected. |
| `7`-`9` | Splitter, sorter, generator |
| `0` | Demolition brush |
| Left mouse | Place a part |
| Hold left mouse | Paint conveyors or demolish tiles |
| Shift + left mouse | Replace a different part |
| Right mouse | Demolish a tile |
| `R` | Rotate the selected output direction clockwise |
| `E` | Pick the tile under the pointer and toggle its inspector |
| `B` | Toggle information for the selected part |
| `O` | Toggle the orders panel |
| Alt + left mouse | Inspect without construction |
| `F` | Change the sorter filter under the pointer |
| `Space` | Launch, pause, or resume production |
| `Tab` | Cycle 1x, 2x, and 4x simulation speed |
| Wheel over the floor | Zoom around the pointer |
| Middle mouse / WASD | Pan the floor |
| `Home` | Fit the floor to the viewport |
| Wheel over a panel | Scroll its contents |
| `Ctrl/Cmd+Z` | Undo a planning stroke, before launch |
| `Ctrl/Cmd+S` | Save the game |
| `Ctrl/Cmd +/-` | Change interface text size |
| `M` | Mute or enable procedural sounds |
| `F11` | Toggle borderless full-window mode |
| `N` in the sandbox | Clear the floor after confirmation |
| `F1` | Open the field notes |
| `Esc` | Close details, open the pause menu, or return from a menu |

The title menu includes the campaign, workshop, field notes, and classic sandbox.
The pause menu includes save, restart, research, and return to the campaign board.
Restart and replacement actions require confirmation.

## Production rules

Machines emit onto an adjacent conveyor in their arrow direction.
Processors accept the correct input from any side.
Each conveyor tile reserves one item slot.
Each processor holds four of each accepted input and one item in production or at its output.
Wrong cargo and filled dock quotas cause backpressure.
Coral outlines mark items that cannot advance.
Required ore pays 1 credit, plates pay 4, gears pay 10, and engines pay 28.

| Machine | Campaign recipe | Base production time | Reserved power |
| --- | --- | --- | --- |
| Miner | Produce one ore from its deposit | 0.75 seconds between emissions | 2 |
| Smelter | One ore to one plate | 1.1 seconds | 3 |
| Gear press | Two plates to one gear | 1.5 seconds | 4 |
| Assembler | Two gears and one plate to one engine | 2.0 seconds | 5 |
| Generator | Supply twelve power | No production cycle | 0 |

The classic sandbox keeps the original one-to-one recipes, free parts, and unlimited mining.
Campaign docks have cargo icons and reject products that they do not request.
Docks turn gold when their quota is filled.

Rotating a machine preserves its inventory and progress.
Demolition refunds 70% of the amount paid, rising to 85% with research.
Free tutorial parts refund nothing.
A generator cannot be removed while its power is committed to other machines.

## Progress, research, and saves

A first clearance earns two research tokens plus the chapter's stars.
An improved medal earns only the difference.
Repeating the same result cannot farm tokens.
Research has three tiers per track and applies to the next dispatch.

The game saves periodically, on dispatch completion, on exit, and on explicit request.
It resumes a loaded factory paused.
An active save includes buffers, work in progress, moving cargo, remaining deposits, credits, research, medals, and the dispatch timer.

Default save directories:

| Platform | Directory |
| --- | --- |
| Windows | `%LOCALAPPDATA%/VexFactory` |
| macOS | `~/Library/Application Support/VexFactory` |
| Linux | `$XDG_DATA_HOME/vexfactory`, or `~/.local/share/vexfactory` |

The file is `campaign.sav`. The previous valid file is `campaign.sav.bak`.
The format has a version and checksum.
The loader checks geometry, material conservation, protected docks, the credit ledger, and campaign progress.
A bad file does not replace the running factory.
The loader can recover the backup.
Starting over after an unreadable save requires confirmation and preserves the old file as `campaign.sav.corrupt`.

Use `--save-dir DIR` for a separate profile.
Use `--no-save` for a temporary session that does not load or write player saves.
Smoke and screenshot runs always use temporary, unsaved state.
Use `--no-audio` on hosts without an audio device.

## Downloads and packaging

Both VexFactory build flags default to `OFF`.
A normal Vecs configure or build does not fetch graphics libraries or artwork.
The graphical flag fetches raylib 6.0 and GLFW 3.5.1 with pinned SHA-256 checksums.
Raylib uses this GLFW target instead of its older bundled copy.
On Linux, X11 is the default. Enable Wayland with `-DGLFW_BUILD_WAYLAND=ON` if its build dependencies are available.

The artwork download is separate from compilation:

```sh
cmake -P demos/vexfactory/fetch_assets.cmake
```

The script downloads [Kenney Tiny Factory 1.0](https://kenney.nl/assets/tiny-factory), checks its SHA-256 checksum, and extracts the pack.
The ZIP is about 90 KB and has a CC0 license.
The script keeps its original `License.txt`.

The default asset directory is `temp/vexfactory/assets`.
Git ignores `temp/`, including builds, packages, and screenshots.
The application never downloads files at runtime.
Use `VEX_FACTORY_ASSET_DIR` during configuration or `--assets DIR` at runtime for another location.

To create a platform-specific ZIP from a Release build:

```sh
cmake --build temp/vexfactory-build --target vex_factory_package
```

This explicit target also fetches the artwork.
The archive contains the executable, its nearby `assets/` folder, instructions, and license notices.
It does not contain system fonts or player saves.
The output is in the build directory's `vexfactory-dist/` folder.
This is an archive, not an installer or a notarized macOS application.
Linux recipients also need the libc++ runtime and desktop OpenGL.

## Tests

The simulation and game tests do not need graphics libraries, artwork, or a display.
They retain the Vecs minimum of CMake 3.20.

```sh
cmake -S . -B temp/vexfactory-tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DBUILD_VEX_FACTORY_TESTS=ON
cmake --build temp/vexfactory-tests --target vex_factory_test vex_factory_campaign_test vex_factory_save_test vex_factory_layout_test
ctest --test-dir temp/vexfactory-tests --output-on-failure
```

On Windows, use `clang-cl` instead of `clang++`.
The graphical build includes these test targets too.

The tests retain the original sandbox checks and cover all campaign systems.
Test-only layouts purchase and complete all ten chapters with no upgrades.
Persistence tests cover deterministic continuation, malformed states, truncated files, and backup recovery.
Layout tests cover small windows, 100-200% text, and logical-to-framebuffer mapping at 1x, 1.5x, 2x, and 3x DPI.

Graphical smoke examples:

```sh
./temp/vexfactory-build/demos/vexfactory/vex_factory --smoke-test
./temp/vexfactory-build/demos/vexfactory/vex_factory --mission 5 --screen briefing --size 960 640 --ui-scale 1.3 --screenshot temp/vexfactory/briefing.png
```

For a display-free Linux host, prepend `LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a`.
The X11 input integration test also needs `libXtst`:

```sh
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a -s '-screen 0 1600x1000x24' python3 demos/vexfactory/graphical_test.py temp/vexfactory-build/demos/vexfactory/vex_factory
```

This test plays the first chapter through real input, saves and reloads, changes text size and zoom, advances the campaign, and buys research.
It also checks that a rendered conveyor appears on the tile clicked by the pointer.
For a 2x DPI desktop, use `-s '-screen 0 3200x2000x24 -noreset'` with `xvfb-run`, and add `--density 2` to the script.
The 2x test also needs `xrdb`, usually in the `x11-xserver-utils` package.
Its profiles and images stay under gitignored `temp/`.
CI includes asset-free tests on Windows, Linux, and macOS, macOS graphical compilation, and Linux graphical/input tests.

## Implementation

All factory state lives in Vecs components. The spatial grid stores entity handles.
Cached queries drive transport, production, and status counters.
The simulation uses a fixed 60 Hz step.
Cached-query matches update in spatial order, so recycled entity IDs do not change merge priority after a reload.
Structural changes flush after the update callbacks return.
No component pointer is used after those changes.
Campaign ore conservation counts a plate as one ore, a gear as two, and an engine as five.
The UI, story, research, and save format do not change `vecs.h`.
