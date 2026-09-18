#ifndef GFX_H
#define GFX_H

#include "ok_types.h"

// Immediate-mode 2D drawing primitives: the entire drawing surface the games
// in this family use. This file is identical in every game. Two backends
// implement it with identical behaviour:
//   gfx_raylib.c     -- wraps raylib (desktop / web / Android).
//   ios/gfx_metal.mm -- native Metal (iOS), no raylib.
// The renderers call these instead of raylib directly, so all drawing code is
// shared and only the primitives are swapped per platform.

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
// Vertical gradient from `top` to `bottom`.
void gfx_rect_gradient_v(int x, int y, int w, int h, Color top, Color bottom);

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

#endif // GFX_H
