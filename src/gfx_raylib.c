// raylib backend for the gfx primitive layer: each entry point is a thin wrapper
// over the raylib call it replaces, so desktop / web / android rendering is
// identical across those platforms. iOS uses ios/gfx_metal.mm instead and never
// compiles this file.
//
// Two entry points do more than forward:
//   gfx_text / gfx_measure_text draw with the bundled Nunito font (loaded from
//     the embedded TTF) via DrawTextEx rather than raylib's built-in pixel font,
//     so every platform lays text out identically.
//   gfx_triangle draws both windings, because raylib's DrawTriangle is
//     back-face culled and the pip outlines are generated in either order.
#include "gfx.h"
#include "font_nunito.h"
#include <raylib.h>
#include <stddef.h>  // NULL

// Corner segments for the rounded card outlines. Six is what the original
// direct raylib calls used and is smooth at every card size the game reaches.
#define GFX_ROUND_SEGMENTS 6

// Bake the glyph atlas well above the largest on-screen size (menu titles reach
// ~46px) so every draw downsamples -- crisp with bilinear filtering.
#define GFX_FONT_BAKE 64

static Font s_font;
static bool s_font_ready = false;

// A hair of inter-glyph tracking, proportional to size, improves legibility
// without looking loose. gfx_text and gfx_measure_text MUST agree so centering
// stays correct. The iOS backend uses the same ratio.
static float text_spacing(float fs) { return fs * 0.05f; }

static void ensure_font(void) {
    if (s_font_ready || !IsWindowReady()) return;
    s_font = LoadFontFromMemory(".ttf", nunito_ttf, (int)nunito_ttf_len,
                                GFX_FONT_BAKE, NULL, 0);
    SetTextureFilter(s_font.texture, TEXTURE_FILTER_BILINEAR);
    s_font_ready = true;
}

void gfx_font_init(void) { ensure_font(); }

void gfx_begin_frame(void)  { BeginDrawing(); }
void gfx_end_frame(void)    { EndDrawing(); }
void gfx_clear(Color color) { ClearBackground(color); }

void gfx_rect(int x, int y, int w, int h, Color color) {
    DrawRectangle(x, y, w, h, color);
}
void gfx_rect_lines(int x, int y, int w, int h, Color color) {
    DrawRectangleLines(x, y, w, h, color);
}
void gfx_line(int x1, int y1, int x2, int y2, Color color) {
    DrawLine(x1, y1, x2, y2, color);
}

void gfx_rect_rounded(int x, int y, int w, int h, float roundness, Color color) {
    DrawRectangleRounded((Rectangle){(float)x, (float)y, (float)w, (float)h},
                         roundness, GFX_ROUND_SEGMENTS, color);
}
void gfx_rect_rounded_lines(int x, int y, int w, int h, float roundness, Color color) {
    DrawRectangleRoundedLines((Rectangle){(float)x, (float)y, (float)w, (float)h},
                              roundness, GFX_ROUND_SEGMENTS, color);
}

void gfx_circle(float cx, float cy, float radius, Color color) {
    DrawCircleV((Vector2){cx, cy}, radius, color);
}
void gfx_circle_lines(float cx, float cy, float radius, Color color) {
    DrawCircleLinesV((Vector2){cx, cy}, radius, color);
}

void gfx_triangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
    // raylib culls back faces, and the pip outlines are traced in whichever
    // direction the parametric curve runs, so emit the triangle both ways.
    DrawTriangle(a, b, c, color);
    DrawTriangle(a, c, b, color);
}

void gfx_text(const char* text, int x, int y, int font_size, Color color) {
    ensure_font();
    if (!s_font_ready) { DrawText(text, x, y, font_size, color); return; }
    DrawTextEx(s_font, text, (Vector2){(float)x, (float)y},
               (float)font_size, text_spacing((float)font_size), color);
}
int gfx_measure_text(const char* text, int font_size) {
    ensure_font();
    if (!s_font_ready) return MeasureText(text, font_size);
    return (int)(MeasureTextEx(s_font, text, (float)font_size,
                               text_spacing((float)font_size)).x + 0.5f);
}
