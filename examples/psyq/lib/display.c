#include "display.h"
#include <libetc.h>

// Display buffers
DoubleBuffer db[2];
DoubleBuffer *cdb;
int currentBuffer = 0;

// Primitive buffer (large enough for all primitives)
char primbuff[2][131072];
char *nextpri;

// Font stream ID
int fontId = -1;

void initDisplay(void) {
    ResetGraph(0);
    
    // Buffer 0: draw at 0,0 - display from 0,240
    SetDefDrawEnv(&db[0].draw, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&db[0].disp, 0, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT);
    
    // Buffer 1: draw at 0,240 - display from 0,0
    SetDefDrawEnv(&db[1].draw, 0, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT);
    SetDefDispEnv(&db[1].disp, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    
    // Set background color
    db[0].draw.isbg = 1;
    db[1].draw.isbg = 1;
    setRGB0(&db[0].draw, 40, 60, 80);
    setRGB0(&db[1].draw, 40, 60, 80);
    
    // Initialize font for text display
    FntLoad(960, 256);
    fontId = FntOpen(16, 16, 288, 64, 0, 512);
    
    SetDispMask(1);
}

void swapBuffers(void) {
    currentBuffer = !currentBuffer;
    cdb = &db[currentBuffer];
    nextpri = primbuff[currentBuffer];
}
