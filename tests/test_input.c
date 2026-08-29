// Unit tests for the touch-gesture recognizer in input.c — no raylib, no
// window, no device.
//
// input.c is included directly and compiled -DPLATFORM_IOS (the Makefile passes
// it), which is the raylib-free configuration it already supports: the platform
// queries it calls are plain extern declarations from ok_types.h, so the test
// supplies them as a scripted touch surface and a scripted clock. The recognizer
// under test is the same C that Android, iOS, and the web build all compile.
//
// The grammar these tests pin down is the one every mobile card game uses:
//   touch down            -> the card under the finger is picked up
//   move and lift         -> it is dropped where the finger let go
//   lift without moving   -> a tap, which sends the card home instead
//   two fingers, tapped   -> the menu
//   swipe up / down       -> move the menu selection
#include "../src/input.c"

#include <stdio.h>
#include <stdlib.h>

#define PASS(name) printf("PASS: %s\n", name)
#define FAIL(name, msg) do { fprintf(stderr, "FAIL: %s — %s\n", name, msg); exit(1); } while(0)

// --- The scripted surface the recognizer reads ------------------------------
static Vector2 g_touches[8];
static int     g_touch_count = 0;
static double  g_time = 0.0;
static int     g_gesture = 0;

int     GetTouchPointCount(void)      { return g_touch_count; }
Vector2 GetTouchPosition(int index)   { return g_touches[index]; }
int     GetGestureDetected(void)      { int g = g_gesture; g_gesture = 0; return g; }
double  GetTime(void)                 { return g_time; }

// The recognizer only asks the renderer two things: which layout is live (the
// touch path is inert on the desktop one) and how wide a card is, which is what
// its distance thresholds scale off.
bool render_use_scaled(void) { return true; }
int  render_card_width(void)   { return 120; }   // a phone-sized card

// Start a fresh gesture: no fingers down, no banked state.
static void reset(void) {
    TouchState blank = {0};
    s_touch = blank;
    g_touch_count = 0;
    g_gesture = 0;
    g_time = 100.0;   // a non-zero clock, as a real one would be
}

// Advance the clock by one 60 Hz frame with the given fingers down, and poll.
static Input frame(double dt, int count, float x0, float y0, float x1, float y1) {
    g_time += dt;
    g_touch_count = count;
    if (count > 0) { g_touches[0].x = x0; g_touches[0].y = y0; }
    if (count > 1) { g_touches[1].x = x1; g_touches[1].y = y1; }
    return input_poll();
}

#define FRAME_DT (1.0 / 60.0)
static Input one(int count, float x, float y) { return frame(FRAME_DT, count, x, y, 0, 0); }
static Input none(void)                       { return frame(FRAME_DT, 0, 0, 0, 0, 0); }

// --------------------------------------------------------------------------
// A finger placed on a card and lifted without moving is a tap: the card should
// fly to its foundation, not be dropped back where it started.
// --------------------------------------------------------------------------
static void test_tap(void) {
    reset();
    Input in = one(1, 300, 400);
    if (!in.left_pressed) FAIL("tap", "no press on the first frame of contact");
    if (!in.left_down)    FAIL("tap", "the pointer is not held during contact");
    if (in.tap)           FAIL("tap", "a tap fired before the finger lifted");

    for (int i = 0; i < 6; i++) one(1, 300, 400);
    in = none();
    if (!in.left_released) FAIL("tap", "no release when the finger lifted");
    if (!in.tap)           FAIL("tap", "a still contact was not read as a tap");
    if (in.tap_x != 300.0f || in.tap_y != 400.0f)
        FAIL("tap", "the tap reported the wrong point");
    if (in.escape_pressed) FAIL("tap", "a one-finger tap opened the menu");
    PASS("tap");
}

// Small wobble is still a tap -- a fingertip is never perfectly still. Past the
// slop (a sixth of a card) the gesture has become a drag.
static void test_tap_slop(void) {
    reset();
    one(1, 300, 400);
    one(1, 306, 404);              // ~7px: inside the 20px slop for a 120px card
    Input in = none();
    if (!in.tap) FAIL("tap_slop", "a small wobble was not forgiven");

    reset();
    one(1, 300, 400);
    one(1, 340, 400);              // 40px: unambiguously a drag
    in = none();
    if (in.tap) FAIL("tap_slop", "a 40px move was still read as a tap");
    PASS("tap_slop");
}

// A drag: the pointer tracks the finger every frame, and the release is a drop
// (no tap), so the run lands on whatever pile it was let go over.
static void test_drag(void) {
    reset();
    Input in = one(1, 200, 300);
    if (in.mouse_x != 200 || in.mouse_y != 300)
        FAIL("drag", "the pointer did not start under the finger");
    for (int i = 1; i <= 10; i++) in = one(1, 200 + i * 20, 300 + i * 10);
    if (in.mouse_x != 400 || in.mouse_y != 400)
        FAIL("drag", "the pointer did not track the finger");
    if (!in.left_down) FAIL("drag", "the pointer was released mid-drag");

    in = none();
    if (!in.left_released) FAIL("drag", "no release at the end of the drag");
    if (in.tap)            FAIL("drag", "a drag was reported as a tap");
    PASS("drag");
}

// Holding a finger on a card for a long time and then lifting is not a tap: the
// player was thinking about a drag, and nothing should move on its own.
static void test_long_press_is_not_a_tap(void) {
    reset();
    one(1, 300, 400);
    frame(TAP_MAX_SECONDS + 0.1, 1, 300, 400, 0, 0);
    Input in = none();
    if (in.tap) FAIL("long_press", "a long hold was reported as a tap");
    if (!in.left_released) FAIL("long_press", "no release after a long hold");
    PASS("long_press");
}

// Two fingers tapped together open the menu. There is no key to press on a
// phone, so this is the only way back out of a game.
static void test_two_finger_tap_opens_the_menu(void) {
    reset();
    one(1, 300, 400);
    frame(FRAME_DT, 2, 300, 400, 500, 400);
    Input in = none();
    if (!in.escape_pressed) FAIL("two_finger", "a two-finger tap did not open the menu");
    if (in.tap)             FAIL("two_finger", "it also fired a one-finger tap");

    // Held too long, it is not a tap at all and must do nothing.
    reset();
    one(1, 300, 400);
    frame(TWO_FINGER_MAX_SECONDS + 0.1, 2, 300, 400, 500, 400);
    in = none();
    if (in.escape_pressed) FAIL("two_finger", "a long two-finger hold opened the menu");
    PASS("two_finger");
}

// Swipes move the menu selection.
static void test_swipes_drive_the_menu(void) {
    reset();
    g_gesture = GESTURE_SWIPE_UP;
    Input in = none();
    if (!in.menu_up || in.menu_down) FAIL("swipe", "swipe up did not move the selection up");

    g_gesture = GESTURE_SWIPE_DOWN;
    in = none();
    if (!in.menu_down || in.menu_up) FAIL("swipe", "swipe down did not move the selection down");
    PASS("swipe");
}

// Two gestures in a row must not bleed into each other: the state is per
// sequence, so a drag followed by a tap is still read as a tap.
static void test_sequences_are_independent(void) {
    reset();
    one(1, 200, 200);
    one(1, 400, 200);
    Input in = none();
    if (in.tap) FAIL("sequences", "the drag was read as a tap");

    in = one(1, 500, 500);
    if (!in.left_pressed) FAIL("sequences", "the second gesture did not press");
    in = none();
    if (!in.tap) FAIL("sequences", "the tap after a drag was swallowed");
    if (in.tap_x != 500.0f || in.tap_y != 500.0f)
        FAIL("sequences", "the tap carried the previous gesture's point");
    PASS("sequences");
}

int main(void) {
    test_tap();
    test_tap_slop();
    test_drag();
    test_long_press_is_not_a_tap();
    test_two_finger_tap_opens_the_menu();
    test_swipes_drive_the_menu();
    test_sequences_are_independent();
    printf("\nAll tests passed.\n");
    return 0;
}
