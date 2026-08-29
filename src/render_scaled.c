// Touch layout: the board is fitted to the live screen, so the cards scale with
// the device instead of being a fixed size, and it re-fits on every rotation.
// Every metric -- gaps, fans, bar heights, font sizes -- is derived from the
// resulting card width at the same ratios the desktop board uses, so the two are
// the same design at different sizes. Compiles to an empty object off OK_SCALED.
//
// Two arrangements, chosen by the shape of the screen:
//
//   Upright   the classic one. Stock, waste, a gap and the four foundations in a
//             row across the top, seven tableau columns beneath.
//
//   Sideways  a phone held sideways has width to spare and very little height,
//             and a top row costs a whole card height of the scarce axis. So the
//             stock and waste go in a rail down the left, the four foundations
//             into a 2x2 block on the right, and the seven tableau columns take
//             the entire height between the bars. Ten card columns across rather
//             than seven, which on an iPhone 12 buys slightly larger cards AND
//             40% more tableau depth than the top row gave.
//
// Rails are chosen by aspect ratio, not merely by "wider than tall". They cost
// three extra columns of width, which only pays for itself when height is
// genuinely the scarce axis -- a phone on its side, at better than 1.6:1. A
// tablet turned sideways is only about 1.33:1 and has height to spare, so it
// keeps the top row and gets bigger cards than rails would have given it.
//
// Both fill the same Layout, so nothing downstream knows which one ran.
#include "render_internal.h"
#include "safe_area.h"

#ifdef OK_SCALED

// Ratios taken from the desktop layout's fixed numbers, so a scaled board keeps
// the proportions of the 80x112 original: gap 16/80, fans 28/80 and 12/80,
// waste spread 24/80.
#define R_GAP(cw)      ((cw) * 16 / 80)
#define R_FAN_UP(cw)   ((cw) * 28 / 80)
#define R_FAN_DOWN(cw) ((cw) * 12 / 80)
#define R_WASTE(cw)    ((cw) * 24 / 80)
#define R_ROW_GAP(cw)  ((cw) * 28 / 80)

// The board must stay usable on a small or short screen, so nothing below is
// allowed to collapse to zero.
#define MIN_CARD_W 24

// Chrome is sized from the SCREEN, never from the card. Sizing it off the card
// is circular -- a taller bar shrinks the card, which shrinks the bar -- and on
// a sideways phone it silently inflated the wordmark bar to 96px where these
// give 36. The formulas are openblocks' and openrackem's, so all three games
// wear the same furniture.
static int title_fs_of(int view_h)  { int fs = view_h / 45; return (fs < 10) ? 10 : fs; }
static int title_bar_of(int view_h) { int fs = title_fs_of(view_h); return fs + fs / 2; }
// The stats band follows the SHORT screen dimension, not the height. The board
// is fitted to the width, so on a tall phone a height-derived font came out
// enormous next to the cards -- 60px against a 132px card. The short side tracks
// the board on both orientations and leaves the sideways numbers unchanged.
static int hud_fs_of(int view_w, int view_h) {
    int shortd = (view_w < view_h) ? view_w : view_h;
    int fs = shortd / 38;
    return (fs < 9) ? 9 : fs;
}
// Two lines -- label over value -- exactly as openblocks' HUD band.
static int hud_h_of(int view_w, int view_h) {
    int fs = hud_fs_of(view_w, view_h);
    return 2 * fs + fs / 3;
}

// The wordmark bar, grown to clear a display cutout when the surface draws under
// one. iOS hands the game a viewport that already excludes the notch, so this
// only ever fires on Android.
static int top_bar_of(int view_h) {
    int bar = title_bar_of(view_h);
    int cut_top, cut_l, cut_r;
    safe_area_get(&cut_top, &cut_l, &cut_r);
    return (cut_top > bar) ? cut_top : bar;
}

// Fill in everything that follows from the card width alone.
static void metrics_from_card(Layout* L, int card_w, int margin) {
    L->card_w    = card_w;
    L->card_h    = card_w * CARD_H / CARD_W;
    L->col_gap   = imax(R_GAP(card_w), 2);
    L->fan_up    = imax(R_FAN_UP(card_w), 6);
    L->fan_down  = imax(R_FAN_DOWN(card_w), 3);
    L->waste_fan = imax(R_WASTE(card_w), 4);
    L->margin_x  = margin;
}

// Spread any unspent width into the column gaps, so the board spans the screen
// instead of huddling in the middle of it. Capped at half a card: past that the
// columns stop reading as one board.
static int spread_gap(int usable_w, int cols, int card_w, int gap) {
    int leftover = usable_w - cols * card_w - (cols - 1) * gap;
    if (leftover <= 0) return gap;
    int room = imax(card_w / 2 - gap, 0);
    int extra = leftover / (cols - 1);
    return gap + ((extra < room) ? extra : room);
}

Layout layout_scaled(int view_w, int view_h) {
    Layout L = {0};
    L.view_w = view_w;
    L.view_h = view_h;

    // Breathing room on every edge, proportional to the short screen dimension
    // so it stays sensible from a phone to a tablet.
    int shortd = (view_w < view_h) ? view_w : view_h;
    int margin = imax(shortd / 28, 6);
    int bar    = top_bar_of(view_h);
    int hud    = hud_h_of(view_w, view_h);

    L.titlebar_h = bar;
    L.hud_y      = bar + margin;
    L.hud_h      = hud;
    L.status_h   = 0;      // the touch board has no bottom bar; the HUD replaces it
    L.title_fs   = title_fs_of(view_h);
    L.status_fs  = hud_fs_of(view_w, view_h);

    // Everything below the HUD belongs to the board.
    int board_y = L.hud_y + hud + margin;
    int avail_h = view_h - board_y - margin;
    if (avail_h < 8) avail_h = 8;
    int usable_w = view_w - 2 * margin;

    // 1.6:1 or wider -- a phone on its side. Below that (a tablet, a squarish
    // window) the top row is the better use of the space.
    bool sideways = (view_h > 0) && (view_w * 10 >= view_h * 16);
    int cols  = sideways ? 10 : 7;      // rails add a column each side
    int card_w = usable_w * 10 / (cols * 10 + (cols - 1) * 2);

    if (sideways) {
        // The tableau owns the full height, so the card can only be as tall as
        // that -- and the foundations stack two high on the right, which is the
        // tighter of the two constraints.
        int by_height = avail_h * CARD_W / CARD_H;
        int by_found  = ((avail_h - imax(R_GAP(card_w), 2)) / 2) * CARD_W / CARD_H;
        if (by_height < card_w) card_w = by_height;
        if (by_found  < card_w) card_w = by_found;
    } else {
        // Upright: the top row plus a tableau column. Four card-heights keeps a
        // normal deal visible with no fan compression at all.
        for (int pass = 0; pass < 2; pass++) {
            int card_h = card_w * CARD_H / CARD_W;
            int need = 4 * card_h + R_ROW_GAP(card_w);
            if (need <= avail_h) break;
            card_w = ((avail_h - R_ROW_GAP(card_w)) / 4) * CARD_W / CARD_H;
        }
    }
    if (card_w < MIN_CARD_W) card_w = MIN_CARD_W;
    metrics_from_card(&L, card_w, margin);

    L.col_gap = spread_gap(usable_w, cols, L.card_w, L.col_gap);
    int content = cols * L.card_w + (cols - 1) * L.col_gap;
    int left = (view_w - content) / 2;
    if (left < 0) left = 0;
    int step = L.card_w + L.col_gap;

    if (sideways) {
        // col 0        stock (top) and waste (below it)
        // cols 1..7    the seven tableau columns
        // cols 8..9    the four foundations, 2x2
        L.stock_x = left;               L.stock_y = board_y;
        L.waste_x = left;               L.waste_y = board_y + L.card_h + L.col_gap;
        for (int c = 0; c < 7; c++) L.tab_x[c] = left + (c + 1) * step;
        for (int f = 0; f < 4; f++) {
            L.found_x[f] = left + (8 + (f % 2)) * step;
            L.found_y[f] = board_y + (f / 2) * (L.card_h + L.col_gap);
        }
        L.tab_y = board_y;
    } else {
        // Stock, waste, a gap, then the four foundations, above the tableau.
        for (int c = 0; c < 7; c++) L.tab_x[c] = left + c * step;
        L.stock_x = L.tab_x[0]; L.stock_y = board_y;
        L.waste_x = L.tab_x[1]; L.waste_y = board_y;
        for (int f = 0; f < 4; f++) { L.found_x[f] = L.tab_x[3 + f]; L.found_y[f] = board_y; }
        L.tab_y = board_y + L.card_h + R_ROW_GAP(L.card_w);
    }
    L.tab_bottom = view_h - margin;

    // Touch targets. Whatever vertical room the tableau did not need goes into
    // the face-up fan: a fingertip needs a bigger target than the 28/80 sliver a
    // mouse can hit. Capped at two fifths of a card, past which a run stops
    // reading as a stacked run. Face-down cards are not draggable, so their fan
    // stays tight.
    int room   = L.tab_bottom - L.tab_y - L.card_h;
    int spread = (room - 3 * L.fan_down) / 5;
    int cap    = L.card_h * 2 / 5;
    if (spread > cap)      spread = cap;
    if (spread > L.fan_up) L.fan_up = spread;

    return L;
}

#endif // OK_SCALED
