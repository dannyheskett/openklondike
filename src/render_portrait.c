// Touch layout: the seven tableau columns are fitted to the live screen, so the
// cards scale with the device instead of being a fixed size. Every other metric
// -- gaps, fans, bar heights, font sizes -- is derived from the resulting card
// width at the same ratios the desktop layout uses, so the two boards are the
// same design at different sizes. Compiles to an empty object off OK_PORTRAIT.
#include "render_internal.h"
#include "safe_area.h"

#ifdef OK_PORTRAIT

// Ratios taken from the desktop layout's fixed numbers, so a scaled board keeps
// the proportions of the 80x112 original: gap 16/80, fans 28/80 and 12/80,
// waste spread 24/80, wordmark 22/80, status text 18/80.
#define R_GAP(cw)       ((cw) * 16 / 80)
#define R_FAN_UP(cw)    ((cw) * 28 / 80)
#define R_FAN_DOWN(cw)  ((cw) * 12 / 80)
#define R_WASTE(cw)     ((cw) * 24 / 80)
#define R_TITLE_FS(cw)  ((cw) * 22 / 80)
#define R_STATUS_FS(cw) ((cw) * 18 / 80)
#define R_ROW_GAP(cw)   ((cw) * 28 / 80)

// The board must stay usable on a small or short screen, so nothing below is
// allowed to collapse to zero.
#define MIN_CARD_W 24

Layout layout_portrait(int view_w, int view_h) {
    // Breathing room on every edge, proportional to the short screen dimension
    // so it stays sensible from a phone to a tablet.
    int shortd = (view_w < view_h) ? view_w : view_h;
    int margin = shortd / 28;
    if (margin < 6) margin = 6;

    // Width pass: seven columns and six gaps span the screen inside the margins.
    // The gap is a fifth of a card (R_GAP), so
    //   7*cw + 6*(cw/5) = usable  =>  8.2*cw = usable  =>  cw = usable*10/82.
    // Getting this divisor wrong does not fail loudly: the columns simply come
    // out too wide, `left` clamps to zero, and the board runs edge to edge with
    // no margin at all. test_layout.c asserts the margins survive.
    int usable = view_w - 2 * margin;
    int card_w = usable * 10 / 82;

    // Height pass. A column needs room for the top row plus a tableau fan below
    // it; a typical column is the card plus eight fan steps, which at these
    // ratios is almost exactly two more card heights. Requiring four card
    // heights of playable space keeps a normal deal fully visible, and shrinks
    // the cards on a short screen (a phone browser held sideways) instead of
    // burying the tableau off the bottom edge. Deeper columns than that are
    // handled by compressing the fan at draw time.
    for (int pass = 0; pass < 2; pass++) {
        int card_h = card_w * CARD_H / CARD_W;
        int bar    = imax(2 * R_TITLE_FS(card_w), 1);
        int top    = imax(bar, 0);
        int tcut, cl, cr;
        safe_area_get(&tcut, &cl, &cr);
        if (tcut > top) top = tcut;
        int status = R_STATUS_FS(card_w) * 14 / 9;
        int budget = view_h - top - margin - R_ROW_GAP(card_w) - status;
        if (budget < 4) budget = 4;
        if (4 * card_h > budget) {
            card_h = budget / 4;
            card_w = card_h * CARD_W / CARD_H;
        } else {
            break;   // it already fits; the second pass would change nothing
        }
    }
    if (card_w < MIN_CARD_W) card_w = MIN_CARD_W;

    Layout L;
    L.view_w    = view_w;
    L.view_h    = view_h;
    L.card_w    = card_w;
    L.card_h    = card_w * CARD_H / CARD_W;
    L.col_gap   = imax(R_GAP(card_w), 2);
    L.fan_up    = imax(R_FAN_UP(card_w), 6);
    L.fan_down  = imax(R_FAN_DOWN(card_w), 3);
    L.waste_fan = imax(R_WASTE(card_w), 4);
    L.title_fs  = imax(R_TITLE_FS(card_w), 10);
    L.status_fs = imax(R_STATUS_FS(card_w), 9);
    L.status_h  = L.status_fs * 14 / 9;
    L.margin_x  = margin;

    // A thin wordmark bar, grown to clear the display cutout (front camera) when
    // the surface draws under it -- so neither the wordmark nor the board below
    // it ever sits beneath the notch.
    L.titlebar_h = 2 * L.title_fs;
    int cut_top, cut_l, cut_r;
    safe_area_get(&cut_top, &cut_l, &cut_r);
    if (cut_top > L.titlebar_h) L.titlebar_h = cut_top;

    // Centre the seven columns in whatever width the final card size leaves.
    int content_w = 7 * L.card_w + 6 * L.col_gap;
    int left = (view_w - content_w) / 2;
    if (left < 0) left = 0;
    for (int c = 0; c < 7; c++) L.col_x[c] = left + c * (L.card_w + L.col_gap);

    L.top_y = L.titlebar_h + margin;
    L.tab_y = L.top_y + L.card_h + R_ROW_GAP(L.card_w);

    // Touch targets. A phone screen is far taller than a seven-column board
    // needs, and the leftover height is better spent making each card in a fan
    // its own comfortable target than left as bare felt -- a fingertip is much
    // wider than the 28/80 sliver the desktop layout exposes. Spread the
    // face-up fan to fill what a busy column (three face-down under six face-up)
    // would occupy, never tighter than the desktop ratio and never past two
    // fifths of a card, beyond which a run stops reading as a stacked run.
    // Face-down cards are not draggable, so their fan stays tight.
    int avail  = L.view_h - L.status_h - L.tab_y - L.card_h;
    int spread = (avail - 3 * L.fan_down) / 5;
    int cap    = L.card_h * 2 / 5;
    if (spread > cap)      spread = cap;
    if (spread > L.fan_up) L.fan_up = spread;

    return L;
}

#endif // OK_PORTRAIT
