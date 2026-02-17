#ifndef SOUND_H
#define SOUND_H

// Initialize SPU and CD audio system
void initSound(void);

// Play a CD audio track (track number, e.g., 2 for second track)
void playCDTrackLoop(int track);

// Play a CD audio track once (no looping)
void playCDTrackOnce(int track);

// Stop CD playback
void stopCDTrack(void);

// Set volume (0-255)
void setCDVolume(int volume);

#endif
