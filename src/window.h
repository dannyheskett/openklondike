#ifndef WINDOW_H_INCLUDED
#define WINDOW_H_INCLUDED

// The window, the same in every game in this family. This file is identical in
// every game.
//
//   desktop  resizable, opens at WINDOW_W x WINDOW_H, never smaller than
//            WINDOW_MIN_W x WINDOW_MIN_H; Alt+Enter toggles borderless
//            fullscreen at the monitor's size and back to the window as it was.
//   web      the canvas follows the browser viewport (web/shell.html sizes it);
//            the browser owns fullscreen.
//   Android  immersive fullscreen at the device's native resolution.
//   iOS      UIKit owns the window (ios/ios_main.mm); these are no-ops.
//
// Every build asks for 4x MSAA, sets 60 fps, keeps Escape for the game rather
// than closing the window, and loads the bundled UI font.

#include <stdbool.h>

#define WINDOW_W      960
#define WINDOW_H      720
#define WINDOW_MIN_W  640
#define WINDOW_MIN_H  480

void window_init(const char* title);
void window_close(void);
bool window_should_close(void);
void window_toggle_fullscreen(void);

// True on the one frame the window goes from focused to unfocused (app
// backgrounded, browser tab hidden, desktop window deactivated). Games return
// to the menu on it. The edge rather than the level: a host that never reports
// focus at all (a headless X server, an embedded webview) would otherwise send
// the player back to the menu on every frame.
bool window_focus_lost(void);

#endif // WINDOW_H_INCLUDED
