cmake_minimum_required( VERSION 3.20 )
project( vecs_test LANGUAGES CXX )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

if ( WIN32 )
    add_compile_options( /W4 )
elseif ( APPLE )
    add_compile_options( -Wall -Wextra -Wpedantic -stdlib=libc++ )
    add_link_options( -stdlib=libc++ )
else()
    add_compile_options( -Wall -Wextra -Wpedantic -stdlib=libc++ )
    add_link_options( -stdlib=libc++ )
endif()

add_executable( vecs_test vecs_test.cpp )
target_include_directories( vecs_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} )
