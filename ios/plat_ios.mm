// iOS platform layer: implements the raylib-compatible query functions the C
// game code calls (declared in ok_types.h), backed by state the UIKit view
// pushes in (ios_main.mm). No raylib.
#import <QuartzCore/QuartzCore.h> // CACurrentMediaTime

#include <stdarg.h>
#include <stdio.h>

#include "ok_types.h"
#include "plat_ios.h"

// --- State fed by the view -------------------------------------------------
static int     s_screen_w = 1, s_screen_h = 1; // drawable size, pixels
static Vector2 s_touches[16];
static int     s_touch_count = 0;
static int     s_gesture = 0;                   // one-shot; cleared on read
static bool    s_focused = true;

void plat_ios_set_screen(int width_px, int height_px) {
    s_screen_w = width_px  > 0 ? width_px  : 1;
    s_screen_h = height_px > 0 ? height_px : 1;
}

void plat_ios_set_touches(const Vector2* points_px, int count) {
    if (count > 16) count = 16;
    if (count < 0)  count = 0;
    for (int i = 0; i < count; i++) s_touches[i] = points_px[i];
    s_touch_count = count;
}

void plat_ios_post_gesture(int gesture) { s_gesture = gesture; }
void plat_ios_set_focus(bool focused)   { s_focused = focused; }

// --- Queries the game reads (C linkage via ok_types.h's extern "C") --------
int GetScreenWidth(void)  { return s_screen_w; }
int GetScreenHeight(void) { return s_screen_h; }
int GetTouchPointCount(void) { return s_touch_count; }

Vector2 GetTouchPosition(int index) {
    if (index < 0 || index >= s_touch_count) { Vector2 z = {0, 0}; return z; }
    return s_touches[index];
}

int GetGestureDetected(void) { int g = s_gesture; s_gesture = 0; return g; }

double GetTime(void)       { return CACurrentMediaTime(); }
bool   IsWindowFocused(void)   { return s_focused; }
bool   WindowShouldClose(void) { return false; }

// raylib-compatible transient formatter: returns a pointer into a small ring of
// static buffers, so a few concurrent TextFormat() results stay valid.
const char* TextFormat(const char* text, ...) {
    enum { BUFS = 4, LEN = 256 };
    static char buffers[BUFS][LEN];
    static int  index = 0;
    char* cur = buffers[index];
    index = (index + 1) % BUFS;
    va_list args;
    va_start(args, text);
    vsnprintf(cur, LEN, text, args);
    va_end(args);
    return cur;
}
