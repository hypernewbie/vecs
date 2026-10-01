# VexFactory

VexFactory is a small factory sandbox that uses Vecs and raylib.
Place conveyors and machines to turn ore into plates, gears, and engines.
Ship 20 plates, 15 gears, and 10 engines to complete the contracts.
The simulation continues after you complete the contracts.

The starter floor contains three complete production lines.
Use the empty floor to build your own layout.
Parts are unlimited. Miners do not need deposits, power, or fuel.

## Build and run

Run these commands from the Vecs repository root.
The graphical build needs CMake 3.20+, Ninja, a C/C++ compiler, and desktop OpenGL 3.3.

### Windows

Open a Visual Studio developer terminal with clang-cl on the path.

```powershell
cmake -S . -B temp/vexfactory-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DBUILD_VEX_FACTORY=ON
cmake --build temp/vexfactory-build --target vex_factory vex_factory_assets
.\temp\vexfactory-build\demos\vexfactory\vex_factory.exe
```

### Linux and macOS

The existing Vecs build uses Clang and libc++.
On Linux, raylib also needs the development packages for OpenGL and X11.
See the [raylib Linux instructions](https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux) for the packages for your distribution.

```sh
cmake -S . -B temp/vexfactory-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DBUILD_VEX_FACTORY=ON
cmake --build temp/vexfactory-build --target vex_factory vex_factory_assets
./temp/vexfactory-build/demos/vexfactory/vex_factory
```

## Downloads and assets

Both VexFactory build flags default to `OFF`.
A normal Vecs configure or build does not fetch raylib or artwork.
The graphical flag fetches raylib 5.5 with a pinned SHA-256 checksum.
CMake stores raylib inside the build directory.

The artwork download is separate from compilation.
Only the explicit `vex_factory_assets` target downloads the artwork.
You can also download it without a graphical build:

```sh
cmake -P demos/vexfactory/fetch_assets.cmake
```

The script downloads [Kenney Tiny Factory 1.0](https://kenney.nl/assets/tiny-factory), checks its SHA-256 checksum, and extracts the pack.
The ZIP is about 90 KB. The pack has a CC0 license.
The script keeps the original `License.txt` with the extracted artwork.
Attribution is optional. VexFactory credits Kenney in its interface.

The default asset directory is `temp/vexfactory/assets`.
Git ignores the entire `temp/` directory, including builds and screenshots.
No images, archives, or third-party source files belong in this demo directory.

For a different directory, set `VEX_FACTORY_ASSET_DIR` during configuration:

```sh
cmake -S . -B temp/vexfactory-build -DBUILD_VEX_FACTORY=ON -DVEX_FACTORY_ASSET_DIR=/path/to/assets
```

You can also select an asset directory at runtime with `--assets DIR`.
The application does not download files at runtime.
If the artwork is missing, the application shows the fetch command and exits.

## Controls

| Control | Action |
| --- | --- |
| `1` | Select a conveyor |
| `2` | Select an ore miner |
| `3` | Select a smelter |
| `4` | Select a gear press |
| `5` | Select an assembler |
| `6` | Select shipping |
| `0` | Select the eraser |
| Left mouse | Place the selected part |
| Hold left mouse | Paint conveyors or erase tiles |
| Right mouse | Erase a tile |
| `R` | Rotate the selected output direction clockwise |
| `E` | Select the part and direction at the pointer |
| `Space` | Pause or resume the simulation |
| `Tab` | Cycle between 1x, 2x, and 4x speed |
| `L` | Reload the starter floor after confirmation |
| `N` | Clear the floor after confirmation |
| `Enter` / `Esc` | Confirm / cancel a reset |
| `Esc` | Close the application when no confirmation is open |

Machines emit onto an adjacent conveyor in the arrow direction.
Processors accept the correct input from any side.
Shipping accepts any item from any side. Ore shipments do not count toward contracts.
Each conveyor tile holds one item. Blocked outputs cause items to queue upstream.
Each processor holds four inputs and one item in production or at its output.

| Machine | Recipe | Production time |
| --- | --- | --- |
| Miner | Produce one ore | 0.75 seconds between emissions |
| Smelter | One ore to one plate | 1.1 seconds |
| Gear press | One plate to one gear | 1.5 seconds |
| Assembler | One gear to one engine | 2.0 seconds |

Rotating a machine preserves its inventory and progress.
Replacing or erasing a part scraps its contents.
The throughput counter measures all shipments over the last 60 simulated seconds.
During the first minute, the counter extrapolates the elapsed interval to one minute.

## Simulation and tests

The simulation uses a fixed 60 Hz step and does not depend on raylib or artwork.
Buildings, inventories, processors, and moving items are Vecs entities and components.
The grid stores entity handles for spatial lookup.
Cached queries drive production, transport, and display counters.
A `Working` tag records active processors.

Vecs currently has no command-buffer API despite the older examples in the main README.
The demo queues creation, destruction, and tag changes until query callbacks return.
No component pointers survive a structural change.

Build the simulation tests without any demo downloads:

```sh
cmake -S . -B temp/vexfactory-tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DBUILD_VEX_FACTORY_TESTS=ON
cmake --build temp/vexfactory-tests --target vex_factory_test
ctest --test-dir temp/vexfactory-tests --output-on-failure
```

On Windows, use `clang-cl` instead of `clang++` in this command.
The graphical build also includes the simulation tests.

Tests cover recipes, jams, full buffers, turns, shared outputs, deletion, rotation, reset, throughput, and deterministic edits.
Conservation checks account for every produced unit as an item, buffered work, a shipment, or scrap.

The graphical smoke command advances the starter by 35 simulated seconds and exits after four frames:

```sh
./temp/vexfactory-build/demos/vexfactory/vex_factory --smoke-test
./temp/vexfactory-build/demos/vexfactory/vex_factory --screenshot temp/vexfactory/preview.png
```

For a display-free Linux host, run the smoke command with `xvfb-run -a` and `LIBGL_ALWAYS_SOFTWARE=1`.
The asset-free simulation tests do not need a display server.
The `--frames N` flag also limits an interactive run to a fixed number of frames.
