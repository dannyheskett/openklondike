// Desktop layout: fixed 80x112 cards on a board that is laid out at a fixed
// pixel size and centred in the window. Resizing the window moves the margins,
// never the cards. Compiles to an empty object off OK_LANDSCAPE.
#include "render_internal.h"

#ifdef OK_LANDSCAPE

#define MARGIN_X   24
#define MARGIN_TOP 24
#define COL_GAP    16
#define ROW_GAP    28
#define FAN_UP     28   // vertical overlap between face-up tableau cards
#define FAN_DOWN   12   // tighter overlap for face-down cards
#define WASTE_FAN  24   // horizontal spread of the 3 drawn waste cards
#define TITLEBAR_H 44   // top bar carrying the game name
#define STATUS_H   28   // bottom bar carrying score / time / moves
#define TITLE_FS   22
#define STATUS_FS  18

#define CONTENT_W  (7 * CARD_W + 6 * COL_GAP)   // 7-column board width

Layout layout_landscape(int view_w, int view_h) {
    // The cards never scale while the viewport honours the minimum size, and the
    // desktop builds enforce that minimum on the window itself (SetWindowMinSize
    // in render_init). The web build cannot: a browser window is whatever the
    // user drags it to. So rather than run the board off the edge, shrink it to
    // fit. The factor is clamped at 1.0, so this only ever reduces below the
    // minimum and never enlarges the fixed size on a big display.
    float s = 1.0f;
    if (view_w < MIN_W) s = (float)view_w / MIN_W;
    if (view_h < MIN_H) { float sy = (float)view_h / MIN_H; if (sy < s) s = sy; }
    if (s < 0.25f) s = 0.25f;   // past this the card indices are unreadable anyway
#define S(v) ((int)((v) * s + 0.5f))

    Layout L;
    L.view_w     = view_w;
    L.view_h     = view_h;
    L.card_w     = S(CARD_W);
    L.card_h     = S(CARD_H);
    L.col_gap    = S(COL_GAP);
    L.fan_up     = S(FAN_UP);
    L.fan_down   = S(FAN_DOWN);
    L.waste_fan  = S(WASTE_FAN);
    L.titlebar_h = S(TITLEBAR_H);
    L.status_h   = S(STATUS_H);
    L.margin_x   = S(MARGIN_X);
    L.title_fs   = S(TITLE_FS);
    L.status_fs  = S(STATUS_FS);

    int content_w = 7 * L.card_w + 6 * L.col_gap;
    int left = (view_w - content_w) / 2;
    if (left < L.margin_x) left = L.margin_x;
    for (int c = 0; c < 7; c++) L.col_x[c] = left + c * (L.card_w + L.col_gap);

    L.top_y = L.titlebar_h + S(MARGIN_TOP);
    L.tab_y = L.top_y + L.card_h + S(ROW_GAP);
    return L;
#undef S
}

#endif // OK_LANDSCAPE
