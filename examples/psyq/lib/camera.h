#ifndef CAMERA_H
#define CAMERA_H

#include <sys/types.h>
#include <libgte.h>

// Camera state
extern MATRIX view_matrix;
extern SVECTOR camera_rotation;
extern VECTOR camera_position;

// Initialize camera
void initCamera(void);

// Update view matrix from camera position/rotation
void updateViewMatrix(void);

#endif // CAMERA_H
