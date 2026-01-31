#ifndef DISPLAY_H
#define DISPLAY_H

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define OT_LENGTH     4096

// Double buffer structure
typedef struct {
    DRAWENV draw;
    DISPENV disp;
    u_long  ot[OT_LENGTH];
} DoubleBuffer;

// Display buffers
extern DoubleBuffer db[2];
extern DoubleBuffer *cdb;
extern int currentBuffer;

// Primitive buffer
extern char primbuff[2][131072];
extern char *nextpri;

// Font stream ID
extern int fontId;

// Initialize display system
void initDisplay(void);

// Swap display buffers
void swapBuffers(void);

#endif // DISPLAY_H
