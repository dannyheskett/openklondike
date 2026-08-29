// Sound effect synthesis. Every effect is generated at startup from code -- there
// are no audio asset files -- as a short mono int16 PCM clip, then handed to the
// platform audio backend (audio.h) to load and play. This file is backend-
// agnostic: audio_raylib.c serves desktop / web / android, ios/audio_ios.mm
// serves iOS.
#include "sound.h"
#include "audio.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#define SAMPLE_RATE 44100

static bool        enabled = false;
static AudioHandle effects[SFX_COUNT];

// Hand a synthesized buffer to the backend and release it: the backends copy the
// samples into their own storage (a raylib Wave, an AVAudioPCMBuffer).
static AudioHandle load_samples(int16_t* samples, int count) {
    if (!samples) return -1;
    AudioHandle h = audio_load(samples, count, SAMPLE_RATE);
    free(samples);
    return h;
}

static AudioHandle make_tone(float freq, float dur, float duty, float vol) {
    int n = (int)(dur * SAMPLE_RATE);
    int16_t* buf = malloc(sizeof(int16_t) * n);
    if (!buf) return -1;
    for (int i = 0; i < n; i++) {
        float phase = freq * ((float)i / SAMPLE_RATE);
        phase -= (int)phase;
        float env = 1.0f - (float)i / n;
        float v = (phase < duty ? 1.0f : -1.0f) * vol * env;
        buf[i] = (int16_t)(v * 32767.0f);
    }
    return load_samples(buf, n);
}

static AudioHandle make_sweep(float f0, float f1, float dur, float duty, float vol) {
    int n = (int)(dur * SAMPLE_RATE);
    int16_t* buf = malloc(sizeof(int16_t) * n);
    if (!buf) return -1;
    float phase = 0.0f;
    for (int i = 0; i < n; i++) {
        float freq = f0 + (f1 - f0) * ((float)i / n);
        phase += freq / SAMPLE_RATE;
        float p = phase - (int)phase;
        float env = 1.0f - (float)i / n;
        float v = (p < duty ? 1.0f : -1.0f) * vol * env;
        buf[i] = (int16_t)(v * 32767.0f);
    }
    return load_samples(buf, n);
}

static AudioHandle make_noise(float dur, float vol) {
    int n = (int)(dur * SAMPLE_RATE);
    int16_t* buf = malloc(sizeof(int16_t) * n);
    if (!buf) return -1;
    float hold = 0.0f;
    for (int i = 0; i < n; i++) {
        if (i % 6 == 0) hold = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        float env = 1.0f - (float)i / n;
        buf[i] = (int16_t)(hold * vol * env * 32767.0f);
    }
    return load_samples(buf, n);
}

static AudioHandle make_arp(const float* freqs, int count, float per_note,
                            float duty, float vol) {
    int note_n = (int)(per_note * SAMPLE_RATE);
    int n = note_n * count;
    int16_t* buf = malloc(sizeof(int16_t) * n);
    if (!buf) return -1;
    for (int j = 0; j < count; j++) {
        for (int i = 0; i < note_n; i++) {
            float phase = freqs[j] * ((float)i / SAMPLE_RATE);
            phase -= (int)phase;
            float env = 1.0f - (float)i / note_n;
            float v = (phase < duty ? 1.0f : -1.0f) * vol * env;
            buf[j * note_n + i] = (int16_t)(v * 32767.0f);
        }
    }
    return load_samples(buf, n);
}

void sound_init(void) {
    for (int i = 0; i < SFX_COUNT; i++) effects[i] = -1;
    audio_init();
    if (!audio_ready()) return;

    static const float found_arp[] = {659.25f, 987.77f};                    // a cheerful up
    static const float win_arp[]   = {523.25f, 659.25f, 783.99f, 1046.50f}; // C E G C
    static const float sel_arp[]   = {659.25f, 987.77f};

    effects[SFX_DEAL]        = make_noise(0.10f, 0.16f);
    effects[SFX_DRAW]        = make_tone(520.0f, 0.04f, 0.5f, 0.16f);
    effects[SFX_FLIP]        = make_tone(700.0f, 0.04f, 0.5f, 0.16f);
    effects[SFX_MOVE]        = make_tone(440.0f, 0.04f, 0.5f, 0.15f);
    effects[SFX_FOUNDATION]  = make_arp(found_arp, 2, 0.04f, 0.5f, 0.22f);
    effects[SFX_RECYCLE]     = make_sweep(220.0f, 440.0f, 0.14f, 0.5f, 0.18f);
    effects[SFX_INVALID]     = make_tone(150.0f, 0.10f, 0.5f, 0.20f);
    effects[SFX_WIN]         = make_arp(win_arp, 4, 0.06f, 0.5f, 0.30f);
    effects[SFX_MENU_MOVE]   = make_tone(440.0f, 0.03f, 0.5f, 0.18f);
    effects[SFX_MENU_SELECT] = make_arp(sel_arp, 2, 0.05f, 0.5f, 0.26f);
}

void sound_shutdown(void) {
    if (!audio_ready()) return;
    for (int i = 0; i < SFX_COUNT; i++)
        if (effects[i] >= 0) audio_unload(effects[i]);
    audio_shutdown();
}

bool sound_is_enabled(void) { return enabled; }
void sound_toggle(void)     { enabled = !enabled; }

void sound_play(SfxId id) {
    if (!enabled || id < 0 || id >= SFX_COUNT || effects[id] < 0) return;
    audio_play(effects[id]);
}
