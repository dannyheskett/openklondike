#ifndef OPENKLONDIKE_RECORDER_H
#define OPENKLONDIKE_RECORDER_H

#include <stdbool.h>
#include "platform.h"
#if !defined(PLATFORM_IOS)
#include <raylib.h>  // for RenderTexture2D (recorder_capture); absent on iOS
#endif

// Frame-fidelity movie recorder. When active, every presented frame is read
// back from the canvas, converted to YUV, encoded as H.264 (visually
// lossless), and muxed straight into an .mp4 at a constant 60 fps.
// There are no intermediate files: encoding is streamed as the game runs.
//
// Encoding each frame is synchronous, so while recording the game may run
// below real-time on slower machines; the resulting video is still exactly one
// frame per rendered frame.

// The recorder is a desktop-only feature: it writes an .mp4 into the working
// directory using the vendored minih264/minimp4 encoders, neither of which is
// available (nor meaningful) on the mobile and web builds. There the whole
// implementation compiles out and these become no-op stubs.

// Start recording to `path`. If `path` is NULL/empty, an auto-named file
// "openklondike-YYYYMMDD-HHMMSS.mp4" is created in the working directory.
// Returns true on success. No-op (returns false) if already recording.
bool recorder_start(const char* path);

// Finalize the .mp4 and release resources. Safe to call when not recording.
void recorder_stop(void);

// Toggle recording on/off (auto-named file when turning on). Returns the new
// active state.
bool recorder_toggle(void);

bool recorder_active(void);

// Read back the canvas and encode one frame. No-op when not recording.
// (Desktop only -- the canvas type is a raylib RenderTexture2D, which the iOS
// build has no equivalent for; iOS never records.)
#if !defined(PLATFORM_IOS)
void recorder_capture(const RenderTexture2D* canvas);
#endif

#endif
