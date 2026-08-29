#ifndef OPENKLONDIKE_PLAT_IOS_H
#define OPENKLONDIKE_PLAT_IOS_H

#include "ok_types.h" // Vector2, bool

// State pushed from the UIKit view (ios_main.mm) into the C-facing queries the
// game reads through ok_types.h (GetScreenWidth / GetTouchPosition / …). All
// coordinates are in drawable pixels, matching what the renderer draws in.
void plat_ios_set_screen(int width_px, int height_px);
void plat_ios_set_touches(const Vector2* points_px, int count);
void plat_ios_post_gesture(int gesture);       // one of the GESTURE_* ids
void plat_ios_set_focus(bool focused);

#endif // OPENKLONDIKE_PLAT_IOS_H
