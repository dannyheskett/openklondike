// Board renderer. Every pixel the game draws is produced here, through the gfx
// primitive layer (gfx.h) rather than raylib directly, so the same card art and
// the same layout logic run on the raylib backends and on the native Metal
// backend the iOS build uses.
//
// All geometry comes out of a Layout (render_internal.h), which the two layout
// translation units compute: render_landscape.c for the fixed-size desktop board
// and render_portrait.c for the touch board that scales its cards to the screen.
// Nothing below hardcodes a card size, so the two are the same renderer.
#include "render_internal.h"
#include "safe_area.h"
#include "recorder.h"
#include "tick.h"   // SIM_HZ: the status clock counts fixed simulation steps
#if !defined(PLATFORM_IOS)
#include <raylib.h>  // window/timing/render textures; absent on iOS
#include <rlgl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// --------------------------------------------------------------------------
// Colors
// --------------------------------------------------------------------------
const Color FELT              = { 12,  92,  52, 255};   // classic green table
const Color FELT_DARK         = { 10,  76,  44, 255};
const Color SLOT_LINE         = { 30, 110,  66, 255};
const Color TEXT_LIGHT        = {235, 235, 225, 255};
static const Color CARD_FACE  = {248, 248, 242, 255};
static const Color CARD_EDGE  = { 40,  40,  40, 255};
static const Color CARD_BACK  = { 36,  72, 156, 255};
static const Color CARD_BACK2 = { 80, 130, 220, 255};
static const Color SLOT_FILL  = { 14,  84,  48, 255};
static const Color RED_PIP    = {200,  30,  40, 255};
static const Color BLACK_PIP  = { 20,  20,  24, 255};
static const Color HILITE     = {255, 235, 120, 255};
static const Color MENU_BG    = { 16,  40,  28, 255};
static const Color TEXT_DIM   = {170, 190, 175, 255};

// Corner radius as a fraction of the card's short side. Scale-invariant, so a
// 40px touch card and an 80px desktop card have the same silhouette.
#define CARD_ROUND 0.12f

// --------------------------------------------------------------------------
// Layout selection
// --------------------------------------------------------------------------
// render_use_portrait() reports the active layout. Native builds have exactly
// one (compile-time constant); the web build has both and picks at runtime.
#if defined(OK_RUNTIME_RENDERER)
static bool s_portrait_mode = false;
void render_set_portrait(bool portrait) { s_portrait_mode = portrait; }
bool render_use_portrait(void) { return s_portrait_mode; }
#elif defined(OK_PORTRAIT)
void render_set_portrait(bool portrait) { (void)portrait; }
bool render_use_portrait(void) { return true; }   // Android / iOS: touch only
#else
void render_set_portrait(bool portrait) { (void)portrait; }
bool render_use_portrait(void) { return false; }  // desktop native: fixed only
#endif

static Layout layout_for(int view_w, int view_h) {
#if defined(OK_RUNTIME_RENDERER)
    return s_portrait_mode ? layout_portrait(view_w, view_h)
                           : layout_landscape(view_w, view_h);
#elif defined(OK_PORTRAIT)
    return layout_portrait(view_w, view_h);
#else
    return layout_landscape(view_w, view_h);
#endif
}

// The layout for the live window, which every hit test works against.
static Layout live_layout(void) {
    return layout_for(GetScreenWidth(), GetScreenHeight());
}

int render_card_width(void) { return live_layout().card_w; }

// --------------------------------------------------------------------------
// Tableau fan geometry
// --------------------------------------------------------------------------
// Fills ys[] with the top y of every card in a tableau column and returns the
// column's bottom edge. A deep column -- up to six face-down cards under a
// thirteen-card run -- is taller than the board at the natural fan spacing, so
// the fan is compressed uniformly until the column fits between tab_y and the
// status bar. Every caller (drawing, hit testing, drop targeting) goes through
// here, so they can never disagree about where a card actually is.
static int tab_card_ys(const Layout* L, const Pile* p, int* ys) {
    if (p->count == 0) return L->tab_y + L->card_h;

    int avail = L->view_h - L->status_h - L->tab_y - L->card_h;
    if (avail < 0) avail = 0;

    int span = 0;   // natural distance from the first card's top to the last's
    for (int i = 0; i + 1 < p->count; i++)
        span += p->cards[i].face_up ? L->fan_up : L->fan_down;

    // Compress only when the column would overflow. The floor keeps a face-down
    // card visible as a distinct sliver even in the deepest column.
    int min_step = imax(L->card_h / 28, 3);
    int y = L->tab_y;
    for (int i = 0; i < p->count; i++) {
        ys[i] = y;
        int step = p->cards[i].face_up ? L->fan_up : L->fan_down;
        if (span > avail && span > 0) step = imax(step * avail / span, min_step);
        y += step;
    }
    return ys[p->count - 1] + L->card_h;
}

// --------------------------------------------------------------------------
// Card art (all vector-drawn — no asset files)
// --------------------------------------------------------------------------
static const char* RANK_STR[14] = {
    "", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"
};

// Fill a closed polygon as a triangle fan from `c`. Works for any polygon that
// is star-shaped about `c` (true for all of our pip shapes). gfx_triangle is
// winding-independent, so the traced direction does not matter.
static void fill_fan(Vector2 c, const Vector2* p, int n, Color col) {
    for (int i = 0; i < n; i++) gfx_triangle(c, p[i], p[(i + 1) % n], col);
}

#define PIP_SEG 30

// Build a heart outline of total height `s` centred on (cx,cy) into `out`.
// `xsquash` scales the width independently (1.0 = the curve's natural ~1.1:1
// width:height; <1 makes it taller and more upright). flip=true points it
// upward (the spade body). Classic heart curve:
//   x = 16 sin³t,  y = 13 cos t − 5 cos 2t − 2 cos 3t − cos 4t
static void heart_outline(float cx, float cy, float s, float xsquash, bool flip,
                          Vector2* out) {
    float xs[PIP_SEG], ys[PIP_SEG];
    float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
    for (int i = 0; i < PIP_SEG; i++) {
        float t = (float)i / PIP_SEG * 2.0f * PI;
        float st = sinf(t);
        float x = 16.0f * st * st * st;
        float y = -(13.0f * cosf(t) - 5.0f * cosf(2*t) - 2.0f * cosf(3*t) - cosf(4*t));
        xs[i] = x; ys[i] = y;
        if (x < minx) minx = x;
        if (x > maxx) maxx = x;
        if (y < miny) miny = y;
        if (y > maxy) maxy = y;
    }
    float sc = s / (maxy - miny);
    float mx = (minx + maxx) * 0.5f, my = (miny + maxy) * 0.5f;
    for (int i = 0; i < PIP_SEG; i++) {
        float nx = (xs[i] - mx) * sc * xsquash;
        float ny = (ys[i] - my) * sc;
        if (flip) ny = -ny;
        out[i].x = cx + nx;
        out[i].y = cy + ny;
    }
}

// A flared pedestal/stem under the spade and club: a narrow neck widening to
// outward-kicked feet, like the base on a real card pip.
static void draw_stem(float cx, float topy, float s, Color col) {
    float nw = s * 0.05f;   // neck half-width
    float fw = s * 0.34f;   // foot half-width (flares past the body lobes)
    float h  = s * 0.26f;
    Vector2 tl = {cx - nw, topy},          tr = {cx + nw, topy};
    Vector2 ml = {cx - nw * 1.4f, topy + h * 0.55f};
    Vector2 mr = {cx + nw * 1.4f, topy + h * 0.55f};
    Vector2 bl = {cx - fw, topy + h},      br = {cx + fw, topy + h};
    gfx_triangle(tl, ml, mr, col);   // neck
    gfx_triangle(tl, mr, tr, col);
    gfx_triangle(ml, bl, br, col);   // flared foot
    gfx_triangle(ml, br, mr, col);
}

// Draw one suit pip centred at (cx,cy) with overall height s.
static void draw_pip(float cx, float cy, float s, int suit) {
    Color col = (suit == 1 || suit == 2) ? RED_PIP : BLACK_PIP;
    Vector2 pts[PIP_SEG];
    switch (suit) {
        case 1: { // diamond — a filled rhombus, taller than wide
            float hw = s * 0.34f, hh = s * 0.5f;
            Vector2 p[4] = {{cx, cy - hh}, {cx + hw, cy}, {cx, cy + hh}, {cx - hw, cy}};
            fill_fan((Vector2){cx, cy}, p, 4, col);
            break;
        }
        case 2: { // heart — upright, slightly taller than wide
            heart_outline(cx, cy, s, 0.86f, false, pts);
            fill_fan((Vector2){cx, cy + s * 0.10f}, pts, PIP_SEG, col);
            break;
        }
        case 3: { // spade — a narrow upward heart on a flared pedestal
            float body = s * 0.74f;
            float byc  = cy - s * 0.10f;
            heart_outline(cx, byc, body, 0.96f, true, pts);
            fill_fan((Vector2){cx, byc - body * 0.10f}, pts, PIP_SEG, col);
            draw_stem(cx, byc + body * 0.30f, s, col);
            break;
        }
        default: { // clubs — trefoil of three distinct lobes over a stem
            float cr = s * 0.255f;
            gfx_circle(cx,                cy - s * 0.25f, cr, col);
            gfx_circle(cx - s * 0.245f,   cy + s * 0.11f, cr, col);
            gfx_circle(cx + s * 0.245f,   cy + s * 0.11f, cr, col);
            draw_stem(cx, cy + s * 0.16f, s, col);
            break;
        }
    }
}

// Card-relative metrics, all proportional to the card width so the art is the
// same design at any scale. The floors keep the smallest touch card legible.
static int card_index_fs(const Layout* L) { return imax(L->card_w * 18 / 80, 8); }
static int card_pad(const Layout* L)      { return imax(L->card_w *  6 / 80, 2); }
static int card_pip_small(const Layout* L){ return imax(L->card_w * 12 / 80, 5); }
static int card_pip_big(const Layout* L)  { return imax(L->card_w * 34 / 80, 12); }

static void draw_card_back(const Layout* L, int x, int y) {
    gfx_rect_rounded(x, y, L->card_w, L->card_h, CARD_ROUND, CARD_BACK);
    gfx_rect_rounded_lines(x, y, L->card_w, L->card_h, CARD_ROUND, CARD_EDGE);
    // A bounded plaid panel inside the card (never spills past its edges).
    int m = imax(L->card_w * 8 / 80, 3);
    int ix = x + m, iy = y + m, iw = L->card_w - 2 * m, ih = L->card_h - 2 * m;
    if (iw <= 0 || ih <= 0) return;
    gfx_rect_lines(ix, iy, iw, ih, CARD_BACK2);
    int step = m;
    for (int gx = ix + step; gx < ix + iw; gx += step)
        gfx_line(gx, iy, gx, iy + ih, CARD_BACK2);
    for (int gy = iy + step; gy < iy + ih; gy += step)
        gfx_line(ix, gy, ix + iw, gy, CARD_BACK2);
}

static void draw_card_face(const Layout* L, int x, int y, Card c, bool hilite) {
    gfx_rect_rounded(x, y, L->card_w, L->card_h, CARD_ROUND, CARD_FACE);
    Color edge = hilite ? HILITE : CARD_EDGE;
    gfx_rect_rounded_lines(x, y, L->card_w, L->card_h, CARD_ROUND, edge);
    if (hilite)
        gfx_rect_rounded_lines(x + 1, y + 1, L->card_w - 2, L->card_h - 2,
                               CARD_ROUND, edge);

    Color col = card_is_red(c) ? RED_PIP : BLACK_PIP;
    const char* rs = RANK_STR[c.rank];
    int fs   = card_index_fs(L);
    int pad  = card_pad(L);
    int psm  = card_pip_small(L);
    int gapy = imax(L->card_w * 8 / 80, 3);

    // Top-left corner index, and the matching bottom-right one (upright; simple
    // and readable at every size).
    gfx_text(rs, x + pad, y + pad * 2 / 3, fs, col);
    draw_pip(x + pad + psm * 0.5f, y + pad * 2 / 3 + fs + gapy, (float)psm, c.suit);
    int tw = gfx_measure_text(rs, fs);
    gfx_text(rs, x + L->card_w - pad - tw, y + L->card_h - pad * 2 / 3 - fs, fs, col);
    draw_pip(x + L->card_w - pad - psm * 0.5f,
             y + L->card_h - pad * 2 / 3 - fs - gapy, (float)psm, c.suit);

    // Large centre pip.
    draw_pip(x + L->card_w / 2.0f, y + L->card_h / 2.0f, (float)card_pip_big(L), c.suit);
}

// Empty pile placeholder. `hint` is -1 for the stock's recycle mark, 0 for none.
static void draw_slot(const Layout* L, int x, int y, int hint, bool hilite) {
    gfx_rect_rounded(x, y, L->card_w, L->card_h, CARD_ROUND, SLOT_FILL);
    gfx_rect_rounded_lines(x, y, L->card_w, L->card_h, CARD_ROUND,
                           hilite ? HILITE : SLOT_LINE);
    if (hint == -1) {
        float cx = x + L->card_w / 2.0f, cy = y + L->card_h / 2.0f;
        float r  = L->card_w / 4.0f;
        gfx_circle_lines(cx, cy, r, SLOT_LINE);
        gfx_circle_lines(cx, cy, r - imax(L->card_w / 40, 1), SLOT_LINE);
    }
}

// --------------------------------------------------------------------------
// Geometry queries, all against an explicit Layout so the live window and the
// recorder's fixed canvas answer them the same way.
// --------------------------------------------------------------------------
// Index of the first waste card that is visible in the fan, and how many are.
static int waste_fan_first(const Game* g, int* fan_out) {
    int fan = (g->draw_mode == DRAW_THREE) ? g->waste_drawn : 1;
    if (fan < 1) fan = 1;
    if (fan > g->waste.count) fan = g->waste.count;
    if (fan_out) *fan_out = fan;
    return g->waste.count - fan;
}

static bool in_card(const Layout* L, int mx, int my, int x, int y) {
    return mx >= x && mx < x + L->card_w && my >= y && my < y + L->card_h;
}

static bool hit_at(const Layout* L, const Game* g, int mx, int my,
                   PileKind* kind, int* index, int* card) {
    // Waste: only the top card is interactive.
    if (g->waste.count > 0) {
        int first = waste_fan_first(g, NULL);
        int tx = L->col_x[SLOT_WASTE] + (g->waste.count - 1 - first) * L->waste_fan;
        if (in_card(L, mx, my, tx, L->top_y)) {
            *kind = LOC_WASTE; *index = 0; *card = g->waste.count - 1;
            return true;
        }
    }
    // Foundations: the top card.
    for (int f = 0; f < 4; f++) {
        if (g->foundation[f].count == 0) continue;
        if (in_card(L, mx, my, L->col_x[SLOT_FOUND0 + f], L->top_y)) {
            *kind = LOC_FOUNDATION; *index = f;
            *card = g->foundation[f].count - 1;
            return true;
        }
    }
    // Tableau: topmost card under the pointer (scan bottom-up through the fan).
    for (int c = 0; c < 7; c++) {
        const Pile* p = &g->tableau[c];
        if (p->count == 0) continue;
        if (mx < L->col_x[c] || mx >= L->col_x[c] + L->card_w) continue;
        int ys[52];
        tab_card_ys(L, p, ys);
        for (int i = p->count - 1; i >= 0; i--) {
            // A card's exposed strip is the distance to the next card's top; the
            // last one shows its whole body. Using the real delta keeps hit
            // testing correct when the fan has been compressed.
            int h = (i == p->count - 1) ? L->card_h : (ys[i + 1] - ys[i]);
            if (my >= ys[i] && my < ys[i] + h) {
                *kind = LOC_TABLEAU; *index = c; *card = i;
                return true;
            }
        }
    }
    return false;
}

static bool card_pos_at(const Layout* L, const Game* g, PileKind kind, int index,
                        int card, int* x, int* y) {
    switch (kind) {
        case LOC_WASTE: {
            int first = waste_fan_first(g, NULL);
            *x = L->col_x[SLOT_WASTE] + (card - first) * L->waste_fan;
            *y = L->top_y;
            return true;
        }
        case LOC_FOUNDATION:
            *x = L->col_x[SLOT_FOUND0 + index];
            *y = L->top_y;
            return true;
        case LOC_TABLEAU: {
            const Pile* p = &g->tableau[index];
            if (card < 0 || card >= p->count) return false;
            int ys[52];
            tab_card_ys(L, p, ys);
            *x = L->col_x[index];
            *y = ys[card];
            return true;
        }
        default:
            return false;
    }
}

static bool drop_target_at(const Layout* L, const Game* g, const DragState* d,
                           PileKind* kind, int* index) {
    // The dragged card's centre picks the overlapping pile.
    int dx = d->mouse_x - d->grab_dx + L->card_w / 2;
    int dy = d->mouse_y - d->grab_dy - d->lift + L->card_h / 2;

    for (int f = 0; f < 4; f++) {
        if (in_card(L, dx, dy, L->col_x[SLOT_FOUND0 + f], L->top_y)) {
            *kind = LOC_FOUNDATION; *index = f; return true;
        }
    }
    // Tableau: match by column x, with vertical slack across the whole fan.
    for (int c = 0; c < 7; c++) {
        int x = L->col_x[c];
        if (dx < x || dx >= x + L->card_w) continue;
        int ys[52];
        int bottom = tab_card_ys(L, &g->tableau[c], ys);
        if (dy >= L->tab_y && dy < bottom) {
            *kind = LOC_TABLEAU; *index = c; return true;
        }
    }
    return false;
}

// --------------------------------------------------------------------------
// Board drawing (parameterized by viewport so it works for window + recorder)
// --------------------------------------------------------------------------
static bool is_dragged(const DragState* d, PileKind k, int idx, int card) {
    return d && d->active && d->src_kind == k && d->src_index == idx
        && card >= d->src_card;
}

// Full-width title bar. When a display cutout (front camera) sits in the bar,
// the wordmark is laid out around it:
//   - cutout absent            -> centred full word (the desktop look)
//   - room both sides          -> "OPEN" left of the camera, "KLONDIKE" right
//   - wide notch, nothing fits -> bar only, no wordmark
// safe_area reports zeros everywhere except Android, so desktop and web always
// take the first branch.
static void draw_titlebar(const Layout* L) {
    gfx_rect(0, 0, L->view_w, L->titlebar_h, FELT_DARK);
    gfx_line(0, L->titlebar_h, L->view_w, L->titlebar_h, SLOT_LINE);

    int fs = L->title_fs;
    int ty = (L->titlebar_h - fs) / 2;
    int full = gfx_measure_text("OPENKLONDIKE", fs);

    int top, cl, cr;
    safe_area_get(&top, &cl, &cr);
    if (cr <= cl) {
        // No horizontal extent reported. With no top inset either there is no
        // cutout at all -> centred wordmark. If there IS an inset we could not
        // localize, leave the bar bare rather than risk centring under a camera.
        if (top <= 0)
            gfx_text("OPENKLONDIKE", (L->view_w - full) / 2, ty, fs, TEXT_LIGHT);
        return;
    }
    int pad     = fs / 2;
    int left_w  = gfx_measure_text("OPEN", fs);
    int right_w = gfx_measure_text("KLONDIKE", fs);
    if (cl >= left_w + pad && L->view_w - cr >= right_w + pad) {
        gfx_text("OPEN", cl - pad - left_w, ty, fs, TEXT_LIGHT);
        gfx_text("KLONDIKE", cr + pad, ty, fs, TEXT_LIGHT);
    } else if (cl >= full + pad) {
        gfx_text("OPENKLONDIKE", cl - pad - full, ty, fs, TEXT_LIGHT);
    } else if (L->view_w - cr >= full + pad) {
        gfx_text("OPENKLONDIKE", cr + pad, ty, fs, TEXT_LIGHT);
    }
}

static void draw_status(const Game* g, const Layout* L) {
    int fs = L->status_fs;
    int y  = L->view_h - L->status_h + (L->status_h - fs) / 2;
    gfx_rect(0, L->view_h - L->status_h, L->view_w, L->status_h, FELT_DARK);
    char buf[64];
    snprintf(buf, sizeof buf, "Score %d", g->score);
    gfx_text(buf, L->margin_x, y, fs, TEXT_LIGHT);
    snprintf(buf, sizeof buf, "Time %d", g->timer_frames / SIM_HZ);
    int tw = gfx_measure_text(buf, fs);
    gfx_text(buf, L->view_w / 2 - tw / 2, y, fs, TEXT_LIGHT);
    snprintf(buf, sizeof buf, "Moves %d", g->moves);
    tw = gfx_measure_text(buf, fs);
    gfx_text(buf, L->view_w - L->margin_x - tw, y, fs, TEXT_LIGHT);
}

typedef struct { const Game* g; const DragState* drag; } BoardCtx;

static void draw_board(void* vctx, int view_w, int view_h) {
    BoardCtx* ctx = (BoardCtx*)vctx;
    const Game* g = ctx->g;
    const DragState* d = ctx->drag;
    gfx_clear(FELT);
    Layout L = layout_for(view_w, view_h);

    // The pile the dragged run would legally land on, highlighted so a touch
    // player can see the drop will take before lifting their finger.
    PileKind hi_kind = LOC_STOCK;
    int hi_index = -1;
    if (d && d->active) {
        PileKind k; int i;
        if (drop_target_at(&L, g, d, &k, &i)
            && game_can_drop(g, d->src_kind, d->src_index, d->src_card, k, i)) {
            hi_kind = k; hi_index = i;
        }
    }

    draw_titlebar(&L);

    // --- Stock ---
    if (g->stock.count > 0) draw_card_back(&L, L.col_x[SLOT_STOCK], L.top_y);
    else                    draw_slot(&L, L.col_x[SLOT_STOCK], L.top_y, -1, false);

    // --- Waste (fan up to the last 3 drawn) ---
    if (g->waste.count == 0) {
        draw_slot(&L, L.col_x[SLOT_WASTE], L.top_y, 0, false);
    } else {
        int first = waste_fan_first(g, NULL);
        for (int i = first; i < g->waste.count; i++) {
            if (is_dragged(d, LOC_WASTE, 0, i)) continue;
            draw_card_face(&L, L.col_x[SLOT_WASTE] + (i - first) * L.waste_fan,
                           L.top_y, g->waste.cards[i], false);
        }
    }

    // --- Foundations ---
    for (int f = 0; f < 4; f++) {
        int fx = L.col_x[SLOT_FOUND0 + f];
        const Pile* p = &g->foundation[f];
        int n = p->count;
        if (is_dragged(d, LOC_FOUNDATION, f, n - 1)) n--;   // hide grabbed top
        bool hl = (hi_kind == LOC_FOUNDATION && hi_index == f);
        if (n == 0) draw_slot(&L, fx, L.top_y, 0, hl);
        else        draw_card_face(&L, fx, L.top_y, p->cards[n - 1], hl);
    }

    // --- Tableau ---
    for (int c = 0; c < 7; c++) {
        const Pile* p = &g->tableau[c];
        int x = L.col_x[c];
        bool hl = (hi_kind == LOC_TABLEAU && hi_index == c);
        if (p->count == 0) {
            draw_slot(&L, x, L.tab_y, 0, hl);
            continue;
        }
        int ys[52];
        tab_card_ys(&L, p, ys);
        int last = p->count - 1;
        for (int i = 0; i < p->count; i++) {
            if (is_dragged(d, LOC_TABLEAU, c, i)) { last = i - 1; break; }
            if (p->cards[i].face_up) draw_card_face(&L, x, ys[i], p->cards[i],
                                                    hl && i == p->count - 1);
            else                     draw_card_back(&L, x, ys[i]);
        }
        // The run was lifted off an empty-looking column: mark the slot instead.
        if (hl && last < 0) draw_slot(&L, x, L.tab_y, 0, true);
    }

    // --- Floating drag stack ---
    if (d && d->active) {
        int dx = d->mouse_x - d->grab_dx;
        int dy = d->mouse_y - d->grab_dy - d->lift;
        for (int i = 0; i < d->count; i++)
            draw_card_face(&L, dx, dy + i * L.fan_up, d->cards[i], false);
    }

    draw_status(g, &L);
}

// --------------------------------------------------------------------------
// Presentation: draw to the window, and (when recording) to a fixed canvas.
// --------------------------------------------------------------------------
typedef void (*SceneFn)(void* ctx, int w, int h);

#ifndef OK_TOUCH
// SSAA factor for the capture path: the frame is drawn at SS× the encoder
// resolution and minified with bilinear filtering, so the MP4 is anti-aliased
// to match (and exceed) the window's MSAA.
#define SS 2

static RenderTexture2D rec_canvas;   // encoder-resolution frame (MIN_W x MIN_H)
static RenderTexture2D rec_super;    // SS× supersampled scratch frame
static bool rec_canvas_ready = false;
#endif

static void emit(SceneFn fn, void* ctx) {
    gfx_begin_frame();
    fn(ctx, GetScreenWidth(), GetScreenHeight());
    gfx_end_frame();

#ifndef OK_TOUCH
    if (recorder_active() && rec_canvas_ready) {
        // 1) Draw the scene at SS× into the supersampled texture (the scene uses
        //    MIN_W/MIN_H coordinates; a scale matrix blows it up to fill).
        BeginTextureMode(rec_super);
        rlPushMatrix();
        rlScalef((float)SS, (float)SS, 1.0f);
        fn(ctx, MIN_W, MIN_H);
        rlPopMatrix();
        EndTextureMode();

        // 2) Minify into the encoder canvas with bilinear filtering (the AA).
        //    Negative source height flips the bottom-up render texture upright.
        BeginTextureMode(rec_canvas);
        Rectangle src = {0, 0, (float)(SS * MIN_W), -(float)(SS * MIN_H)};
        Rectangle dst = {0, 0, (float)MIN_W, (float)MIN_H};
        DrawTexturePro(rec_super.texture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
        EndTextureMode();

        recorder_capture(&rec_canvas);
    }
#endif
}

// --------------------------------------------------------------------------
// Lifecycle
// --------------------------------------------------------------------------
void render_init(void) {
#if defined(PLATFORM_IOS)
    // iOS: UIKit owns the window/surface and drives the loop (CADisplayLink);
    // the Metal layer is attached separately by the app shell. Nothing to do.
#else
#if defined(PLATFORM_ANDROID)
    // Immersive fullscreen so the app draws under the status bar / camera cutout
    // (paired with windowLayoutInDisplayCutoutMode=shortEdges in the theme).
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
    // Request 0x0: raylib's Android backend then renders at the device's native
    // resolution. A fixed size here gets aspect-letterboxed into the display.
    InitWindow(0, 0, "openklondike");
#elif defined(PLATFORM_WEB)
    // Let the GL canvas follow the browser viewport (the HTML shell sizes it);
    // GetScreenWidth/Height then track it so the board re-fits on resize/rotate.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(MIN_W, MIN_H, "openklondike");
#else
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(MIN_W, MIN_H, "openklondike");
    // The desktop board is a fixed pixel size, so the window is never allowed
    // below it -- that is what lets the cards stay unscaled. A browser window
    // cannot be constrained this way, which is why the layout can also shrink.
    SetWindowMinSize(MIN_W, MIN_H);
#endif
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    gfx_font_init();   // load the bundled UI font now the GL context exists
#ifndef OK_TOUCH
    rec_canvas = LoadRenderTexture(MIN_W, MIN_H);
    rec_super  = LoadRenderTexture(SS * MIN_W, SS * MIN_H);
    SetTextureFilter(rec_super.texture, TEXTURE_FILTER_BILINEAR);  // smooth minify
    rec_canvas_ready = true;
#endif
#endif // PLATFORM_IOS
}

void render_cleanup(void) {
#if !defined(PLATFORM_IOS)
#ifndef OK_TOUCH
    if (rec_canvas_ready) {
        UnloadRenderTexture(rec_canvas);
        UnloadRenderTexture(rec_super);
        rec_canvas_ready = false;
    }
#endif
    CloseWindow();
#endif
}

bool render_window_should_close(void) { return WindowShouldClose(); }
bool render_window_focused(void)      { return IsWindowFocused(); }

void render_toggle_fullscreen(void) {
#if defined(PLATFORM_ANDROID) || defined(PLATFORM_IOS) || defined(PLATFORM_WEB)
    // Mobile apps are always fullscreen, and a browser tab cannot enter
    // fullscreen without a user gesture the game does not own.
#else
    if (IsWindowFullscreen()) {
        ToggleFullscreen();
        SetWindowSize(MIN_W, MIN_H);
    } else {
        int m = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(m), GetMonitorHeight(m));
        ToggleFullscreen();
    }
#endif
}

// --------------------------------------------------------------------------
// Public scenes
// --------------------------------------------------------------------------
void render_frame(const Game* g, const DragState* drag) {
    BoardCtx ctx = { g, drag };
    emit(draw_board, &ctx);
}

// Menu geometry, scaled off the viewport's short side so the panel is the same
// proportion of a phone screen as of the 704x704 desktop minimum (where these
// ratios reproduce the original fixed 400px panel exactly).
typedef struct {
    int cx, px, py, panel_w, panel_h;
    int title_fs, title_y, items_y, line_h, item_fs, pad;
} MenuLayout;

static MenuLayout menu_layout(int vw, int vh, int rows) {
    MenuLayout m;
    int u = (vw < vh) ? vw : vh;
    m.title_fs = imax(u * 46 / 704, 14);
    m.item_fs  = imax(u * 22 / 704, 10);
    m.line_h   = imax(u * 32 / 704, m.item_fs * 3 / 2);
    m.pad      = imax(u * 26 / 704, 8);
    int side   = imax(u / 28, 6);
    m.panel_w  = imax(u * 400 / 704, 160);
    if (m.panel_w > vw - 2 * side) m.panel_w = vw - 2 * side;
    m.panel_h  = m.title_fs + 2 * m.pad + rows * m.line_h + m.pad * 2;
    m.cx       = vw / 2;
    m.px       = m.cx - m.panel_w / 2;
    m.py       = (vh - m.panel_h) / 2;
    if (m.py < 0) m.py = 0;
    m.title_y  = m.py + m.pad;
    m.items_y  = m.title_y + m.title_fs + m.pad;
    return m;
}

// Menu item rectangles captured by the last render_menu(), for touch hit
// testing. Written by draw_menu, read by render_menu_hit_test.
static Rectangle s_menu_item_rects[8];
static int s_menu_item_count = 0;

typedef struct {
    const char* title; const char** labels;
    int count; int selected; int gap_before;
} MenuCtx;

static void draw_menu(void* vctx, int view_w, int view_h) {
    MenuCtx* mc = (MenuCtx*)vctx;
    gfx_clear(FELT);

    int extra = (mc->gap_before >= 0) ? 1 : 0;
    MenuLayout m = menu_layout(view_w, view_h, mc->count + extra);

    gfx_rect_rounded(m.px, m.py, m.panel_w, m.panel_h, 0.05f, MENU_BG);
    gfx_rect_rounded_lines(m.px, m.py, m.panel_w, m.panel_h, 0.05f, TEXT_DIM);

    gfx_text(mc->title, m.cx - gfx_measure_text(mc->title, m.title_fs) / 2,
             m.title_y, m.title_fs, TEXT_LIGHT);

    s_menu_item_count = (mc->count < 8) ? mc->count : 8;
    int y = m.items_y;
    for (int i = 0; i < mc->count; i++) {
        if (mc->gap_before == i) y += m.line_h;
        const char* label = mc->labels[i];
        int lw = gfx_measure_text(label, m.item_fs);
        Color col = (i == mc->selected) ? HILITE : TEXT_DIM;
        if (i == mc->selected) {
            gfx_text(">", m.cx - lw / 2 - m.item_fs * 3 / 2, y, m.item_fs, HILITE);
            gfx_text("<", m.cx + lw / 2 + m.item_fs / 2, y, m.item_fs, HILITE);
        }
        gfx_text(label, m.cx - lw / 2, y, m.item_fs, col);
        if (i < 8) {
            // A generous full-width row, so a finger lands on the item and not
            // between two of them.
            s_menu_item_rects[i] = (Rectangle){
                (float)m.px, (float)(y - (m.line_h - m.item_fs) / 2),
                (float)m.panel_w, (float)m.line_h };
        }
        y += m.line_h;
    }
}

void render_menu(const char* title, const char** labels, int count,
                 int selected, int gap_before) {
    MenuCtx ctx = { title, labels, count, selected, gap_before };
    emit(draw_menu, &ctx);
}

int render_menu_hit_test(Vector2 p) {
    for (int i = 0; i < s_menu_item_count; i++)
        if (CheckCollisionPointRec(p, s_menu_item_rects[i])) return i;
    return -1;
}

// --------------------------------------------------------------------------
// Hit testing against the live window
// --------------------------------------------------------------------------
bool render_stock_hit(int mx, int my) {
    Layout L = live_layout();
    return in_card(&L, mx, my, L.col_x[SLOT_STOCK], L.top_y);
}

bool render_hit(const Game* g, int mx, int my,
                PileKind* kind, int* index, int* card) {
    Layout L = live_layout();
    return hit_at(&L, g, mx, my, kind, index, card);
}

bool render_card_pos(const Game* g, PileKind kind, int index, int card,
                     int* x, int* y) {
    Layout L = live_layout();
    return card_pos_at(&L, g, kind, index, card, x, y);
}

bool render_drop_target(const Game* g, const DragState* d,
                        PileKind* kind, int* index) {
    Layout L = live_layout();
    return drop_target_at(&L, g, d, kind, index);
}

// --------------------------------------------------------------------------
// Win cascade (bouncing cards)
// --------------------------------------------------------------------------
// The cards launch off the foundations and bounce across the board, leaving a
// trail. On the raylib platforms the trail accumulates into a render texture, so
// it is unbounded and free to redraw. iOS has no render texture (the Metal
// backend rebuilds its vertex list every frame), so there the trail is a ring of
// the most recent stamps, redrawn over a freshly-drawn board each frame.
#define GRAVITY   0.55f
#define DAMP      0.82f

#if defined(PLATFORM_IOS)
#define BOUNCE_TRAIL 256
#endif

static struct {
    bool  active;
#if defined(PLATFORM_IOS)
    Game  snap;                       // the final board, redrawn under the trail
    struct { Card card; int x, y; } trail[BOUNCE_TRAIL];
    int   trail_n, trail_head;
#else
    RenderTexture2D canvas;
#endif
    int   W, H;
    Pile  found[4];
    int   found_x[4], found_y[4];
    bool  flying;
    bool  done;
    Card  card;
    float x, y, vx, vy;
    unsigned rng;
} B;

static unsigned brand(void) { B.rng = B.rng * 1103515245u + 12345u; return (B.rng >> 16) & 0x7fff; }

void render_bounce_begin(const Game* g) {
    B.W = GetScreenWidth();
    B.H = GetScreenHeight();
    B.active = true;
    B.flying = false;
    B.done = false;
    B.rng = (unsigned)(g->score * 2654435761u + g->moves + 1);

    Layout L = layout_for(B.W, B.H);
    for (int f = 0; f < 4; f++) {
        B.found[f] = g->foundation[f];
        B.found_x[f] = L.col_x[SLOT_FOUND0 + f];
        B.found_y[f] = L.top_y;
    }

#if defined(PLATFORM_IOS)
    B.snap = *g;
    B.trail_n = 0;
    B.trail_head = 0;
#else
    // Paint the final board as the static backdrop the cards bounce over.
    B.canvas = LoadRenderTexture(B.W, B.H);
    BoardCtx ctx = { g, NULL };
    BeginTextureMode(B.canvas);
    draw_board(&ctx, B.W, B.H);
    EndTextureMode();
#endif
}

static bool spawn_next(void) {
    int best = -1;
    for (int f = 0; f < 4; f++) {
        if (B.found[f].count == 0) continue;
        if (best < 0 || B.found[f].cards[B.found[f].count - 1].rank
                       > B.found[best].cards[B.found[best].count - 1].rank)
            best = f;
    }
    if (best < 0) return false;
    B.card = B.found[best].cards[--B.found[best].count];
    B.x = (float)B.found_x[best];
    B.y = (float)B.found_y[best];
    B.vx = ((brand() % 2) ? 1.0f : -1.0f) * (4.0f + brand() % 6);
    B.vy = -(2.0f + brand() % 4);
    B.flying = true;
    return true;
}

// One fixed simulation step of the cascade: advance the flying card and stamp it
// into the trail. Called `steps` times per rendered frame so the trail has the
// same density at 60 and at 120 Hz.
static void bounce_advance(const Layout* L) {
    if (!B.flying && !B.done && !spawn_next()) B.done = true;
    if (!B.flying) return;

    B.vy += GRAVITY;
    B.x += B.vx;
    B.y += B.vy;
    float floor_y = (float)(B.H - L->status_h - L->card_h);
    if (B.y > floor_y) { B.y = floor_y; B.vy = -B.vy * DAMP; }

#if defined(PLATFORM_IOS)
    B.trail[B.trail_head].card = B.card;
    B.trail[B.trail_head].x = (int)B.x;
    B.trail[B.trail_head].y = (int)B.y;
    B.trail_head = (B.trail_head + 1) % BOUNCE_TRAIL;
    if (B.trail_n < BOUNCE_TRAIL) B.trail_n++;
#else
    BeginTextureMode(B.canvas);
    draw_card_face(L, (int)B.x, (int)B.y, B.card, false);
    EndTextureMode();
#endif
    if (B.x < -L->card_w || B.x > B.W) B.flying = false;
}

bool render_bounce_step(int steps) {
    if (!B.active) return true;
    Layout L = layout_for(B.W, B.H);

    for (int s = 0; s < steps; s++) bounce_advance(&L);

    gfx_begin_frame();
#if defined(PLATFORM_IOS)
    BoardCtx ctx = { &B.snap, NULL };
    draw_board(&ctx, B.W, B.H);
    // Oldest first, so newer stamps land on top as they would on a canvas.
    for (int i = 0; i < B.trail_n; i++) {
        int idx = (B.trail_head - B.trail_n + i + 2 * BOUNCE_TRAIL) % BOUNCE_TRAIL;
        draw_card_face(&L, B.trail[idx].x, B.trail[idx].y, B.trail[idx].card, false);
    }
#else
    // Blit the trail canvas to the window (textures are stored bottom-up).
    Rectangle src = {0, 0, (float)B.W, -(float)B.H};
    DrawTextureRec(B.canvas.texture, src, (Vector2){0, 0}, WHITE);
#endif
    if (B.done) {
        MenuLayout m = menu_layout(B.W, B.H, 2);
        int cx = B.W / 2, cy = B.H / 2;
        int pw = m.panel_w, ph = m.title_fs + m.item_fs + m.pad * 3;
        gfx_rect(cx - pw / 2, cy - ph / 2, pw, ph, (Color){0, 0, 0, 190});
        gfx_rect_lines(cx - pw / 2, cy - ph / 2, pw, ph, TEXT_LIGHT);
        const char* msg = "YOU WIN";
        gfx_text(msg, cx - gfx_measure_text(msg, m.title_fs) / 2,
                 cy - ph / 2 + m.pad, m.title_fs, HILITE);
        const char* sub = render_use_portrait() ? "Tap to continue" : "Press any key";
        gfx_text(sub, cx - gfx_measure_text(sub, m.item_fs) / 2,
                 cy - ph / 2 + m.pad * 2 + m.title_fs, m.item_fs, TEXT_DIM);
    }
    gfx_end_frame();
    return B.done;
}

void render_bounce_end(void) {
    if (!B.active) return;
#if !defined(PLATFORM_IOS)
    UnloadRenderTexture(B.canvas);
#endif
    B.active = false;
}
