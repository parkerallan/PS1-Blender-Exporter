#include "lighting.h"
#include "display.h"
#include <sys/types.h>
#include <libgte.h>

void initGTE(void) {
    MATRIX light_matrix;
    VECTOR light_direction;
    
    InitGeom();
    SetGeomOffset(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
    SetGeomScreen(SCREEN_WIDTH / 2);
    
    // Setup lighting - bright midday sun from above and slightly forward
    light_direction.vx = 0;      // No horizontal offset
    light_direction.vy = 3584;   // Mostly downward (positive Y = down)
    light_direction.vz = -1024;  // Slightly forward toward camera
    
    // Normalize and set as light matrix column 0
    SetRotMatrix(&light_matrix);
    SetTransMatrix(&light_matrix);
    
    // Set light color matrix (RGB for 3 light sources)
    // Bright white sunlight - much brighter than moonlight
    // Matrix is [row][col] where cols are lights, rows are R,G,B
    MATRIX color_matrix;
    color_matrix.m[0][0] = 4096;  // Red component of Light 0 (maximum)
    color_matrix.m[1][0] = 4096;  // Green component of Light 0 (maximum)
    color_matrix.m[2][0] = 4096;  // Blue component of Light 0 (maximum)
    
    color_matrix.m[0][1] = 0;     // Red component of Light 1 (disabled)
    color_matrix.m[1][1] = 0;     // Green component of Light 1
    color_matrix.m[2][1] = 0;     // Blue component of Light 1
    
    color_matrix.m[0][2] = 0;     // Red component of Light 2 (disabled)
    color_matrix.m[1][2] = 0;     // Green component of Light 2
    color_matrix.m[2][2] = 0;     // Blue component of Light 2
    
    SetColorMatrix(&color_matrix);
    
    // Set light direction matrix
    MATRIX light_dir_matrix;
    light_dir_matrix.m[0][0] = -light_direction.vx;
    light_dir_matrix.m[0][1] = -light_direction.vy;
    light_dir_matrix.m[0][2] = -light_direction.vz;
    
    light_dir_matrix.m[1][0] = 0;  // Light 1 direction
    light_dir_matrix.m[1][1] = 0;
    light_dir_matrix.m[1][2] = 0;
    
    light_dir_matrix.m[2][0] = 0;  // Light 2 direction
    light_dir_matrix.m[2][1] = 0;
    light_dir_matrix.m[2][2] = 0;
    
    SetLightMatrix(&light_dir_matrix);
    
    // Set ambient color (background light) - bright for sunny day
    CVECTOR ambient = {80, 80, 80, 0};  // Much brighter ambient light
    SetBackColor(ambient.r, ambient.g, ambient.b);
}
