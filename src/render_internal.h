#ifndef OPENKLONDIKE_RENDER_INTERNAL_H
#define OPENKLONDIKE_RENDER_INTERNAL_H

// Private interface shared between the renderer translation units:
//   render.c           -- colours, card art, board/menu drawing, hit testing,
//                         the win cascade, lifecycle, and layout dispatch
//   render_landscape.c -- the fixed-size desktop layout   (body under OK_LANDSCAPE)
//   render_portrait.c  -- the adaptive touch layout       (body under OK_PORTRAIT)
//
// The two layout files supply nothing but geometry: every pixel is drawn by the
// shared code in render.c, reading the metrics out of a Layout. That is what
// makes the scaled touch board and the fixed desktop board the same renderer
// rather than two that must be kept in sync.
//
// render_portrait/landscape.c compile to empty objects on platforms that don't
// use them, so both can sit in the build's source list unconditionally.

#include "render.h"
#include "gfx.h"

// Shared by every renderer TU (and so defined once, here: the layout tests
// compile several of those translation units into a single binary).
static inline int imax(int a, int b) { return a > b ? a : b; }

// Every metric the board renderer needs, in pixels, for one viewport. All of it
// is derived from the card size, so scaling the cards scales the whole board.
typedef struct {
    int view_w, view_h;   // viewport this layout was computed for
    int card_w, card_h;
    int col_gap;          // horizontal gap between columns
    int col_x[7];         // x of each column / top-row slot
    int top_y;            // y of the top row (stock / waste / foundations)
    int tab_y;            // y where the tableau fans begin
    int fan_up;           // vertical overlap between face-up tableau cards
    int fan_down;         // tighter overlap for face-down cards
    int waste_fan;        // horizontal spread of the three drawn waste cards
    int titlebar_h;       // top bar carrying the game name
    int status_h;         // bottom bar carrying score / time / moves
    int margin_x;         // side margin used by the status bar text
    int title_fs;         // wordmark font size
    int status_fs;        // status bar font size
} Layout;

// Top-row slot indices map onto tableau columns:
//   col 0 = stock, col 1 = waste, col 2 = gap, cols 3..6 = foundations 0..3.
#define SLOT_STOCK   0
#define SLOT_WASTE   1
#define SLOT_FOUND0  3

// Board colours (defined in render.c), shared with the layout files.
extern const Color FELT;
extern const Color FELT_DARK;
extern const Color SLOT_LINE;
extern const Color TEXT_LIGHT;

// Per-layout geometry (defined in render_landscape.c / render_portrait.c).
#ifdef OK_LANDSCAPE
Layout layout_landscape(int view_w, int view_h);
#endif
#ifdef OK_PORTRAIT
Layout layout_portrait(int view_w, int view_h);
#endif

#endif // OPENKLONDIKE_RENDER_INTERNAL_H
