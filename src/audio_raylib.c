// raylib audio backend (desktop / web / android): InitAudioDevice /
// LoadSoundFromWave / PlaySound, as sound.c used before the audio abstraction.
// Each effect keeps a small ring of sound aliases (LoadSoundAlias) that share the
// sample data but have independent playback state, and playback round-robins
// across the ring — so re-triggering the same effect overlaps cleanly on a free
// voice instead of restarting the single instance. This mirrors the iOS backend's
// voice pool (audio_ios.mm). iOS uses that backend instead of this file.
#include "audio.h"
#include <raylib.h>

#define MAX_SOUNDS 32
#define VOICES     4   // per-effect voices: how many instances of one effect may overlap

typedef struct {
    Sound voices[VOICES]; // voices[0] owns the sample data; [1..] are aliases sharing it
    int   next;           // round-robin cursor
} SoundSet;

static SoundSet s_sounds[MAX_SOUNDS];
static int      s_count = 0;
static bool     s_ready = false;

void audio_init(void) {
    InitAudioDevice();
    s_ready = IsAudioDeviceReady();
    s_count = 0;
}

void audio_shutdown(void) {
    if (!s_ready) return;
    CloseAudioDevice();
    s_ready = false;
    s_count = 0;
}

bool audio_ready(void) { return s_ready; }

AudioHandle audio_load(const int16_t* samples, int frame_count, int sample_rate) {
    if (!s_ready || s_count >= MAX_SOUNDS || frame_count <= 0) return -1;
    Wave wave = {
        .frameCount = (unsigned int)frame_count,
        .sampleRate = (unsigned int)sample_rate,
        .sampleSize = 16,
        .channels   = 1,
        .data       = (void*)samples,
    };
    SoundSet* set = &s_sounds[s_count];
    set->voices[0] = LoadSoundFromWave(wave);
    for (int i = 1; i < VOICES; i++) set->voices[i] = LoadSoundAlias(set->voices[0]);
    set->next = 0;
    return s_count++;
}

void audio_play(AudioHandle handle) {
    if (!s_ready || handle < 0 || handle >= s_count) return;
    SoundSet* set = &s_sounds[handle];
    // Next voice in the ring: consecutive plays of the same effect overlap
    // instead of cutting the previous instance short. Wrap-around only restarts
    // a voice once all VOICES are already sounding.
    PlaySound(set->voices[set->next]);
    set->next = (set->next + 1) % VOICES;
}

void audio_unload(AudioHandle handle) {
    if (!s_ready || handle < 0 || handle >= s_count) return;
    SoundSet* set = &s_sounds[handle];
    // Aliases first (they only reference voices[0]'s shared data), then the owner
    // (UnloadSound frees the shared sample data as well).
    for (int i = 1; i < VOICES; i++) UnloadSoundAlias(set->voices[i]);
    UnloadSound(set->voices[0]);
}
