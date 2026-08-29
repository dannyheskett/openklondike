#ifndef OPENKLONDIKE_GFX_METAL_H
#define OPENKLONDIKE_GFX_METAL_H

#import <QuartzCore/CAMetalLayer.h>

// Wire the Metal backend to the view's layer, and tell it the drawable size in
// pixels. Called from the iOS app shell (ios_main.mm). The gfx_* primitives
// (gfx.h) are implemented on top of this in gfx_metal.mm.
void gfx_metal_attach(CAMetalLayer* layer);
// Full drawable size (px) plus the origin (px) the game's (0,0) maps to — the
// safe-area top-left, so drawing sits below the notch / above the home indicator.
void gfx_metal_set_viewport(int full_w, int full_h, int origin_x, int origin_y);

#endif // OPENKLONDIKE_GFX_METAL_H
