#ifndef OPENKLONDIKE_AUDIO_H
#define OPENKLONDIKE_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

// Minimal audio playback backend: load short mono int16 PCM clips (the effects
// sound.c synthesizes) and play them on demand. Two implementations, chosen by
// platform: audio_raylib.c (desktop / web / android — raylib's audio) and
// audio_ios.mm (iOS — AVAudioEngine). sound.c owns the synthesis and is backend-
// agnostic.

typedef int AudioHandle; // >= 0 valid; < 0 invalid

#ifdef __cplusplus
extern "C" {
#endif

void audio_init(void);
void audio_shutdown(void);
bool audio_ready(void);

// Copy `frame_count` mono int16 samples at `sample_rate`; returns a handle.
AudioHandle audio_load(const int16_t* samples, int frame_count, int sample_rate);
void audio_play(AudioHandle handle);
void audio_unload(AudioHandle handle);

#ifdef __cplusplus
}
#endif

#endif // OPENKLONDIKE_AUDIO_H
