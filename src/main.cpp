#include <stdexcept>

// make sure that one token is defined
#if !defined( MANDELBROT_MODE ) && !defined( PATHTRACER_MODE )
    #define PATHTRACER_MODE
#endif

#if defined( MANDELBROT_MODE )
    #include "mandelbrotApp.h"
#elif defined( PATHTRACER_MODE ) 
    #include "pathtracerApp.h"
#endif


int main( int argc, char* argv[] ) {

    printf( "starting main!\n" );
    
#if defined( MANDELBROT_MODE )
    MandelbrotApp app = MandelbrotApp( 2000, 2000 ); // dimensions must match shader
#elif defined( PATHTRACER_MODE )
    const int32_t spp = argc>1 ? atoi(argv[1]) : 500;    // samples per pixel 
    const uint32_t resy = argc>2 ? static_cast<uint32_t>( atoi(argv[2]) ) : 600;    // vertical pixel resolution
    const uint32_t resx = resy*3/2;	                    // horiziontal pixel resolution
    PathtracerApp app = PathtracerApp( resx, resy, spp );
#endif

    app.init();
    app.preRun();
    printf( "now running app!\n" );
    try {
        app.run();
        app.saveRenderedImage();
    }
    catch (const std::runtime_error& e) {
        printf("%s\n", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
