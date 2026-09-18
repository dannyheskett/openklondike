#include "window.h"
#include "gfx.h"

#if !defined(PLATFORM_IOS)
#include <raylib.h>
#endif

#if defined(PLATFORM_IOS)

void window_init(const char* title) { (void)title; }
void window_close(void) {}
bool window_should_close(void) { return false; }
void window_toggle_fullscreen(void) {}

#else

void window_init(const char* title) {
#if defined(PLATFORM_ANDROID)
    // Immersive fullscreen so the game draws under the status bar and camera
    // cutout (paired with windowLayoutInDisplayCutoutMode=shortEdges in the
    // theme). 0x0 makes raylib render at the device's native resolution; any
    // fixed size would be letterboxed into the display.
    SetConfigFlags(FLAG_FULLSCREEN_MODE | FLAG_MSAA_4X_HINT);
    InitWindow(0, 0, title);
#elif defined(PLATFORM_WEB)
    // The HTML shell sizes the canvas to the viewport; GetScreenWidth/Height
    // follow it on resize and rotation.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(WINDOW_MIN_W, WINDOW_MIN_H, title);
#else
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(WINDOW_W, WINDOW_H, title);
    SetWindowMinSize(WINDOW_MIN_W, WINDOW_MIN_H);
#endif
    SetExitKey(KEY_NULL);   // Escape belongs to the game, not the window
    SetTargetFPS(60);
    gfx_font_init();        // the GL context exists now
}

void window_close(void) { CloseWindow(); }

bool window_should_close(void) { return WindowShouldClose(); }

void window_toggle_fullscreen(void) {
#if defined(PLATFORM_ANDROID) || defined(PLATFORM_WEB)
    // Android is always fullscreen, and a browser tab cannot enter fullscreen
    // outside a user-gesture handler the game does not own.
#else
    static int prev_w = WINDOW_W, prev_h = WINDOW_H;
    static Vector2 prev_pos = { 0, 0 };
    if (IsWindowFullscreen()) {
        ToggleFullscreen();
        SetWindowSize(prev_w, prev_h);
        SetWindowPosition((int)prev_pos.x, (int)prev_pos.y);
    } else {
        prev_w = GetScreenWidth();
        prev_h = GetScreenHeight();
        prev_pos = GetWindowPosition();
        int m = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(m), GetMonitorHeight(m));
        ToggleFullscreen();
    }
#endif
}

#endif // PLATFORM_IOS

// iOS reports focus through plat_ios.mm (app active / resigned).
bool window_focus_lost(void) {
    static bool had_focus = true;
    bool focused = IsWindowFocused();
    bool lost = had_focus && !focused;
    had_focus = focused;
    return lost;
}
