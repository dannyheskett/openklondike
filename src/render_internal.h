#ifndef OPENKLONDIKE_RENDER_INTERNAL_H
#define OPENKLONDIKE_RENDER_INTERNAL_H

// Private interface shared between the renderer translation units:
//   render.c           -- colours, card art, board/menu drawing, hit testing,
//                         the win cascade, lifecycle, and layout dispatch
//   render_fixed.c     -- the fixed-size desktop layout  (body under OK_FIXED)
//   render_scaled.c    -- the adaptive touch layout      (body under OK_SCALED)
//
// "fixed" and "scaled" describe the board, not the device orientation: the
// scaled board is what phones use, and it runs in either orientation.
//
// The two layout files supply nothing but geometry: every pixel is drawn by the
// shared code in render.c, reading the metrics out of a Layout. That is what
// makes the scaled touch board and the fixed desktop board the same renderer
// rather than two that must be kept in sync.
//
// render_fixed.c / render_scaled.c compile to empty objects on the platforms
// that don't use them, so both can sit in the build's source list
// unconditionally.

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

    // Pile positions, given explicitly rather than as indices into one row of
    // columns. The fixed and portrait boards put stock, waste and the four
    // foundations in a row above the tableau; the landscape board puts stock and
    // waste in a rail down the left and the foundations in a 2x2 block on the
    // right, so the tableau gets the whole height. Both are just different fills
    // of these fields, and every consumer -- drawing, hit testing, drop
    // targeting -- reads them without caring which arrangement produced them.
    int stock_x,  stock_y;
    int waste_x,  waste_y;
    int found_x[4], found_y[4];
    int tab_x[7];         // x of each tableau column
    int tab_y;            // y where the tableau fans begin
    int tab_bottom;       // y the tableau must not grow past

    int fan_up;           // vertical overlap between face-up tableau cards
    int fan_down;         // tighter overlap for face-down cards
    int waste_fan;        // horizontal spread of the three drawn waste cards

    int titlebar_h;       // top bar carrying the wordmark
    int hud_h;            // stats band under it (0 on the fixed desktop board)
    int hud_y;            // top of that band
    int status_h;         // bottom bar carrying score/time/moves (fixed board only)
    int margin_x;         // side margin
    int title_fs;         // wordmark font size
    int status_fs;        // stats font size
} Layout;

// Board colours (defined in render.c), shared with the layout files.
extern const Color FELT;
extern const Color FELT_DARK;
extern const Color SLOT_LINE;
extern const Color TEXT_LIGHT;

// Per-layout geometry (defined in render_fixed.c / render_scaled.c).
#ifdef OK_FIXED
Layout layout_fixed(int view_w, int view_h);
#endif
#ifdef OK_SCALED
Layout layout_scaled(int view_w, int view_h);
#endif

#endif // OPENKLONDIKE_RENDER_INTERNAL_H
