#ifndef PRESENT_H
#define PRESENT_H

// Frame presentation, the same in every game in this family. This file is
// identical in every game.
//
// A scene is a function that draws the whole frame for a view_w x view_h view.
// present() calls it once for the window at the window's live size, and, while
// the desktop recorder is running, once more at the fixed capture size
// (REC_W x REC_H, platform.h), supersampled 2x and minified for anti-aliasing.
// Scenes therefore derive every metric from the view they are given, never
// from the window. While recording, the window also shows a small REC mark.

typedef void (*SceneFn)(void* ctx, int view_w, int view_h);

void present_init(void);      // after window_init(): the capture canvases
void present_cleanup(void);   // before window_close()
void present(SceneFn scene, void* ctx);

#endif // PRESENT_H
