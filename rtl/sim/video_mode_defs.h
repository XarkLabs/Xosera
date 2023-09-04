#if defined(MODE_640x400)        //	640x400@70Hz 	(often treated as 720x400 VGA text mode)
// VGA mode 640x400 @ 70Hz (pixel clock 25.175Mhz)
const double PIXEL_CLOCK_MHZ = 25.175;        // pixel clock in MHz
const int    VISIBLE_WIDTH   = 640;           // horizontal active pixels
const int    VISIBLE_HEIGHT  = 400;           // vertical active lines
const int    H_FRONT_PORCH   = 16;            // H pre-sync (front porch) pixels
const int    H_SYNC_PULSE    = 96;            // H sync pulse pixels
const int    H_BACK_PORCH    = 48;            // H post-sync (back porch) pixels
const int    V_FRONT_PORCH   = 12;            // V pre-sync (front porch) lines
const int    V_SYNC_PULSE    = 2;             // V sync pulse lines
const int    V_BACK_PORCH    = 35;            // V post-sync (back porch) lines
const int    H_SYNC_POLARITY = 0;             // H sync pulse active level
const int    V_SYNC_POLARITY = 1;             // V sync pulse active level
#elif defined(MODE_640x400_85)                // 640x400@85Hz
// VGA mode 640x480 @ 75Hz (pixel clock 31.5Mhz)
const double PIXEL_CLOCK_MHZ = 31.500;        // pixel clock in MHz
const int    VISIBLE_WIDTH   = 640;           // horizontal active pixels
const int    VISIBLE_HEIGHT  = 400;           // vertical active lines
const int    H_FRONT_PORCH   = 32;            // H pre-sync (front porch) pixels
const int    H_SYNC_PULSE    = 64;            // H sync pulse pixels
const int    H_BACK_PORCH    = 96;            // H post-sync (back porch) pixels
const int    V_FRONT_PORCH   = 1;             // V pre-sync (front porch) lines
const int    V_SYNC_PULSE    = 3;             // V sync pulse lines
const int    V_BACK_PORCH    = 41;            // V post-sync (back porch) lines
const int    H_SYNC_POLARITY = 0;             // H sync pulse active level
const int    V_SYNC_POLARITY = 1;             // V sync pulse active level
#elif defined(MODE_640x480)                   // 640x480@60Hz	(default)
// VGA mode 640x480 @ 60Hz (pixel clock 25.175Mhz)
const double PIXEL_CLOCK_MHZ = 25.175;        // pixel clock in MHz
const int    VISIBLE_WIDTH   = 640;           // horizontal active pixels
const int    VISIBLE_HEIGHT  = 480;           // vertical active lines
const int    H_FRONT_PORCH   = 16;            // H pre-sync (front porch) pixels
const int    H_SYNC_PULSE    = 96;            // H sync pulse pixels
const int    H_BACK_PORCH    = 48;            // H post-sync (back porch) pixels
const int    V_FRONT_PORCH   = 10;            // V pre-sync (front porch) lines
const int    V_SYNC_PULSE    = 2;             // V sync pulse lines
const int    V_BACK_PORCH    = 33;            // V post-sync (back porch) lines
const int    H_SYNC_POLARITY = 0;             // H sync pulse active level
const int    V_SYNC_POLARITY = 0;             // V sync pulse active level
#elif defined(MODE_640x480_75)                // 640x480@75Hz
// VGA mode 640x480 @ 75Hz (pixel clock 31.5Mhz)
const double PIXEL_CLOCK_MHZ = 31.500;        // pixel clock in MHz
const int    VISIBLE_WIDTH   = 640;           // horizontal active pixels
const int    VISIBLE_HEIGHT  = 480;           // vertical active lines
const int    H_FRONT_PORCH   = 16;            // H pre-sync (front porch) pixels
const int    H_SYNC_PULSE    = 64;            // H sync pulse pixels
const int    H_BACK_PORCH    = 120;           // H post-sync (back porch) pixels
const int    V_FRONT_PORCH   = 1;             // V pre-sync (front porch) lines
const int    V_SYNC_PULSE    = 3;             // V sync pulse lines
const int    V_BACK_PORCH    = 16;            // V post-sync (back porch) lines
const int    H_SYNC_POLARITY = 0;             // H sync pulse active level
const int    V_SYNC_POLARITY = 0;             // V sync pulse active level
#elif defined(MODE_640x480_85)                // 640x480@85Hz
// VGA mode 640x480 @ 85Hz (pixel clock 36.000Mhz)
const double PIXEL_CLOCK_MHZ = 36.000;        // pixel clock in MHz
const int    VISIBLE_WIDTH   = 640;           // horizontal active pixels
const int    VISIBLE_HEIGHT  = 480;           // vertical active lines
const int    H_FRONT_PORCH   = 56;            // H pre-sync (front porch) pixels
const int    H_SYNC_PULSE    = 56;            // H sync pulse pixels
const int    H_BACK_PORCH    = 80;            // H post-sync (back porch) pixels
const int    V_FRONT_PORCH   = 1;             // V pre-sync (front porch) lines
const int    V_SYNC_PULSE    = 3;             // V sync pulse lines
const int    V_BACK_PORCH    = 25;            // V post-sync (back porch) lines
const int    H_SYNC_POLARITY = 0;             // H sync pulse active level
const int    V_SYNC_POLARITY = 0;             // V sync pulse active level
#elif defined(MODE_720x400)
// VGA mode 720x400 @ 70Hz (pixel clock 28.322Mhz)
const double PIXEL_CLOCK_MHZ = 28.322;        // pixel clock in MHz
const int    VISIBLE_WIDTH   = 720;           // horizontal active pixels
const int    VISIBLE_HEIGHT  = 400;           // vertical active lines
const int    H_FRONT_PORCH   = 18;            // H pre-sync (front porch) pixels
const int    H_SYNC_PULSE    = 108;           // H sync pulse pixels
const int    H_BACK_PORCH    = 54;            // H post-sync (back porch) pixels
const int    V_FRONT_PORCH   = 12;            // V pre-sync (front porch) lines
const int    V_SYNC_PULSE    = 2;             // V sync pulse lines
const int    V_BACK_PORCH    = 35;            // V post-sync (back porch) lines
const int    H_SYNC_POLARITY = 0;             // H sync pulse active level
const int    V_SYNC_POLARITY = 1;             // V sync pulse active level
#elif defined(MODE_848x480)         // 848x480@60Hz	(works well, 16:9 480p)
// VGA mode 848x480 @ 60Hz (pixel clock 33.750Mhz)
const double PIXEL_CLOCK_MHZ = 33.750;        // pixel clock in MHz
const int    VISIBLE_WIDTH   = 848;           // horizontal active pixels
const int    VISIBLE_HEIGHT  = 480;           // vertical active lines
const int    H_FRONT_PORCH   = 16;            // H pre-sync (front porch) pixels
const int    H_SYNC_PULSE    = 112;           // H sync pulse pixels
const int    H_BACK_PORCH    = 112;           // H post-sync (back porch) pixels
const int    V_FRONT_PORCH   = 6;             // V pre-sync (front porch) lines
const int    V_SYNC_PULSE    = 8;             // V sync pulse lines
const int    V_BACK_PORCH    = 23;            // V post-sync (back porch) lines
const int    H_SYNC_POLARITY = 1;             // H sync pulse active level
const int    V_SYNC_POLARITY = 1;             // V sync pulse active level
#elif defined(MODE_800x600)         //	800x600@60Hz	(out of spec for design on iCE40UP5K)
// VGA mode 800x600 @ 60Hz (pixel clock 40.000Mhz)
const double PIXEL_CLOCK_MHZ = 40.000;        // pixel clock in MHz
const int    VISIBLE_WIDTH   = 800;           // horizontal active pixels
const int    VISIBLE_HEIGHT  = 600;           // vertical active lines
const int    H_FRONT_PORCH   = 40;            // H pre-sync (front porch) pixels
const int    H_SYNC_PULSE    = 128;           // H sync pulse pixels
const int    H_BACK_PORCH    = 88;            // H post-sync (back porch) pixels
const int    V_FRONT_PORCH   = 1;             // V pre-sync (front porch) lines
const int    V_SYNC_PULSE    = 4;             // V sync pulse lines
const int    V_BACK_PORCH    = 23;            // V post-sync (back porch) lines
const int    H_SYNC_POLARITY = 1;             // H sync pulse active level
const int    V_SYNC_POLARITY = 1;             // V sync pulse active level
#elif defined(MODE_1024x768)        //	1024x768@60Hz	(out of spec for design on iCE40UP5K)
// VGA mode 1024x768 @ 60Hz (pixel clock 65.000Mhz)
const double PIXEL_CLOCK_MHZ = 65.000;        // pixel clock in MHz
const int    VISIBLE_WIDTH   = 1024;          // horizontal active pixels
const int    VISIBLE_HEIGHT  = 768;           // vertical active lines
const int    H_FRONT_PORCH   = 24;            // H pre-sync (front porch) pixels
const int    H_SYNC_PULSE    = 136;           // H sync pulse pixels
const int    H_BACK_PORCH    = 160;           // H post-sync (back porch) pixels
const int    V_FRONT_PORCH   = 3;             // V pre-sync (front porch) lines
const int    V_SYNC_PULSE    = 6;             // V sync pulse lines
const int    V_BACK_PORCH    = 29;            // V post-sync (back porch) lines
const int    H_SYNC_POLARITY = 0;             // H sync pulse active level
const int    V_SYNC_POLARITY = 0;             // V sync pulse active level
#elif defined(MODE_1280x720)        //	1280x720@60Hz	(out of spec for design on iCE40UP5K)
// VGA mode 1280x720 @ 60Hz (pixel clock 72.250Mhz)
const double PIXEL_CLOCK_MHZ = 72.250;        // pixel clock in MHz
const int    VISIBLE_WIDTH   = 1280;          // horizontal active pixels
const int    VISIBLE_HEIGHT  = 720;           // vertical active lines
const int    H_FRONT_PORCH   = 110;           // H pre-sync (front porch) pixels
const int    H_SYNC_PULSE    = 40;            // H sync pulse pixels
const int    H_BACK_PORCH    = 220;           // H post-sync (back porch) pixels
const int    V_FRONT_PORCH   = 5;             // V pre-sync (front porch) lines
const int    V_SYNC_PULSE    = 5;             // V sync pulse lines
const int    V_BACK_PORCH    = 20;            // V post-sync (back porch) lines
const int    H_SYNC_POLARITY = 1;             // H sync pulse active level
const int    V_SYNC_POLARITY = 1;             // V sync pulse active level
#else
#warning Unknown video mode (or not defined, see Makefile)
const double PIXEL_CLOCK_MHZ = 25.175;        // pixel clock in MHz
const int    VISIBLE_WIDTH   = 640;           // horizontal active pixels
const int    VISIBLE_HEIGHT  = 400;           // vertical active lines
const int    H_FRONT_PORCH   = 16;            // H pre-sync (front porch) pixels
const int    H_SYNC_PULSE    = 96;            // H sync pulse pixels
const int    H_BACK_PORCH    = 48;            // H post-sync (back porch) pixels
const int    V_FRONT_PORCH   = 12;            // V pre-sync (front porch) lines
const int    V_SYNC_PULSE    = 2;             // V sync pulse lines
const int    V_BACK_PORCH    = 35;            // V post-sync (back porch) lines
const int    H_SYNC_POLARITY = 0;             // H sync pulse active level
const int    V_SYNC_POLARITY = 1;             // V sync pulse active level
#endif

const int TOTAL_WIDTH      = H_FRONT_PORCH + H_SYNC_PULSE + H_BACK_PORCH + VISIBLE_WIDTH;
const int TOTAL_HEIGHT     = V_FRONT_PORCH + V_SYNC_PULSE + V_BACK_PORCH + VISIBLE_HEIGHT;
const int OFFSCREEN_WIDTH  = TOTAL_WIDTH - VISIBLE_WIDTH;
const int OFFSCREEN_HEIGHT = TOTAL_HEIGHT - VISIBLE_HEIGHT;
