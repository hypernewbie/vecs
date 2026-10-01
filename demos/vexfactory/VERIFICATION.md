# Verification notes

## Local checks

The implementation passed these local checks:

- Windows clang-cl Debug builds and all four game test suites.
- Windows clang-cl Release builds and all four game test suites.
- Linux Clang/libc++ builds and all four game test suites.
- Linux AddressSanitizer and UndefinedBehaviorSanitizer checks.
- Test-only factory layouts complete all ten chapters with no research upgrades.
- Save/reload preserves exact merge behavior through subsequent construction and demolition.
- Persistence checks reject corrupt checksums, truncated files, invalid geometry, forged credit ledgers, and unprotected campaign docks.
- Backup recovery preserves the previous valid profile.
- X11 input testing completes the first chapter through actual mouse and keyboard input, reloads its factory, advances the campaign, and purchases research.
- Native-resolution layout checks cover 960x640 through 1920x1080 with 100-200% text settings.
- Software-rendered title, campaign, briefing, game, and ending screens render at 960x640 with 200% text.
- X11 input tests pass at 1x and 2x DPI, including borderless mode and clicks through optional detail panels.
- Pixel probes check that conveyor placement changes the rendered tile under the pointer, not a smaller or offset map.
- Coordinate tests cover Retina/Wayland points and Windows/X11 pixels at 1x, 1.5x, 2x, and 3x DPI.
- The Windows Release ZIP includes the executable, CC0 tilemap and pixel fonts, their licenses, instructions, and library license notices.
- The packaged Linux executable finds its nearby artwork and passes the graphical smoke check.
- A default Vecs configuration and the asset-free test configuration do not download graphics dependencies.

The existing Vecs SIMD, no-SIMD, and small-configuration suites also passed before the campaign conversion.
The conversion does not modify `vecs.h` or `vecs_test.cpp`.

## macOS coverage

The CI workflow includes macOS simulation, campaign, persistence, and layout tests.
It also compiles the graphical Cocoa/high-DPI game on macOS.
A native Retina playtest is not replaced by those checks.

The factory view now uses the full window width, without a permanent sidebar or large status cards.
Rendering and mouse input share an explicit logical projection.
This corrects the earlier 2D-camera bug: raylib discarded DPI scaling during factory rendering, but input still used logical coordinates.
The Kenney pixel fonts use a small bitmap atlas with nearest-neighbor sampling.
Glyph sizes and positions align to framebuffer pixels. Factory zoom does not change text size.
All platforms use the same CC0 fonts and accept `--font FILE.ttf` as a body-text override.
Campaign data, production rules, and the existing version-1 save format remain unchanged.

## Deliberate limits

- The campaign has ten chapters and one story ending, with optional medal text and replay.
- The original sandbox retains its one-to-one recipes.
- Packages are platform-specific ZIP archives, not installers or notarized macOS bundles.
- Fonts come from the opt-in Kenney CC0 download. Normal Vecs builds do not fetch them.
- Saves are versioned local files, not cloud saves.
- No network access occurs during gameplay.
