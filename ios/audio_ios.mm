// iOS audio backend: AVAudioEngine driving a small pool of player-node "voices".
// sound.c synthesizes short mono int16 PCM clips, all at one shared format
// (SFX_SAMPLE_RATE, mono); here each becomes an AVAudioPCMBuffer (float32).
// Playback round-robins across the voice pool, so re-triggering the same effect
// lands on a free voice and overlaps cleanly instead of truncating the previous
// instance (a single node per effect would cut itself off). All voices share the
// clip format, so any voice can play any buffer.
#import <AVFoundation/AVFoundation.h>

#include "audio.h"

#define MAX_SOUNDS 32
#define VOICES     8       // simultaneous playbacks; wrap-around interrupts the oldest
#define SFX_SAMPLE_RATE 44100.0 // must match sound.c's SAMPLE_RATE (single shared format)

static AVAudioEngine*     s_engine;
static AVAudioPCMBuffer*  s_buffers[MAX_SOUNDS];
static AVAudioPlayerNode* s_voices[VOICES];
static int  s_voice_next = 0;
static int  s_count = 0;
static bool s_ready = false;

void audio_init(void) {
    @autoreleasepool {
        NSError* err = nil;
        AVAudioSession* session = [AVAudioSession sharedInstance];
        // Ambient: mixes with other audio and honours the silent switch. The
        // effects are decoration, not the point of the app, so the hardware
        // switch wins over the in-game "Sound: On" toggle (Apple's guidance, and
        // what players expect from a phone they have deliberately silenced).
        [session setCategory:AVAudioSessionCategoryAmbient error:&err];
        [session setActive:YES error:&err];

        s_engine = [[AVAudioEngine alloc] init];
        AVAudioMixerNode* mixer = s_engine.mainMixerNode; // instantiate before connecting

        // Wire the voice pool once, all with the shared clip format. Attaching
        // and connecting before start() is the well-supported ordering.
        AVAudioFormat* fmt =
            [[AVAudioFormat alloc] initStandardFormatWithSampleRate:SFX_SAMPLE_RATE channels:1];
        for (int i = 0; i < VOICES; i++) {
            s_voices[i] = [[AVAudioPlayerNode alloc] init];
            [s_engine attachNode:s_voices[i]];
            [s_engine connect:s_voices[i] to:mixer format:fmt];
        }

        s_count = 0;
        s_voice_next = 0;
        s_ready = [s_engine startAndReturnError:&err] ? true : false;
    }
}

void audio_shutdown(void) {
    if (!s_ready) return;
    [s_engine stop];
    s_ready = false;
}

bool audio_ready(void) { return s_ready; }

AudioHandle audio_load(const int16_t* samples, int frame_count, int sample_rate) {
    if (!s_ready || s_count >= MAX_SOUNDS || frame_count <= 0) return -1;
    @autoreleasepool {
        AVAudioFormat* fmt =
            [[AVAudioFormat alloc] initStandardFormatWithSampleRate:sample_rate channels:1];
        AVAudioPCMBuffer* buf =
            [[AVAudioPCMBuffer alloc] initWithPCMFormat:fmt frameCapacity:(AVAudioFrameCount)frame_count];
        buf.frameLength = (AVAudioFrameCount)frame_count;
        float* dst = buf.floatChannelData[0];
        for (int i = 0; i < frame_count; i++) dst[i] = samples[i] / 32768.0f;

        s_buffers[s_count] = buf;
        return s_count++;
    }
}

void audio_play(AudioHandle handle) {
    if (!s_ready || handle < 0 || handle >= s_count) return;
    // An interruption (phone call, Siri, another app taking the session) stops the
    // engine and it does not restart itself; without this the game goes silent for
    // the rest of the launch. Restarting is cheap and a no-op while it's running.
    if (!s_engine.isRunning) {
        NSError* err = nil;
        [[AVAudioSession sharedInstance] setActive:YES error:&err];
        if (![s_engine startAndReturnError:&err]) return;
    }
    // Round-robin the pool: consecutive plays (even of the same effect) land on
    // different voices and overlap. Interrupt only kicks in when the pool wraps
    // back to a voice still playing a long clip, avoiding queued-latency buildup.
    AVAudioPlayerNode* v = s_voices[s_voice_next];
    s_voice_next = (s_voice_next + 1) % VOICES;
    [v scheduleBuffer:s_buffers[handle]
                atTime:nil
               options:AVAudioPlayerNodeBufferInterrupts
     completionHandler:nil];
    if (!v.isPlaying) [v play];
}

void audio_unload(AudioHandle handle) {
    (void)handle; // buffers/voices released when the engine tears down at shutdown
}
