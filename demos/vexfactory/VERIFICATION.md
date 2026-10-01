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
- Native-resolution layout checks cover 960x640 through 1920x1080 with 100-140% text settings.
- Software-rendered title, campaign, briefing, game, and ending screens render at 960x640 with 140% text.
- The Windows Release ZIP includes the executable, CC0 tilemap, instructions, and library license notices.
- The packaged Linux executable finds its nearby artwork and passes the graphical smoke check.
- A default Vecs configuration and the asset-free test configuration do not download graphics dependencies.

The existing Vecs SIMD, no-SIMD, and small-configuration suites also passed before the campaign conversion.
The conversion does not modify `vecs.h` or `vecs_test.cpp`.

## macOS coverage

The CI workflow includes macOS simulation, campaign, persistence, and layout tests.
It also compiles the graphical Cocoa/high-DPI game on macOS.
A native Retina playtest is not replaced by those checks.

The interface no longer uses the downscaled render-texture canvas from the demo.
It draws in native logical coordinates, bakes the system font at framebuffer density, and keeps interface size independent of factory zoom.
macOS uses the system Arial font when available and accepts `--font FILE.ttf` as an override.

## Deliberate limits

- The campaign has ten chapters and one story ending, with optional medal text and replay.
- The original sandbox retains its one-to-one recipes.
- Packages are platform-specific ZIP archives, not installers or notarized macOS bundles.
- The game uses installed system fonts. It does not distribute font files.
- Saves are versioned local files, not cloud saves.
- No network access occurs during gameplay.
