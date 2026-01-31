#include "animation.h"
#include "chardata/rika-idle.h"
#include "chardata/rika-walk.h"

// Animation state
int current_anim = 0;  // 0 = idle, 1 = walk
int current_frame = 0;
int frame_timer = 0;

void initAnimation(void) {
    current_anim = 0;
    current_frame = 0;
    frame_timer = 0;
}

int getAnimFrameCount(void) {
    if (current_anim == 0) {
        return IDLE_FRAMES_COUNT;
    } else {
        return WALK_FRAMES_COUNT;
    }
}

SVECTOR* getCurrentAnimVerts(void) {
    if (current_anim == 0) {
        return idle_anim[current_frame];
    } else {
        return walk_anim[current_frame];
    }
}

void updateAnimation(void) {
    frame_timer++;
    if (frame_timer >= ANIM_SPEED) {
        frame_timer = 0;
        current_frame++;
        if (current_frame >= getAnimFrameCount()) {
            current_frame = 0;  // Loop
        }
    }
}
