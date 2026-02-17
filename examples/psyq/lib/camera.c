#include "camera.h"

// Camera state
MATRIX view_matrix;
SVECTOR camera_rotation = {0, 0, 0};
VECTOR camera_position = {0, -12800, -20800};  // Camera position in world space (moved back for h=512)

void initCamera(void) {
    camera_rotation.vx = 0;
    camera_rotation.vy = 0;
    camera_rotation.vz = 0;
    camera_position.vx = 0;
    camera_position.vy = -12800;
    camera_position.vz = -20800;
}

void updateViewMatrix(void) {
    // GTE uses inverted camera position for view transform
    VECTOR view_translation;
    view_translation.vx = -camera_position.vx;
    view_translation.vy = -camera_position.vy;
    view_translation.vz = -camera_position.vz;
    
    RotMatrix(&camera_rotation, &view_matrix);
    ApplyMatrixLV(&view_matrix, &view_translation, &view_translation);
    TransMatrix(&view_matrix, &view_translation);
}
