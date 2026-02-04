#include "texture.h"
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

//----------------------------------------------------------
// VRAM Slot Manager Implementation
// Manages texture budget for large environments with more
// textures than can fit in VRAM simultaneously.
//----------------------------------------------------------

// VRAM slots array - 6 slots for environment textures
// Positioned in the texture page area (X >= 640 is safe for textures)
// Layout assumes 16-bit textures (64 VRAM words = 64 pixels wide)
// For 256x256 textures in 16-bit mode, each needs 256 pixels = 4 texture pages wide

// VRAM (1024 x 512) 16 bit layout (no CLUT)
// ┌──────────┬───────────┬───────────┬───────────┐
// │ FB 0     │ Slot 0    │ Slot 1    │ Slot 2    │ Y=0
// │ (0,0)    │ (320,0)   │ (576,0)   │ (832,0)   │
// │ 320x240  │ 256x256   │ 256x256   │ 192x256   │
// ├──────────┼───────────┼───────────┼───────────┤ Y=240/256
// │ FB 1     │ Slot 3    │ Slot 4    │ Slot 5    │
// │ (0,240)  │ (320,256) │ (576,256) │ (832,256) │
// │ 320x240  │ 256x256   │ 256x256   │ 192x256   │
// └──────────────────────────────────────────────┘ Y=512

VRAMSlot vram_slots[VRAM_SLOT_COUNT];

void initVRAMManager(void) {
    int i;
    
    // Clear all slots first
    for (i = 0; i < VRAM_SLOT_COUNT; i++) {
        vram_slots[i].tpage = 0;
        vram_slots[i].clut = 0;
        vram_slots[i].in_use = 0;
        vram_slots[i].loaded = NULL;
    }
    
    // Slot 0: 320,0 (256x256)
    vram_slots[0].pixel_rect.x = 320;
    vram_slots[0].pixel_rect.y = 0;
    vram_slots[0].pixel_rect.w = 256;
    vram_slots[0].pixel_rect.h = 256;
    vram_slots[0].clut_rect.x = 0;
    vram_slots[0].clut_rect.y = CLUT_Y_BASE;
    vram_slots[0].clut_rect.w = 256;
    vram_slots[0].clut_rect.h = 1;
    
    // Slot 1: 576,0 (256x256)
    vram_slots[1].pixel_rect.x = 576;
    vram_slots[1].pixel_rect.y = 0;
    vram_slots[1].pixel_rect.w = 256;
    vram_slots[1].pixel_rect.h = 256;
    vram_slots[1].clut_rect.x = 0;
    vram_slots[1].clut_rect.y = CLUT_Y_BASE + 1;
    vram_slots[1].clut_rect.w = 256;
    vram_slots[1].clut_rect.h = 1;
    
    // Slot 2: 832,0 (192x256 - limited to edge of VRAM)
    vram_slots[2].pixel_rect.x = 832;
    vram_slots[2].pixel_rect.y = 0;
    vram_slots[2].pixel_rect.w = 192;
    vram_slots[2].pixel_rect.h = 256;
    vram_slots[2].clut_rect.x = 0;
    vram_slots[2].clut_rect.y = CLUT_Y_BASE + 2;
    vram_slots[2].clut_rect.w = 256;
    vram_slots[2].clut_rect.h = 1;
    
    // Slot 3: 320,256 (256x256)
    vram_slots[3].pixel_rect.x = 320;
    vram_slots[3].pixel_rect.y = 256;
    vram_slots[3].pixel_rect.w = 256;
    vram_slots[3].pixel_rect.h = 256;
    vram_slots[3].clut_rect.x = 0;
    vram_slots[3].clut_rect.y = CLUT_Y_BASE + 3;
    vram_slots[3].clut_rect.w = 256;
    vram_slots[3].clut_rect.h = 1;
    
    // Slot 4: 576,256 (256x256)
    vram_slots[4].pixel_rect.x = 576;
    vram_slots[4].pixel_rect.y = 256;
    vram_slots[4].pixel_rect.w = 256;
    vram_slots[4].pixel_rect.h = 256;
    vram_slots[4].clut_rect.x = 0;
    vram_slots[4].clut_rect.y = CLUT_Y_BASE + 4;
    vram_slots[4].clut_rect.w = 256;
    vram_slots[4].clut_rect.h = 1;
    
    // Slot 5: 832,256 (192x256)
    vram_slots[5].pixel_rect.x = 832;
    vram_slots[5].pixel_rect.y = 256;
    vram_slots[5].pixel_rect.w = 192;
    vram_slots[5].pixel_rect.h = 256;
    vram_slots[5].clut_rect.x = 0;
    vram_slots[5].clut_rect.y = CLUT_Y_BASE + 5;
    vram_slots[5].clut_rect.w = 256;
    vram_slots[5].clut_rect.h = 1;
}

// Bind a TIM texture to a specific VRAM slot
// This function IGNORES the hardcoded px/py in the TIM header
// and uses our predefined slot coordinates instead.
u_short BindTexture(const u_char *timHeader, int slotIdx, u_short *outClut) {
    GsIMAGE timData;
    RECT pixel_rect;
    RECT clut_rect;
    VRAMSlot *slot;
    
    // Validate slot index
    if (slotIdx < 0 || slotIdx >= VRAM_SLOT_COUNT) {
        return 0;
    }
    
    slot = &vram_slots[slotIdx];
    
    // Parse the TIM header using GsGetTimInfo
    // Note: TIM format has 4-byte magic + 4-byte flags, then the actual image data
    // We skip the first 4 bytes (TIM magic 0x10) to get to the data section
    GsGetTimInfo((u_long *)(timHeader + 4), &timData);
    
    // CRUCIAL: Override the hardcoded px/py with our slot coordinates
    // This allows loading textures into managed VRAM regions regardless
    // of what coordinates were set during PNG-to-TIM conversion.
    
    // Build pixel data RECT using OUR slot coordinates, not the TIM's
    pixel_rect.x = slot->pixel_rect.x;
    pixel_rect.y = slot->pixel_rect.y;
    pixel_rect.w = timData.pw;  // Use actual texture width from TIM
    pixel_rect.h = timData.ph;  // Use actual texture height from TIM
    
    // Load pixel data to our slot location using LoadImage
    LoadImage(&pixel_rect, (u_long *)timData.pixel);
    DrawSync(0);
    
    // Calculate tpage using OUR slot coordinates
    // getTPage(pmode, abr, x, y) - pmode from TIM, coords from our slot
    slot->tpage = getTPage(timData.pmode, 0, slot->pixel_rect.x, slot->pixel_rect.y);
    
    // Handle CLUT if present (4-bit or 8-bit textures)
    if (timData.pmode < 2) {  // 4-bit (pmode=0) or 8-bit (pmode=1) have CLUTs
        // Build CLUT RECT using our slot's CLUT coordinates
        clut_rect.x = slot->clut_rect.x;
        clut_rect.y = slot->clut_rect.y;
        clut_rect.w = (timData.pmode == 0) ? 16 : 256;  // 4-bit=16 colors, 8-bit=256 colors
        clut_rect.h = 1;
        
        // Load CLUT to our slot's CLUT location
        LoadImage(&clut_rect, (u_long *)timData.clut);
        DrawSync(0);
        
        // Calculate CLUT ID using our coordinates
        slot->clut = getClut(slot->clut_rect.x, slot->clut_rect.y);
    } else {
        // 16-bit or 24-bit textures don't use CLUT
        slot->clut = 0;
    }
    
    slot->in_use = 1;
    slot->loaded = timHeader;  // Track which texture is loaded
    
    // Return CLUT if requested
    if (outClut != NULL) {
        *outClut = slot->clut;
    }
    
    return slot->tpage;
}

// Find if a texture is already loaded in any slot
// Returns slot index if found, -1 if not loaded
int FindLoadedTexture(const u_char *timHeader) {
    int i;
    for (i = 0; i < VRAM_SLOT_COUNT; i++) {
        if (vram_slots[i].in_use && vram_slots[i].loaded == timHeader) {
            return i;
        }
    }
    return -1;
}

// Bind texture only if not already in the specified slot
// Avoids redundant VRAM uploads when texture is already loaded
u_short BindTextureIfNeeded(const u_char *timHeader, int slotIdx, u_short *outClut) {
    VRAMSlot *slot;
    
    if (slotIdx < 0 || slotIdx >= VRAM_SLOT_COUNT) {
        return 0;
    }
    
    slot = &vram_slots[slotIdx];
    
    // Check if this texture is already in this slot
    if (slot->in_use && slot->loaded == timHeader) {
        // Already loaded, just return existing tpage/clut
        if (outClut != NULL) {
            *outClut = slot->clut;
        }
        return slot->tpage;
    }
    
    // Not loaded, do full bind
    return BindTexture(timHeader, slotIdx, outClut);
}

// Unbind/clear a VRAM slot
void UnbindSlot(int slotIdx) {
    if (slotIdx >= 0 && slotIdx < VRAM_SLOT_COUNT) {
        vram_slots[slotIdx].in_use = 0;
        vram_slots[slotIdx].tpage = 0;
        vram_slots[slotIdx].clut = 0;
        vram_slots[slotIdx].loaded = NULL;
    }
}

// Get tpage for a slot
u_short GetSlotTPage(int slotIdx) {
    if (slotIdx >= 0 && slotIdx < VRAM_SLOT_COUNT) {
        return vram_slots[slotIdx].tpage;
    }
    return 0;
}

// Get clut for a slot
u_short GetSlotClut(int slotIdx) {
    if (slotIdx >= 0 && slotIdx < VRAM_SLOT_COUNT) {
        return vram_slots[slotIdx].clut;
    }
    return 0;
}

// Check if slot is in use
int IsSlotInUse(int slotIdx) {
    if (slotIdx >= 0 && slotIdx < VRAM_SLOT_COUNT) {
        return vram_slots[slotIdx].in_use;
    }
    return 0;
}
