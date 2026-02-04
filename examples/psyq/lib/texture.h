#ifndef TEXTURE_H
#define TEXTURE_H

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

//----------------------------------------------------------
// VRAM Slot Manager
// Manages VRAM texture budget for large environments.
// Allows loading more textures than VRAM can hold by
// binding textures to managed slots, ignoring the hardcoded
// px/py coordinates embedded in TIM headers.
//
// Use case: A level has 10 different 256x256 textures but
// only 3 can fit in VRAM at once. Load textures into slots
// as needed for the current area/objects being rendered.
//----------------------------------------------------------

// Number of available VRAM slots for managed textures
// Each slot is 64 VRAM words wide (256 pixels for 4-bit, 128 for 8-bit, 64 for 16-bit)
#define VRAM_SLOT_COUNT 6

// CLUT slots in VRAM (Y positions for color lookup tables)
#define CLUT_Y_BASE 480

// Texture slot info - tracks what's loaded where
typedef struct {
    RECT pixel_rect;        // VRAM region for pixel data
    RECT clut_rect;         // VRAM region for CLUT (if applicable)
    u_short tpage;          // Texture page ID for this slot
    u_short clut;           // CLUT ID for this slot
    int in_use;             // Flag indicating if slot is occupied
    const u_char *loaded;   // Pointer to currently loaded TIM (for tracking)
} VRAMSlot;

// VRAM slot array
extern VRAMSlot vram_slots[VRAM_SLOT_COUNT];

// Initialize VRAM slot manager with predefined regions
void initVRAMManager(void);

// Bind a TIM texture to a specific VRAM slot
// Ignores the hardcoded px/py in the TIM and uses our slot coordinates.
// Returns: tpage ID for the bound texture
// Parameters:
//   timHeader - pointer to TIM data array (e.g., wall_texture_tim)
//   slotIdx   - which VRAM slot to use (0 to VRAM_SLOT_COUNT-1)
//   outClut   - output pointer for CLUT value (can be NULL if not needed)
u_short BindTexture(const u_char *timHeader, int slotIdx, u_short *outClut);

// Check if a texture is already loaded in a slot
// Returns slot index if found, -1 if not loaded
int FindLoadedTexture(const u_char *timHeader);

// Bind texture, but skip upload if already in that slot (optimization)
u_short BindTextureIfNeeded(const u_char *timHeader, int slotIdx, u_short *outClut);

// Unbind/clear a VRAM slot (marks it as available for reuse)
void UnbindSlot(int slotIdx);

// Get tpage for a slot
u_short GetSlotTPage(int slotIdx);

// Get clut for a slot
u_short GetSlotClut(int slotIdx);

// Check if slot is in use
int IsSlotInUse(int slotIdx);

#endif // TEXTURE_H
