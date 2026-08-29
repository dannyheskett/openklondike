#ifndef OPENKLONDIKE_GFX_H
#define OPENKLONDIKE_GFX_H

#include "ok_types.h"

// Immediate-mode 2D drawing primitives -- the entire drawing surface the shared
// board renderer needs. Two backends implement this identically-behaving API:
//   gfx_raylib.c   -- wraps raylib (desktop / web / android). Behaviour-identical
//                     to the direct raylib calls it replaces.
//   ios/gfx_metal.mm -- native Metal (iOS), no raylib.
// render.c calls these instead of raylib directly, so the card art and the
// layout logic are shared and only the primitives are swapped per platform.
//
// The set is larger than a block game's would need because a playing card is a
// rounded rectangle carrying vector suit pips: rounded fills and outlines for
// the card bodies, circles for the club trefoil and the recycle hint, and raw
// triangles for the diamond / heart / spade fans.

#ifdef __cplusplus
extern "C" {
#endif

// Load the bundled UI font. Must be called once after the window / GL context
// exists (the backends also lazy-load on first text draw as a fallback).
void gfx_font_init(void);

void gfx_begin_frame(void);
void gfx_end_frame(void);
void gfx_clear(Color color);

void gfx_rect(int x, int y, int w, int h, Color color);
void gfx_rect_lines(int x, int y, int w, int h, Color color);
void gfx_line(int x1, int y1, int x2, int y2, Color color);

// Rounded rectangle, filled and stroked. `roundness` is raylib's: the corner
// radius as a fraction (0..1) of the shorter side, so a card keeps the same
// visual corner at any scale.
void gfx_rect_rounded(int x, int y, int w, int h, float roundness, Color color);
void gfx_rect_rounded_lines(int x, int y, int w, int h, float roundness, Color color);

void gfx_circle(float cx, float cy, float radius, Color color);
void gfx_circle_lines(float cx, float cy, float radius, Color color);

// A filled triangle. Winding-independent: the backend draws it whichever way the
// vertices are ordered, so the pip fans do not have to care about orientation.
void gfx_triangle(Vector2 a, Vector2 b, Vector2 c, Color color);

void gfx_text(const char* text, int x, int y, int font_size, Color color);
int  gfx_measure_text(const char* text, int font_size);

#ifdef __cplusplus
}
#endif

#endif // OPENKLONDIKE_GFX_H
