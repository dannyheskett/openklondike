#ifndef SAFE_AREA_H
#define SAFE_AREA_H

// Display-cutout and system-bar insets, in device pixels. The Android Activity
// reads the window insets and pushes them here over JNI; the layout keeps the
// board clear of them.
//
// All four edges matter because this game rotates: in landscape the camera
// cutout and the gesture bar move to a side edge, where a top-only inset (what
// the portrait games in this family carry) would not protect anything.
//
// Every field is 0 when there is no inset, and on every non-Android platform,
// where nothing sets them: iOS already hands the game a viewport with the safe
// area subtracted (ios/ios_main.mm), and desktop and web have no insets.
typedef struct {
    int top, bottom, left, right;   // safe insets to keep clear
    int cutout_left, cutout_right;  // top cutout bounding box (== when absent)
} SafeArea;

SafeArea safe_area_get(void);

#endif // SAFE_AREA_H
