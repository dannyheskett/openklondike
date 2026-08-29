#include "input.h"
#include "render.h"
#include "platform.h"
#include "ok_types.h"
#if !defined(PLATFORM_IOS)
#include <raylib.h>  // keyboard/mouse; iOS is touch-only (queries come from plat_ios)
#endif
#include <stdlib.h>
#include <math.h>

// input_poll() composes up to two sources into one Input:
//   - mouse + keyboard: desktop native builds, and the web build when it is
//     running the desktop layout (a PC browser)
//   - touch:            Android, iOS, and the web build running the touch layout
// The web build carries both and picks by layout, so the same binary gives a
// phone gestures and a laptop a mouse. The two never run together on the
// pointer fields, because a mouse click and a touch-down would otherwise both
// try to start a drag in the same frame.

#if !defined(PLATFORM_ANDROID) && !defined(PLATFORM_IOS)

#define DOUBLE_CLICK_FRAMES 26   // ~0.43s at 60fps
#define DOUBLE_CLICK_SLOP   6    // px the pointer may drift between the two clicks

// Keys only. Always polled on the platforms that have a keyboard, so a desktop
// browser running the touch layout can still hit Escape.
static void poll_keys(Input* in) {
    in->escape_pressed = IsKeyPressed(KEY_ESCAPE);

    bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    in->fullscreen_toggle = alt && IsKeyPressed(KEY_ENTER);

    in->menu_up    = IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W);
    in->menu_down  = IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S);
    in->menu_left  = IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A);
    in->menu_right = IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D);
    in->select_pressed = (IsKeyPressed(KEY_ENTER) && !in->fullscreen_toggle)
                       || IsKeyPressed(KEY_SPACE);
}

// Mouse pointer: click to deal, drag to move, double-click or right-click to
// send a card to its foundation.
static void poll_mouse(Input* in) {
    static int frames_since_click = 1000;
    static int last_x = 0, last_y = 0;

    Vector2 mp = GetMousePosition();
    in->mouse_x = (int)mp.x;
    in->mouse_y = (int)mp.y;

    in->left_pressed  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    in->left_down     = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    in->left_released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    bool right_pressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

    bool double_click = false;
    frames_since_click++;
    if (in->left_pressed) {
        if (frames_since_click <= DOUBLE_CLICK_FRAMES
            && abs(in->mouse_x - last_x) <= DOUBLE_CLICK_SLOP
            && abs(in->mouse_y - last_y) <= DOUBLE_CLICK_SLOP) {
            double_click = true;
            frames_since_click = 1000;   // consume; a triple click isn't two doubles
        } else {
            frames_since_click = 0;
        }
        last_x = in->mouse_x;
        last_y = in->mouse_y;
    }
    in->auto_move_pressed = double_click || right_pressed;
    in->any_pressed = in->left_pressed || right_pressed;
}
#endif // !PLATFORM_ANDROID && !PLATFORM_IOS

#ifdef OK_TOUCH
// Gesture-recognizer state that persists across frames for the current touch
// sequence (first finger down to last finger up). Held in one module-owned
// value rather than function statics so the hidden state is explicit and the
// unit tests can reset it between cases.
typedef struct {
    bool    active;    // a touch sequence is in progress
    Vector2 origin;    // where the sequence started (px)
    Vector2 last;      // most recent pointer position (source of the tap point)
    double  t0;        // sequence start time (s)
    float   travel;    // furthest distance from the origin reached so far (px)
    int     max_np;    // most simultaneous fingers seen during the sequence
} TouchState;

static TouchState s_touch;

// Longest a contact may last and still count as a tap rather than a considered
// drag that happened to end where it began. Generous: deliberately placing a
// finger on a card, pausing, and lifting is a tap in every mobile card game.
#define TAP_MAX_SECONDS      0.70
// Two-finger contact longer than this is neither a tap nor a menu request.
#define TWO_FINGER_MAX_SECONDS 0.50

// How far the finger may wander and still be a tap, and how far it must travel
// before the run is lifted clear of the fingertip. Both scale with the card, so
// they feel identical on a phone and on a tablet.
static float tap_slop(void) {
    int cw = render_card_width();
    float slop = cw / 6.0f;
    return (slop < 8.0f) ? 8.0f : slop;
}

// Touch source: the board is manipulated directly, the way every mobile card
// game works. Touch a card and it follows the finger; lift it over a legal pile
// and it lands there; tap a card and it flies home on its own; tap the stock to
// deal. A two-finger tap opens the menu (there is no key to press), and swipes
// move the menu selection.
static void poll_touch(Input* in) {
    // Active pointers: touch points, or the mouse while its button is held.
    // A desktop browser reports no touch points for a mouse, so without this the
    // touch layout would be unplayable when forced on a PC.
    int n = GetTouchPointCount();
    Vector2 pts[8];
    int np = 0;
    for (int i = 0; i < n && np < 8; i++) pts[np++] = GetTouchPosition(i);
#if !defined(PLATFORM_IOS)
    if (n == 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) pts[np++] = GetMousePosition();
#endif

    double now = GetTime();

    if (np > 0) {
        Vector2 p = pts[0];
        if (!s_touch.active) {
            s_touch.active = true;
            s_touch.origin = p;
            s_touch.t0     = now;
            s_touch.travel = 0.0f;
            s_touch.max_np = 0;
            in->left_pressed = true;
        }
        if (np > s_touch.max_np) s_touch.max_np = np;
        // pts[0] jumps when a second finger lands or lifts, so stop tracking
        // travel then rather than misreading the jump as a drag.
        if (s_touch.max_np < 2) {
            float dx = p.x - s_touch.origin.x, dy = p.y - s_touch.origin.y;
            float d  = dx * dx + dy * dy;
            if (d > s_touch.travel * s_touch.travel) s_touch.travel = sqrtf(d);
            s_touch.last = p;
        }
        in->left_down = true;
        in->mouse_x = (int)s_touch.last.x;
        in->mouse_y = (int)s_touch.last.y;
    } else if (s_touch.active) {
        double dur = now - s_touch.t0;
        in->mouse_x = (int)s_touch.last.x;
        in->mouse_y = (int)s_touch.last.y;
        in->left_released = true;

        if (s_touch.max_np >= 2) {
            // Two-finger tap: open the menu. The game stays resumable, and main
            // handles Escape before the release, so the in-flight run is
            // returned rather than dropped somewhere by the second finger.
            if (dur < TWO_FINGER_MAX_SECONDS) {
                in->escape_pressed = true;
                in->any_pressed    = true;
            }
        } else if (dur < TAP_MAX_SECONDS && s_touch.travel <= tap_slop()) {
            in->tap        = true;
            in->tap_x      = s_touch.last.x;
            in->tap_y      = s_touch.last.y;
            in->any_pressed = true;
        }
        s_touch.active = false;
    }

    // Swipes drive the menu: up/down move the cursor, left/right cycle an
    // Options value. (Taps are decided on release, above.) These fields are read
    // only on the menu screens, so a swipe across the board while playing is
    // ignored rather than doing something surprising to a card.
    int g = GetGestureDetected();
    if (g == GESTURE_SWIPE_UP)    in->menu_up    = true;
    if (g == GESTURE_SWIPE_DOWN)  in->menu_down  = true;
    if (g == GESTURE_SWIPE_LEFT)  in->menu_left  = true;
    if (g == GESTURE_SWIPE_RIGHT) in->menu_right = true;

#if !defined(PLATFORM_IOS)
    // Android hardware/gesture Back button (KEY_BACK); harmless no-op on web.
    if (IsKeyPressed(KEY_BACK)) {
        in->escape_pressed = true;
        in->any_pressed    = true;
    }
#endif
}
#endif // OK_TOUCH

Input input_poll(void) {
    Input in = {0};
#if !defined(PLATFORM_ANDROID) && !defined(PLATFORM_IOS)
    poll_keys(&in);
    if (!render_use_scaled()) poll_mouse(&in);
#endif
#ifdef OK_TOUCH
    if (render_use_scaled()) poll_touch(&in);
#endif
    in.any_pressed = in.any_pressed || in.left_pressed || in.escape_pressed
                   || in.menu_up || in.menu_down || in.select_pressed;
    return in;
}
