#ifndef OPENKLONDIKE_INPUT_H
#define OPENKLONDIKE_INPUT_H

#include <stdbool.h>

typedef struct {
    int  mouse_x, mouse_y;
    bool left_pressed;     // left button just went down
    bool left_down;        // left button held
    bool left_released;    // left button just went up
    bool double_click;     // two left presses in quick succession
    bool right_pressed;    // right click (shortcut: send to foundation)

    bool escape_pressed;
    bool fullscreen_toggle;  // Alt+Enter

    // Menu navigation
    bool menu_up, menu_down;
    bool select_pressed;
    bool any_pressed;
} Input;

Input input_poll(void);

#endif
