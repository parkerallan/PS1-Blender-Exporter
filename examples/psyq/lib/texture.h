#ifndef TEXTURE_H
#define TEXTURE_H

#include <sys/types.h>

// Texture info
extern u_short tpage;
extern u_short clut;

// Moon texture info
extern u_short moon_tpage;
extern u_short moon_clut;

// Coin texture info
extern u_short coin_tpage;
extern u_short coin_clut;

// Star texture info
extern u_short star_tpage;
extern u_short star_clut;

// Load textures into VRAM
void loadTexture(void);

#endif // TEXTURE_H
