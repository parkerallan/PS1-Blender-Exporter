#include "input.h"
#include "camera.h"
#include "animation.h"

// Controller state
u_long padState = 0;
u_long padStateOld = 0;

void initController(void) {
    PadInit(0);  // Initialize controller system
    padState = 0;
    padStateOld = 0;
}

void handleInput(void) {
    padStateOld = padState;
    padState = PadRead(0);  // Read pad 0 (first controller)
    
    // Triangle button to switch animation
    if ((padState & PADRup) && !(padStateOld & PADRup)) {
        current_anim = !current_anim;
        current_frame = 0;
        frame_timer = 0;
    }
    
    // D-pad left/right to orbit camera
    if (padState & PADLleft) {
        camera_rotation.vy += 32;
    }
    if (padState & PADLright) {
        camera_rotation.vy -= 32;
    }
    
    // D-pad up/down to tilt camera
    if (padState & PADLup) {
        camera_rotation.vx += 32;
    }
    if (padState & PADLdown) {
        camera_rotation.vx -= 32;
    }
    
    // L1/R1 to move camera closer/farther
    if (padState & PADL1) {
        camera_position.vz += 50;
    }
    if (padState & PADR1) {
        camera_position.vz -= 50;
    }

    // L2/R2 to raise/lower camera
    if (padState & PADL2) {
        camera_position.vy -= 50;
    }
    if (padState & PADR2) {
        camera_position.vy += 50;
    }
}
