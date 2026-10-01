add_executable( vex_factory_test simulation_test.cpp )
target_include_directories( vex_factory_test PRIVATE "${PROJECT_SOURCE_DIR}" )
add_test( NAME vex_factory_simulation COMMAND vex_factory_test )

if ( NOT BUILD_VEX_FACTORY )
    return()
endif()

# Only the graphical demo needs C, raylib, or a network connection.
enable_language( C )
include( FetchContent )
if ( POLICY CMP0135 )
    cmake_policy( SET CMP0135 NEW )
endif()
set( PLATFORM Desktop CACHE STRING "" FORCE )
set( USE_EXTERNAL_GLFW OFF CACHE STRING "" FORCE )
set( BUILD_EXAMPLES OFF CACHE BOOL "" FORCE )
set( BUILD_GAMES OFF CACHE BOOL "" FORCE )
FetchContent_Declare( raylib
    URL https://codeload.github.com/raysan5/raylib/tar.gz/refs/tags/5.5
    URL_HASH SHA256=aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e
)
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
