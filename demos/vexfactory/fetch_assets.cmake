cmake_minimum_required( VERSION 3.20 )

if ( NOT DEFINED VEX_FACTORY_ASSET_DIR OR VEX_FACTORY_ASSET_DIR STREQUAL "" )
    get_filename_component( VEX_FACTORY_ASSET_DIR
        "${CMAKE_CURRENT_LIST_DIR}/../../temp/vexfactory/assets" ABSOLUTE )
endif()
file( MAKE_DIRECTORY "${VEX_FACTORY_ASSET_DIR}" )

function( fetch_pack archive url checksum label )
    set( cached_hash "" )
    if ( EXISTS "${archive}" )
        file( SHA256 "${archive}" cached_hash )
    endif()
    if ( NOT cached_hash STREQUAL checksum )
        message( STATUS "Download ${label}" )
        file( DOWNLOAD "${url}" "${archive}.part"
            TLS_VERIFY ON TIMEOUT 60 INACTIVITY_TIMEOUT 20 STATUS download_status )
        list( GET download_status 0 download_code )
        list( GET download_status 1 download_message )
        if ( NOT download_code EQUAL 0 )
            file( REMOVE "${archive}.part" )
            message( FATAL_ERROR "Asset download failed: ${download_message}. Run this command again to retry." )
        endif()
        file( SHA256 "${archive}.part" downloaded_hash )
        if ( NOT downloaded_hash STREQUAL checksum )
            file( REMOVE "${archive}.part" )
            message( FATAL_ERROR "Asset checksum mismatch. No files were extracted from this pack." )
        endif()
        file( RENAME "${archive}.part" "${archive}" )
    endif()
endfunction()

# Kenney Tiny Factory 1.0, CC0. Preserve its original License.txt.
set( artwork "${VEX_FACTORY_ASSET_DIR}/kenney_tiny-factory-1.0.zip" )
fetch_pack( "${artwork}"
    "https://kenney.nl/media/pages/assets/tiny-factory/1652277319-1788860879/kenney_tiny-factory.zip"
    "eaec4169aa4bf3ea7cec4be2b06147b1a02e54bdbe61d49d4568df7e57b0db2a"
    "Kenney Tiny Factory 1.0 (CC0, about 90 KB)" )
file( ARCHIVE_EXTRACT INPUT "${artwork}" DESTINATION "${VEX_FACTORY_ASSET_DIR}" )

# Kenney Fonts, CC0. Extract only the two pixel faces, with a separate license.
set( fonts "${VEX_FACTORY_ASSET_DIR}/kenney_fonts.zip" )
fetch_pack( "${fonts}"
    "https://kenney.nl/media/pages/assets/kenney-fonts/8d5435c213-1677661710/kenney_kenney-fonts.zip"
    "4e69a86eef3cd47e9d8207413868cd08bcddeb2dae4047dbd10362e2a7a16bac"
    "Kenney Fonts (CC0, about 58 KB)" )
file( ARCHIVE_EXTRACT INPUT "${fonts}" DESTINATION "${VEX_FACTORY_ASSET_DIR}"
    PATTERNS "Fonts/Kenney Pixel.ttf" "Fonts/Kenney Pixel Square.ttf" )
file( ARCHIVE_EXTRACT INPUT "${fonts}" DESTINATION "${VEX_FACTORY_ASSET_DIR}/Fonts" PATTERNS "License.txt" )
foreach( required Tilemap/tilemap_packed.png License.txt "Fonts/Kenney Pixel.ttf" "Fonts/Kenney Pixel Square.ttf" Fonts/License.txt )
    if ( NOT EXISTS "${VEX_FACTORY_ASSET_DIR}/${required}" )
        message( FATAL_ERROR "The asset packs do not contain the expected file: ${required}" )
    endif()
endforeach()
message( STATUS "VexFactory artwork and pixel fonts ready: ${VEX_FACTORY_ASSET_DIR}" )
