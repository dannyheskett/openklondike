#ifndef OPENKLONDIKE_SAFE_AREA_H
#define OPENKLONDIKE_SAFE_AREA_H

// Display-cutout geometry for the top edge, in device pixels. The Android
// Activity reads the window's DisplayCutout and pushes it here over JNI; the
// portrait renderer reads it to lay the title bar out around the front camera.
//
// All outputs are 0 when there is no cutout (and on every non-Android platform,
// where nothing ever sets them) -- callers then fall back to the plain
// full-width, centered title bar.
//
//   *top          height to keep clear at the top (safe-inset top)
//   *cutout_left  left x of the cutout's bounding box
//   *cutout_right right x of the cutout's bounding box (== left when absent)
void safe_area_get(int* top, int* cutout_left, int* cutout_right);

#endif // OPENKLONDIKE_SAFE_AREA_H
