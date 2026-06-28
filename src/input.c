#include "input.h"
#include <raylib.h>
#include <stdlib.h>

#define DOUBLE_CLICK_FRAMES 26   // ~0.43s at 60fps
#define DOUBLE_CLICK_SLOP   6    // px the pointer may drift between the two clicks

Input input_poll(void) {
    static int  frames_since_click = 1000;
    static int  last_x = 0, last_y = 0;

    Input in = {0};

    Vector2 mp = GetMousePosition();
    in.mouse_x = (int)mp.x;
    in.mouse_y = (int)mp.y;

    in.left_pressed  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    in.left_down     = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    in.left_released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    in.right_pressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

    frames_since_click++;
    if (in.left_pressed) {
        if (frames_since_click <= DOUBLE_CLICK_FRAMES
            && abs(in.mouse_x - last_x) <= DOUBLE_CLICK_SLOP
            && abs(in.mouse_y - last_y) <= DOUBLE_CLICK_SLOP) {
            in.double_click = true;
            frames_since_click = 1000;   // consume; a triple click isn't two doubles
        } else {
            frames_since_click = 0;
        }
        last_x = in.mouse_x;
        last_y = in.mouse_y;
    }

    in.escape_pressed = IsKeyPressed(KEY_ESCAPE);

    bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    in.fullscreen_toggle = alt && IsKeyPressed(KEY_ENTER);

    in.menu_up   = IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W);
    in.menu_down = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S);
    in.select_pressed = (IsKeyPressed(KEY_ENTER) && !in.fullscreen_toggle)
                      || IsKeyPressed(KEY_SPACE);

    in.any_pressed = in.left_pressed || in.right_pressed || in.escape_pressed
                  || in.menu_up || in.menu_down || in.select_pressed;

    return in;
}
