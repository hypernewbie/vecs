add_executable( vex_factory_test simulation_test.cpp )
target_include_directories( vex_factory_test PRIVATE "${PROJECT_SOURCE_DIR}" )
add_test( NAME vex_factory_simulation COMMAND vex_factory_test )

if ( NOT BUILD_VEX_FACTORY )
    return()
endif()

# Only the graphical demo needs C, downloaded libraries, or newer CMake.
if ( CMAKE_VERSION VERSION_LESS 3.25 )
    message( FATAL_ERROR "VexFactory with raylib 6.0 needs CMake 3.25 or newer. The asset-free simulation tests need only CMake 3.20." )
endif()
enable_language( C )
include( FetchContent )
cmake_policy( SET CMP0135 NEW )
set( GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE )
set( GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE )
set( GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE )
set( GLFW_INSTALL OFF CACHE BOOL "" FORCE )
set( GLFW_LIBRARY_TYPE STATIC CACHE STRING "" FORCE )
# Keep the existing X11 desktop backend without requiring Wayland build tools.
# Operators can explicitly enable Wayland with -DGLFW_BUILD_WAYLAND=ON.
set( GLFW_BUILD_WAYLAND OFF CACHE BOOL "Build the optional Wayland backend" )
FetchContent_Declare( glfw3
    URL https://codeload.github.com/glfw/glfw/tar.gz/refs/tags/3.5.1
    URL_HASH SHA256=5234f4f29473e9a06bc7847d8371858dd135d38466eeeaa652fdc9f8f9ff0c20
    OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable( glfw3 )

set( PLATFORM Desktop CACHE STRING "" FORCE )
set( USE_EXTERNAL_GLFW ON CACHE STRING "" FORCE )
set( BUILD_EXAMPLES OFF CACHE BOOL "" FORCE )
set( BUILD_GAMES OFF CACHE BOOL "" FORCE )
FetchContent_Declare( raylib
    URL https://codeload.github.com/raysan5/raylib/tar.gz/refs/tags/6.0
    URL_HASH SHA256=2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b
)
# raylib's find_package(glfw3) resolves to the pinned target above, not its
# older bundled GLFW or an arbitrary system installation.
FetchContent_MakeAvailable( raylib )

set( VEX_FACTORY_ASSET_DIR "${PROJECT_SOURCE_DIR}/temp/vexfactory/assets" CACHE PATH
    "Directory for the opt-in Kenney Tiny Factory asset download" )
file( TO_CMAKE_PATH "${VEX_FACTORY_ASSET_DIR}" VEX_FACTORY_ASSET_DIR )
add_custom_target( vex_factory_assets
    COMMAND "${CMAKE_COMMAND}" "-DVEX_FACTORY_ASSET_DIR=${VEX_FACTORY_ASSET_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/fetch_assets.cmake"
    COMMENT "Fetch the pinned CC0 Tiny Factory pack (explicit opt-in)"
    VERBATIM
)

add_executable( vex_factory vex_factory.cpp )
target_include_directories( vex_factory PRIVATE "${PROJECT_SOURCE_DIR}" )
target_link_libraries( vex_factory PRIVATE raylib glfw )
target_compile_definitions( vex_factory PRIVATE
    VEX_FACTORY_DEFAULT_ASSET_DIR="${VEX_FACTORY_ASSET_DIR}" )
# Do not depend on vex_factory_assets: compiling must not fetch artwork.
