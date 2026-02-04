#ifndef SOUND_H
#define SOUND_H

// Initialize SPU and CD audio system
void initSound(void);

// Play a CD audio track (track number, e.g., 2 for second track)
void playCDTrack(int track);

// Stop CD playback
void stopCD(void);

#endif
