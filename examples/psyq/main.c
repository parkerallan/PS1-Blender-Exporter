/*
 * PlayStation 1 Model Viewer Example
 * Renders the Rika model with idle/walk animation switching
 * Press X to switch between animations
 */

#include <stdlib.h>
#include <libgte.h>
#include <sys/types.h>
#include <libetc.h>
#include <libgpu.h>

// Include model and animations
#include "rika.h"
#include "rika-idle.h"
#include "rika-walk.h"

// Include texture
#include "rikatexture.h"

// Display settings
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define OT_LENGTH     4096

// Triangle and quad counts from rika.h
#define TRI_COUNT  200
#define QUAD_COUNT 264

// Double buffer structure
typedef struct {
    DRAWENV draw;
    DISPENV disp;
    u_long  ot[OT_LENGTH];
} DoubleBuffer;

DoubleBuffer db[2];
DoubleBuffer *cdb;
int currentBuffer = 0;

// Primitive buffer (large enough for all primitives)
char primbuff[2][131072];
char *nextpri;

// Controller
u_long padState = 0;
u_long padStateOld = 0;

// Texture info
TIM_IMAGE tim;
u_short tpage = 0;
u_short clut = 0;

// Transform
MATRIX world_matrix;
SVECTOR rotation = {0, 0, 0};
VECTOR translation = {0, 4000, 4000};
VECTOR scale = {ONE, ONE, ONE};

// Animation state
int current_anim = 0;  // 0 = idle, 1 = walk
int current_frame = 0;
int frame_timer = 0;
#define ANIM_SPEED 2  // Update every 2 vsyncs

// Font stream ID
int fontId = -1;

// Get current animation frame count
int getAnimFrameCount(void) {
    if (current_anim == 0) {
        return IDLE_FRAMES_COUNT;
    } else {
        return WALK_FRAMES_COUNT;
    }
}

// Get current animation vertices
SVECTOR* getCurrentAnimVerts(void) {
    if (current_anim == 0) {
        return idle_anim[current_frame];
    } else {
        return walk_anim[current_frame];
    }
}

//----------------------------------------------------------
// Initialize display
//----------------------------------------------------------
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

//----------------------------------------------------------
// Initialize GTE (Geometry Transformation Engine)
//----------------------------------------------------------
void initGTE(void) {
    InitGeom();
    SetGeomOffset(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
    SetGeomScreen(SCREEN_WIDTH / 2);
}

//----------------------------------------------------------
// Initialize controller
//----------------------------------------------------------
void initController(void) {
    PadInit(0);  // Initialize controller system
    padState = 0;
    padStateOld = 0;
}

//----------------------------------------------------------
// Load texture into VRAM
//----------------------------------------------------------
void loadTexture(void) {
    OpenTIM((u_long*)rikatexture_tim);
    ReadTIM(&tim);
    
    LoadImage(tim.prect, tim.paddr);
    DrawSync(0);
    
    if (tim.mode & 0x8) {
        LoadImage(tim.crect, tim.caddr);
        DrawSync(0);
        clut = getClut(tim.crect->x, tim.crect->y);
    }
    
    tpage = getTPage(tim.mode & 0x3, 0, tim.prect->x, tim.prect->y);
}

//----------------------------------------------------------
// Update animation
//----------------------------------------------------------
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

//----------------------------------------------------------
// Handle input
//----------------------------------------------------------
void handleInput(void) {
    padStateOld = padState;
    padState = PadRead(0);  // Read pad 0 (first controller)
    
    // Triangle button to switch animation
    if ((padState & PADRup) && !(padStateOld & PADRup)) {
        current_anim = !current_anim;
        current_frame = 0;
        frame_timer = 0;
    }
    
    // D-pad left/right for Y rotation
    if (padState & PADLleft) {
        rotation.vy -= 32;
    }
    if (padState & PADLright) {
        rotation.vy += 32;
    }
    
    // D-pad up/down for X rotation
    if (padState & PADLup) {
        rotation.vx -= 32;
    }
    if (padState & PADLdown) {
        rotation.vx += 32;
    }
    
    // L1/R1 for zoom
    if (padState & PADL1) {
        translation.vz += 50;
    }
    if (padState & PADR1) {
        translation.vz -= 50;
    }

    // L2/R2 for height
    if (padState & PADL2) {
        translation.vy += 50;
    }
    if (padState & PADR2) {
        translation.vy -= 50;
    }
}

//----------------------------------------------------------
// Render triangles
//----------------------------------------------------------
void renderTriangles(SVECTOR *verts) {
    int i;
    long otz, p, flg;
    POLY_FT3 *poly;
    
    for (i = 0; i < TRI_COUNT; i++) {
        poly = (POLY_FT3 *)nextpri;
        setPolyFT3(poly);
        
        int v0 = tri_faces[i][0];
        int v1 = tri_faces[i][1];
        int v2 = tri_faces[i][2];
        
        otz = RotAverage3(
            &verts[v0], &verts[v1], &verts[v2],
            (long*)&poly->x0, (long*)&poly->x1, (long*)&poly->x2,
            &p, &flg
        );
        
        if (otz > 0 && otz < OT_LENGTH) {
            // Set UVs
            int uv0 = tri_uvs[i][0];
            int uv1 = tri_uvs[i][1];
            int uv2 = tri_uvs[i][2];
            
            setUV3(poly,
                uvs[uv0].vx, uvs[uv0].vy,
                uvs[uv1].vx, uvs[uv1].vy,
                uvs[uv2].vx, uvs[uv2].vy
            );
            
            poly->tpage = tpage;
            poly->clut = clut;
            setRGB0(poly, 128, 128, 128);
            
            addPrim(&cdb->ot[otz], poly);
            nextpri += sizeof(POLY_FT3);
        }
    }
}

//----------------------------------------------------------
// Render quads
//----------------------------------------------------------
void renderQuads(SVECTOR *verts) {
    int i;
    long otz, p, flg;
    POLY_FT4 *poly;
    
    for (i = 0; i < QUAD_COUNT; i++) {
        poly = (POLY_FT4 *)nextpri;
        setPolyFT4(poly);
        
        int v0 = quad_faces[i][0];
        int v1 = quad_faces[i][1];
        int v2 = quad_faces[i][2];
        int v3 = quad_faces[i][3];
        
        otz = RotAverage4(
            &verts[v0], &verts[v1], &verts[v2], &verts[v3],
            (long*)&poly->x0, (long*)&poly->x1,
            (long*)&poly->x2, (long*)&poly->x3,
            &p, &flg
        );
        
        if (otz > 0 && otz < OT_LENGTH) {
            // Set UVs
            int uv0 = quad_uvs[i][0];
            int uv1 = quad_uvs[i][1];
            int uv2 = quad_uvs[i][2];
            int uv3 = quad_uvs[i][3];
            
            setUV4(poly,
                uvs[uv0].vx, uvs[uv0].vy,
                uvs[uv1].vx, uvs[uv1].vy,
                uvs[uv2].vx, uvs[uv2].vy,
                uvs[uv3].vx, uvs[uv3].vy
            );
            
            poly->tpage = tpage;
            poly->clut = clut;
            setRGB0(poly, 128, 128, 128);
            
            addPrim(&cdb->ot[otz], poly);
            nextpri += sizeof(POLY_FT4);
        }
    }
}

//----------------------------------------------------------
// Render the model
//----------------------------------------------------------
void renderModel(SVECTOR *verts) {
    // Build transformation matrix
    RotMatrix(&rotation, &world_matrix);
    TransMatrix(&world_matrix, &translation);
    ScaleMatrix(&world_matrix, &scale);
    
    // Set as current GTE matrix
    SetRotMatrix(&world_matrix);
    SetTransMatrix(&world_matrix);
    
    // Render all primitives
    renderTriangles(verts);
    renderQuads(verts);
}

//----------------------------------------------------------
// Main function
//----------------------------------------------------------
int main(void) {
    // Initialize everything
    initDisplay();
    initGTE();
    initController();
    loadTexture();
    
    // Main loop
    while (1) {
        // Handle input
        handleInput();
        
        // Update animation
        updateAnimation();
        
        // Swap double buffer
        currentBuffer = !currentBuffer;
        cdb = &db[currentBuffer];
        
        // Clear ordering table
        ClearOTagR(cdb->ot, OT_LENGTH);
        
        // Reset primitive buffer
        nextpri = primbuff[currentBuffer];
        
        // Display animation state
        FntPrint(fontId, "Animation: %s\n", current_anim == 0 ? "IDLE" : "WALK");
        FntPrint(fontId, "Frame: %d/%d\n", current_frame, getAnimFrameCount());
        FntFlush(fontId);
        
        // Render model with current animation frame
        renderModel(getCurrentAnimVerts());
        
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
