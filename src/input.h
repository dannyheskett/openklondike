#ifndef OPENKLONDIKE_INPUT_H
#define OPENKLONDIKE_INPUT_H

#include <stdbool.h>

// One frame of pointer input, composed from up to two sources: mouse + keyboard
// on the desktop builds and desktop browsers, and touch gestures on Android,
// iOS, and mobile browsers. Both sources fill the same fields, so main.c drives
// the game from a single pointer regardless of platform -- the only thing that
// differs is when the auto-move request arrives.
typedef struct {
    int  mouse_x, mouse_y;
    bool left_pressed;     // pointer just went down
    bool left_down;        // pointer held
    bool left_released;    // pointer just went up

    // Send the card home. On the desktop this is decided at press time (a
    // double-click or a right-click, which are unambiguous the moment they
    // happen). On touch it can only be decided on release, once the gesture has
    // proved to be a tap rather than the start of a drag -- so it arrives as
    // `tap`, together with the point that was tapped.
    bool  auto_move_pressed;
    bool  tap;
    float tap_x, tap_y;

    bool escape_pressed;
    bool fullscreen_toggle;  // Alt+Enter

    // Menu navigation
    bool menu_up, menu_down;
    bool select_pressed;
    bool any_pressed;
} Input;

Input input_poll(void);

#endif
