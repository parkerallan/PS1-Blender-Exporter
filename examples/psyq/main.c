/*
 * PlayStation 1 Model Viewer Example
 * Renders the Rika model with idle/walk animation switching
 */

#include <stdlib.h>
#include <libgte.h>
#include <sys/types.h>
#include <libetc.h>
#include <libgpu.h>

// Include subsystems
#include "lib/display.h"
#include "lib/lighting.h"
#include "lib/input.h"
#include "lib/texture.h"
#include "lib/camera.h"
#include "lib/animation.h"
#include "lib/sound.h"

// Include model and animations
#include "chardata/rika.h"
#include "chardata/ground.h"
#include "chardata/moon.h"
#include "chardata/coin.h"
#include "chardata/coin-spin.h"
#include "chardata/star.h"

// Include texture data for VRAM manager
#include "chardata/rikatexture.h"
#include "chardata/moontexture.h"
#include "chardata/startexture.h"
#include "chardata/cointexture.h"

// Include model rendering
#include "lib/model.h"

// Model data for renderer
ModelData rika_model;
ModelData ground_model;
ModelData moon_model;
ModelData coin_model;
ModelData star_model;

// Coin animation state
int coin_frame = 0;

//----------------------------------------------------------
// Texture Slots - each texture gets its own slot
//----------------------------------------------------------
#define SLOT_0  0
#define SLOT_1  1
#define SLOT_2  2
#define SLOT_3  3
#define SLOT_4  4
#define SLOT_5  5

//----------------------------------------------------------
// Initialize model data structures
//----------------------------------------------------------
void initModels(void) {
    // Setup model data structure
    rika_model.tri_count = RIKA_TRI_COUNT;
    rika_model.quad_count = RIKA_QUAD_COUNT;
    rika_model.tri_faces = rika_tri_faces;
    rika_model.tri_uvs = rika_tri_uvs;
    rika_model.quad_faces = rika_quad_faces;
    rika_model.quad_uvs = rika_quad_uvs;
    rika_model.uvs = rika_uvs;
    rika_model.normals = rika_normals;
    rika_model.material_flags = rika_material_flags;
    rika_model.vertex_colors = rika_vertex_colors;
    rika_model.specular = NULL;  // Optional: set if exported with specular
    rika_model.metallic = NULL;  // Optional: set if exported with metallic
    rika_model.mesh_ids = rika_mesh_ids;
    rika_model.visible_meshes = 0xFFFFFFFF;  // All meshes visible by default
    
    // Setup ground model data structure
    ground_model.tri_count = GROUND_TRI_COUNT;
    ground_model.quad_count = GROUND_QUAD_COUNT;
    ground_model.tri_faces = ground_tri_faces;
    ground_model.tri_uvs = ground_tri_uvs;
    ground_model.quad_faces = ground_quad_faces;
    ground_model.quad_uvs = ground_quad_uvs;
    ground_model.uvs = ground_uvs;
    ground_model.normals = ground_normals;
    ground_model.material_flags = ground_material_flags;
    ground_model.vertex_colors = ground_vertex_colors;
    ground_model.specular = NULL;  // Optional: set if exported with specular
    ground_model.metallic = NULL;  // Optional: set if exported with metallic
    ground_model.mesh_ids = NULL;  // TODO: Re-export ground model
    ground_model.visible_meshes = 0xFFFFFFFF;  // All meshes visible by default
    
    // Setup moon model data structure
    moon_model.tri_count = MOON_TRI_COUNT;
    moon_model.quad_count = MOON_QUAD_COUNT;
    moon_model.tri_faces = moon_tri_faces;
    moon_model.tri_uvs = moon_tri_uvs;
    moon_model.quad_faces = moon_quad_faces;
    moon_model.quad_uvs = moon_quad_uvs;
    moon_model.uvs = moon_uvs;
    moon_model.normals = moon_normals;
    moon_model.material_flags = moon_material_flags;
    moon_model.vertex_colors = moon_vertex_colors;
    moon_model.specular = NULL;  // Optional: set if exported with specular
    moon_model.metallic = NULL;  // Optional: set if exported with metallic
    moon_model.mesh_ids = NULL;  // TODO: Re-export moon model
    moon_model.visible_meshes = 0xFFFFFFFF;  // All meshes visible by default
    
    // Setup coin model data structure (with metallic)
    coin_model.tri_count = COIN_TRI_COUNT;
    coin_model.quad_count = COIN_QUAD_COUNT;
    coin_model.tri_faces = coin_tri_faces;
    coin_model.tri_uvs = coin_tri_uvs;
    coin_model.quad_faces = coin_quad_faces;
    coin_model.quad_uvs = coin_quad_uvs;
    coin_model.uvs = coin_uvs;
    coin_model.normals = coin_normals;
    coin_model.material_flags = coin_material_flags;
    coin_model.vertex_colors = coin_vertex_colors;
    coin_model.specular = NULL;
    coin_model.metallic = coin_metallic;  // Coin uses metallic
    coin_model.mesh_ids = NULL;  // TODO: Re-export coin model
    coin_model.visible_meshes = 0xFFFFFFFF;  // All meshes visible by default
    
    // Setup star model data structure (with specular)
    star_model.tri_count = STAR_TRI_COUNT;
    star_model.quad_count = STAR_QUAD_COUNT;
    star_model.tri_faces = star_tri_faces;
    star_model.tri_uvs = star_tri_uvs;
    star_model.quad_faces = star_quad_faces;
    star_model.quad_uvs = star_quad_uvs;
    star_model.uvs = star_uvs;
    star_model.normals = star_normals;
    star_model.material_flags = star_material_flags;
    star_model.vertex_colors = star_vertex_colors;
    star_model.specular = star_specular;  // Star uses specular
    star_model.metallic = NULL;
    star_model.mesh_ids = NULL;  // TODO: Re-export star model
    star_model.visible_meshes = 0xFFFFFFFF;  // All meshes visible by default
}

//----------------------------------------------------------
// Load All Textures via VRAM Manager
//----------------------------------------------------------
void loadAllTextures(void) {
    BindTexture(rikatexture_tim, SLOT_0, NULL);
    BindTexture(moontexture_tim, SLOT_1, NULL);
    BindTexture(startexture_tim, SLOT_2, NULL);
    BindTexture(cointexture_tim, SLOT_3, NULL);
    DrawSync(0);
}

//----------------------------------------------------------
// Render all models
//----------------------------------------------------------
void renderScene(void) {
    // Update and set view matrix
    updateViewMatrix();
    SetRotMatrix(&view_matrix);
    SetTransMatrix(&view_matrix);
    
    // Render rika model at origin with view matrix
    nextpri = renderModel(getCurrentAnimVerts(), &rika_model, nextpri, cdb->ot, OT_LENGTH, 
                          GetSlotTPage(SLOT_0), GetSlotClut(SLOT_0));
    
    // Render ground plane positioned below rika
    MATRIX ground_world_matrix;
    SVECTOR ground_rot = {0, 0, 0};
    VECTOR ground_pos = {0, 0, 0};
    
    RotMatrix(&ground_rot, &ground_world_matrix);
    TransMatrix(&ground_world_matrix, &ground_pos);
    
    // Compose with view matrix: result = view_matrix * ground_world_matrix
    MATRIX ground_view_matrix;
    CompMatrix(&view_matrix, &ground_world_matrix, &ground_view_matrix);
    
    SetRotMatrix(&ground_view_matrix);
    SetTransMatrix(&ground_view_matrix);
    
    nextpri = renderModel(ground_vertices, &ground_model, nextpri, cdb->ot, OT_LENGTH, 
                          GetSlotTPage(SLOT_0), GetSlotClut(SLOT_0));
    
    // Render moon plane positioned above rika
    // Uses VRAM manager slot - texture loaded via BindTexture at init
    MATRIX moon_world_matrix;
    SVECTOR moon_rot = {0, 0, 0};
    VECTOR moon_pos = {0, 0, 0};  // Position moon in the sky
    
    RotMatrix(&moon_rot, &moon_world_matrix);
    TransMatrix(&moon_world_matrix, &moon_pos);
    
    // Compose with view matrix: result = view_matrix * moon_world_matrix
    MATRIX moon_view_matrix;
    CompMatrix(&view_matrix, &moon_world_matrix, &moon_view_matrix);
    
    SetRotMatrix(&moon_view_matrix);
    SetTransMatrix(&moon_view_matrix);
    
    // Get tpage/clut from VRAM manager slot
    nextpri = renderModel(moon_vertices, &moon_model, nextpri, cdb->ot, OT_LENGTH, 
                          GetSlotTPage(SLOT_1), GetSlotClut(SLOT_1));
    
    // Render coin with spin animation to the left of rika
    MATRIX coin_world_matrix;
    SVECTOR coin_rot = {0, 0, 0};
    VECTOR coin_pos = {-3000, -2000, 0};  // Left of rika
    
    RotMatrix(&coin_rot, &coin_world_matrix);
    TransMatrix(&coin_world_matrix, &coin_pos);
    
    MATRIX coin_view_matrix;
    CompMatrix(&view_matrix, &coin_world_matrix, &coin_view_matrix);
    
    SetRotMatrix(&coin_view_matrix);
    SetTransMatrix(&coin_view_matrix);
    
    // Get current coin animation frame - use VRAM slot for texture
    SVECTOR *coin_verts = spin_anim[coin_frame];
    nextpri = renderModel(coin_verts, &coin_model, nextpri, cdb->ot, OT_LENGTH, 
                          GetSlotTPage(SLOT_3), GetSlotClut(SLOT_3));
    
    // Update coin animation frame
    coin_frame = (coin_frame + 1) % SPIN_FRAMES_COUNT;
    
    // Render star to the right of rika - use VRAM slot for texture
    MATRIX star_world_matrix;
    SVECTOR star_rot = {0, 0, 0};
    VECTOR star_pos = {3000, -2000, 0};  // Right of rika
    
    RotMatrix(&star_rot, &star_world_matrix);
    TransMatrix(&star_world_matrix, &star_pos);
    
    MATRIX star_view_matrix;
    CompMatrix(&view_matrix, &star_world_matrix, &star_view_matrix);
    
    SetRotMatrix(&star_view_matrix);
    SetTransMatrix(&star_view_matrix);
    
    nextpri = renderModel(star_vertices, &star_model, nextpri, cdb->ot, OT_LENGTH, 
                          GetSlotTPage(SLOT_2), GetSlotClut(SLOT_2));
}

//----------------------------------------------------------
// Main function
//----------------------------------------------------------
int main(void) {
    // Initialize all systems
    initDisplay();
    initGTE();
    initController();
    initCamera();
    initAnimation();
    
    // Initialize VRAM manager and load all textures
    initVRAMManager();
    loadAllTextures();
    
    // Initialize sound system
    initSound();
    // Play track 2 (lastecho.wav)
    playCDTrackLoop(2);
    // Set volume
    setCDVolume(10);

    initModels();
    
    // Main loop
    while (1) {
        // Handle input
        handleInput();
        
        // Circle button to toggle head visibility (mesh 3)
        if ((padState & PADRright) && !(padStateOld & PADRright)) {
            rika_model.visible_meshes ^= (1 << 3);  // Toggle bit 3 (Head)
        }
        
        // Update animation
        updateAnimation();
        
        // Swap double buffer
        swapBuffers();
        
        // Clear ordering table
        ClearOTagR(cdb->ot, OT_LENGTH);
        
        // Display animation state
        FntPrint(fontId, "Animation: %s\n", current_anim == 0 ? "IDLE" : "WALK");
        FntPrint(fontId, "Frame: %d/%d\n", current_frame, getAnimFrameCount());
        FntPrint(fontId, "Camera: X=%d Y=%d Z=%d\n", camera_position.vx, camera_position.vy, camera_position.vz);
        FntPrint(fontId, "VRAM Slots: 4/%d in use\n", VRAM_SLOT_COUNT);
        FntPrint(fontId, "Head: %s (Circle to toggle)\n", (rika_model.visible_meshes & (1 << 3)) ? "ON" : "OFF");
        FntFlush(fontId);
        
        // Render scene
        renderScene();
        
        // Wait for GPU and VSync
        DrawSync(0);
        VSync(0);
        
        // Set draw/display environments
        PutDrawEnv(&cdb->draw);
        PutDispEnv(&cdb->disp);
        
        // Draw the ordering table
        DrawOTag(&cdb->ot[OT_LENGTH - 1]);
    }
    
    return 0;
}
