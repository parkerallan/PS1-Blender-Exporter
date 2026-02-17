#include "sound.h"
#include <sys/types.h>
#include <libspu.h>
#include <libds.h>
#include <libcd.h>

static SpuCommonAttr soundAttr;
static CdlATV cdVolume;

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

void playCDTrackLoop(int track)
{
    int tracks[2];
    tracks[0] = track;
    tracks[1] = 0;
    
    DsPlay(2, tracks, 0);
}

void playCDTrackOnce(int track)
{
    int tracks[2];
    tracks[0] = track;
    tracks[1] = 0;
    
    DsPlay(1, tracks, 0);
}

void stopCDTrack(void)
{
    DsPlay(0, NULL, 0);
}

void setCDVolume(int volume)
{
    // Volume is 0-255, maps directly to CdlATV range (0-255)
    // CdlATV controls: val0=CD Left to SPU Left, val1=CD Left to SPU Right,
    //                  val2=CD Right to SPU Right, val3=CD Right to SPU Left
    cdVolume.val0 = volume;  // Left to Left
    cdVolume.val1 = 0;       // Left to Right (no crossover)
    cdVolume.val2 = volume;  // Right to Right
    cdVolume.val3 = 0;       // Right to Left (no crossover)
    
    CdMix(&cdVolume);
}