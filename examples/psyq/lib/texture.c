#include "texture.h"
#include "chardata/rikatexture.h"
#include "chardata/moontexture.h"
#include "chardata/cointexture.h"
#include "chardata/startexture.h"
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

// Texture info
u_short tpage = 0;
u_short clut = 0;

// Moon texture info
u_short moon_tpage = 0;
u_short moon_clut = 0;

// Coin texture info
u_short coin_tpage = 0;
u_short coin_clut = 0;

// Star texture info
u_short star_tpage = 0;
u_short star_clut = 0;

void loadTexture(void) {
    GsIMAGE rika_timData;
    GsIMAGE moon_timData;
    GsIMAGE coin_timData;
    GsIMAGE star_timData;
    
    // Load rika texture using GsGetTimInfo and LoadTPage
    GsGetTimInfo((u_long *)(rikatexture_tim + 4), &rika_timData);
    tpage = LoadTPage(rika_timData.pixel, rika_timData.pmode, 0, 
                      rika_timData.px, rika_timData.py, 
                      rika_timData.pw, rika_timData.ph);
    clut = LoadClut(rika_timData.clut, rika_timData.cx, rika_timData.cy);
    DrawSync(0);
    
    // Load moon texture using GsGetTimInfo and LoadTPage
    GsGetTimInfo((u_long *)(moontexture_tim + 4), &moon_timData);
    moon_tpage = LoadTPage(moon_timData.pixel, moon_timData.pmode, 0, 
                           moon_timData.px, moon_timData.py,
                           moon_timData.pw, moon_timData.ph);
    moon_clut = LoadClut(moon_timData.clut, moon_timData.cx, moon_timData.cy);
    DrawSync(0);
    
    // Load coin texture using GsGetTimInfo and LoadTPage
    GsGetTimInfo((u_long *)(cointexture_tim + 4), &coin_timData);
    coin_tpage = LoadTPage(coin_timData.pixel, coin_timData.pmode, 0, 
                           coin_timData.px, coin_timData.py,
                           coin_timData.pw, coin_timData.ph);
    coin_clut = LoadClut(coin_timData.clut, coin_timData.cx, coin_timData.cy);
    DrawSync(0);
    
    // Load star texture using GsGetTimInfo and LoadTPage
    GsGetTimInfo((u_long *)(startexture_tim + 4), &star_timData);
    star_tpage = LoadTPage(star_timData.pixel, star_timData.pmode, 0, 
                           star_timData.px, star_timData.py,
                           star_timData.pw, star_timData.ph);
    star_clut = LoadClut(star_timData.clut, star_timData.cx, star_timData.cy);
    DrawSync(0);
}
