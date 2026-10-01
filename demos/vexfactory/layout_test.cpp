#include "layout.h"
#include <cstdio>
#include <cstdlib>
using namespace vexfactory;
#define CHECK( condition ) do { if ( !( condition ) ) { std::fprintf( stderr, "Layout failure at line %d: %s\n", __LINE__, #condition ); return EXIT_FAILURE; } } while ( 0 )
int main()
{
    int cases = 0;
    for ( float scale : { 1.0f, 1.25f, 1.4f, 1.5f, 1.75f, 2.0f } ) for ( auto size : { Box{ 0, 0, 960, 640 }, Box{ 0, 0, 1280, 720 }, Box{ 0, 0, 1440, 900 }, Box{ 0, 0, 1920, 1080 } } )
    {
        const auto l = Layout::make( size.width, size.height, scale );
        CHECK( l.bodyText() >= 24 && l.smallText() >= 20 );
        CHECK( l.world.width >= size.width * .97f && l.world.height > 300 );
        CHECK( l.world.y >= l.header.height && l.world.y + l.world.height <= l.footer.y );
        CHECK( l.panel.x >= l.world.x && l.panel.x + l.panel.width <= l.world.x + l.world.width );
        CHECK( l.panelBody.height > 100 && l.panelBody.y + l.panelBody.height <= l.footer.y );
        CHECK( l.tileSize( 24, 16 ) >= 40 );
        if ( scale == 1 ) CHECK( l.world.width * l.world.height >= size.width * size.height * .69f );
        ++cases;
    }
    // Retina/Wayland use logical cursor coordinates. Windows/X11 use pixels.
    // All these surfaces must produce identical layout and logical hit tests.
    for ( const auto surface : {
        Surface::make( 1440, 900, 1440, 900, 1, 1 ),
        Surface::make( 1440, 900, 2880, 1800, 2, 2 ),
        Surface::make( 1440, 900, 4320, 2700, 3, 3 ),
        Surface::make( 2880, 1800, 2880, 1800, 2, 2 ),
        Surface::make( 2160, 1350, 2160, 1350, 1.5f, 1.5f ) } )
    {
        CHECK( surface.width == 1440 && surface.height == 900 );
        const auto l = Layout::make( surface.width, surface.height, 1 );
        const float zoom = l.tileSize( 24, 16 );
        const float x = l.world.x + l.world.width / 2 + ( 4.5f - 12 ) * zoom;
        const float y = l.world.y + l.world.height / 2 + ( 3.5f - 8 ) * zoom;
        const auto pixel = surface.pixels( { x, y, zoom, zoom } );
        const float nativeX = pixel.x * surface.windowWidth / surface.framebufferWidth;
        const float nativeY = pixel.y * surface.windowHeight / surface.framebufferHeight;
        CHECK( std::abs( surface.mouseX( nativeX ) - x ) < .001f && std::abs( surface.mouseY( nativeY ) - y ) < .001f );
        CHECK( std::abs( pixel.width / surface.densityX() - zoom ) < .001f );
        CHECK( static_cast<int>( ( surface.mouseX( nativeX ) - l.world.x - l.world.width / 2 ) / zoom + 12 ) == 4 );
        CHECK( static_cast<int>( ( surface.mouseY( nativeY ) - l.world.y - l.world.height / 2 ) / zoom + 8 ) == 3 );
        ++cases;
    }
    CHECK( Layout::make( 1440, 900, 9 ).scale == MaxUiScale );
    std::printf( "VexFactory layout: %d minimal-HUD and DPI/input cases passed.\n", cases );
}
