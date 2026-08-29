// Touch layout: the board is fitted to the live screen, so the cards scale with
// the device instead of being a fixed size, and it re-fits on every rotation.
// Every metric -- gaps, fans, font sizes -- is derived from the resulting card
// width at the same ratios the desktop board uses, so the two are the same
// design at different sizes. Compiles to an empty object off OK_SCALED.
//
// One arrangement, in both orientations: stock, waste, a gap, then the four
// foundations across the top, with the seven tableau columns in the same
// seven-column grid beneath. That is what every established Klondike does --
// World of Solitaire and Green Felt both lay a landscape board out exactly this
// way -- and it is how players read the board. An earlier cut moved stock and
// waste into a left rail and the foundations into a 2x2 block on the right to
// buy the tableau a whole card height; it did buy it, and it looked wrong, which
// is the more important measurement.
//
// The space a landscape phone leaves below the board is not a bug to design
// away. A seven-column board on a 2:1 screen does not fill it, and the
// references leave the same gap. Bigger cards and quieter chrome are the honest
// answer; a cleverer arrangement is not.
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

// How many card heights of tableau to reserve below the top row. Upright there
// is height to spare, so ask for enough that a normal deal never compresses.
// Sideways there is not: asking for the same shrinks the cards until they are
// unreadable, so ask for less and let the draw-time fan compression cover the
// deep columns, which is what it is for.
#define TABLEAU_HEIGHTS_UPRIGHT  4
#define TABLEAU_HEIGHTS_SIDEWAYS 22   // tenths, i.e. 2.2

// Chrome is sized from the SCREEN, never from the card. Sizing it off the card
// is circular -- a taller bar shrinks the card, which shrinks the bar -- and on
// a sideways phone it silently inflated the wordmark bar to 96px where this
// gives 36.
static int title_fs_of(int view_h)  { int fs = view_h / 45; return (fs < 10) ? 10 : fs; }
static int title_bar_of(int view_h) { int fs = title_fs_of(view_h); return fs + fs / 2; }

// The stats are a footnote, not a scoreboard. Every reference implementation
// keeps them to one quiet line of small text; an earlier cut gave them
// openblocks' two-line label-over-value band, which suits a game whose numbers
// are the main feedback and shouts in one whose numbers are incidental.
static int stats_fs_of(int view_w, int view_h) {
    int shortd = (view_w < view_h) ? view_w : view_h;
    int fs = shortd / 46;
    return (fs < 9) ? 9 : fs;
}
static int stats_h_of(int view_w, int view_h) {
    return stats_fs_of(view_w, view_h) * 3 / 2;
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

Layout layout_scaled(int view_w, int view_h) {
    Layout L = {0};
    L.view_w = view_w;
    L.view_h = view_h;

    // Breathing room on every edge, proportional to the short screen dimension
    // so it stays sensible from a phone to a tablet.
    int shortd = (view_w < view_h) ? view_w : view_h;
    int margin = imax(shortd / 28, 6);

    L.titlebar_h = top_bar_of(view_h);
    L.title_fs   = title_fs_of(view_h);
    L.status_fs  = stats_fs_of(view_w, view_h);
    L.hud_h      = stats_h_of(view_w, view_h);
    L.hud_y      = L.titlebar_h + margin / 2;
    L.status_h   = 0;   // no bottom bar on touch: it lands on the home indicator

    int board_y  = L.hud_y + L.hud_h + margin;
    int avail_h  = imax(view_h - board_y - margin, 8);
    int usable_w = view_w - 2 * margin;

    // Width pass: seven cards and six gaps of a fifth of a card each.
    int card_w = usable_w * 10 / 82;

    // Height pass: the top row plus the tableau reservation. Two passes, because
    // shrinking the card shrinks the row gap too and frees a little back.
    int tenths = (view_w > view_h) ? TABLEAU_HEIGHTS_SIDEWAYS
                                   : TABLEAU_HEIGHTS_UPRIGHT * 10;
    for (int pass = 0; pass < 2; pass++) {
        int card_h = card_w * CARD_H / CARD_W;
        int need = card_h + card_h * tenths / 10 + R_ROW_GAP(card_w);
        if (need <= avail_h) break;
        card_w = ((avail_h - R_ROW_GAP(card_w)) * 10 / (10 + tenths)) * CARD_W / CARD_H;
    }
    if (card_w < MIN_CARD_W) card_w = MIN_CARD_W;

    L.card_w    = card_w;
    L.card_h    = card_w * CARD_H / CARD_W;
    L.col_gap   = imax(R_GAP(card_w), 2);
    L.fan_up    = imax(R_FAN_UP(card_w), 6);
    L.fan_down  = imax(R_FAN_DOWN(card_w), 3);
    L.waste_fan = imax(R_WASTE(card_w), 4);
    L.margin_x  = margin;

    // Any width the height pass left unspent goes into the column gaps, so the
    // board spans the screen rather than huddling in the middle of it. Capped at
    // a third of a card: past that the columns stop reading as one board, and
    // the references all keep their columns close.
    int leftover = usable_w - 7 * L.card_w - 6 * L.col_gap;
    if (leftover > 0) {
        int room = imax(L.card_w / 3 - L.col_gap, 0);
        int extra = leftover / 6;
        L.col_gap += (extra < room) ? extra : room;
    }

    int content = 7 * L.card_w + 6 * L.col_gap;
    int left = (view_w - content) / 2;
    if (left < 0) left = 0;
    int step = L.card_w + L.col_gap;

    // Top row: stock, waste, a gap, then the four foundations -- columns 0, 1
    // and 3..6 of the same grid the tableau uses.
    for (int c = 0; c < 7; c++) L.tab_x[c] = left + c * step;
    L.stock_x = L.tab_x[0]; L.stock_y = board_y;
    L.waste_x = L.tab_x[1]; L.waste_y = board_y;
    for (int f = 0; f < 4; f++) { L.found_x[f] = L.tab_x[3 + f]; L.found_y[f] = board_y; }

    L.tab_y      = board_y + L.card_h + R_ROW_GAP(L.card_w);
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
