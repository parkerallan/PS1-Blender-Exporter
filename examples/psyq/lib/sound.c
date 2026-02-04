#include "sound.h"
#include <sys/types.h>
#include <libspu.h>
#include <libds.h>

static SpuCommonAttr soundAttr;

void initSound(void)
{
    SpuInit();
    DsInit();
    
    soundAttr.mask = (SPU_COMMON_MVOLL | SPU_COMMON_MVOLR | SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR | SPU_COMMON_CDMIX);
    soundAttr.mvol.left = 0x3FFF;
    soundAttr.mvol.right = 0x3FFF;
    soundAttr.cd.volume.left = 0x3FFF;
    soundAttr.cd.volume.right = 0x3FFF;
    soundAttr.cd.mix = SPU_ON;
    
    SpuSetCommonAttr(&soundAttr);
}

void playCDTrack(int track)
{
    int tracks[2];
    tracks[0] = track;
    tracks[1] = 0;
    
    DsPlay(2, tracks, 0);
}

void stopCD(void)
{
    DsPlay(0, NULL, 0);
}
