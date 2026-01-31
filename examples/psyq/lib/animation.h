#ifndef ANIMATION_H
#define ANIMATION_H

#include <sys/types.h>
#include <libgte.h>

// Animation state
extern int current_anim;     // 0 = idle, 1 = walk
extern int current_frame;
extern int frame_timer;

#define ANIM_SPEED 2  // Update every 2 vsyncs

// Initialize animation system
void initAnimation(void);

// Get current animation frame count
int getAnimFrameCount(void);

// Get current animation vertices
SVECTOR* getCurrentAnimVerts(void);

// Update animation (call once per frame)
void updateAnimation(void);

#endif // ANIMATION_H
