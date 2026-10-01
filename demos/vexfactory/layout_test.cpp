#include "layout.h"
#include <cstdio>
#include <cstdlib>
using namespace vexfactory;
int main()
{
    int cases = 0;
    for ( float scale : { 1.0f, 1.15f, 1.3f, 1.4f } ) for ( auto size : { Box{ 0, 0, 960, 640 }, Box{ 0, 0, 1280, 720 }, Box{ 0, 0, 1440, 900 }, Box{ 0, 0, 1920, 1080 } } )
    {
        const auto l = Layout::make( size.width, size.height, scale );
        const bool valid = l.bodyText() >= 20 && l.smallText() >= 18 && l.world.width > 300 && l.world.height > 300 && l.panelBody.height > 220 &&
            l.world.x + l.world.width < l.panel.x && l.panel.x + l.panel.width < size.width && l.panel.y + l.panel.height <= l.footer.y && l.panelBody.y + l.panelBody.height < l.footer.y;
        if ( !valid ) { std::fprintf( stderr, "Invalid layout %.0fx%.0f scale %.2f\n", size.width, size.height, scale ); return EXIT_FAILURE; }
        ++cases;
    }
    std::printf( "VexFactory layout: %d native-resolution/large-text cases passed.\n", cases );
}
