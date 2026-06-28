#include "render.h"
#include "recorder.h"
#include <raylib.h>
#include <rlgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// --------------------------------------------------------------------------
// Layout metrics (all fixed — the board never scales)
// --------------------------------------------------------------------------
#define MARGIN_X   24
#define MARGIN_TOP 24
#define COL_GAP    16
#define ROW_GAP    28
#define FAN_UP     28   // vertical overlap between face-up tableau cards
#define FAN_DOWN   12   // tighter overlap for face-down cards
#define WASTE_FAN  24   // horizontal spread of the 3 drawn waste cards
#define TITLEBAR_H 44   // top bar carrying the game name
#define STATUS_H   28   // bottom bar carrying score / time / moves

#define CONTENT_W  (7 * CARD_W + 6 * COL_GAP)   // 7-column board width

// Top-row slot indices map onto tableau columns:
//   col 0 = stock, col 1 = waste, col 2 = gap, cols 3..6 = foundations 0..3.
#define SLOT_STOCK   0
#define SLOT_WASTE   1
#define SLOT_FOUND0  3

// --------------------------------------------------------------------------
// Colors
// --------------------------------------------------------------------------
static const Color FELT        = { 12,  92,  52, 255};   // classic green table
static const Color FELT_DARK   = { 10,  76,  44, 255};
static const Color CARD_FACE   = {248, 248, 242, 255};
static const Color CARD_EDGE   = { 40,  40,  40, 255};
static const Color CARD_BACK   = { 36,  72, 156, 255};
static const Color CARD_BACK2  = { 80, 130, 220, 255};
static const Color SLOT_LINE   = { 30, 110,  66, 255};
static const Color SLOT_FILL   = { 14,  84,  48, 255};
static const Color RED_PIP     = {200,  30,  40, 255};
static const Color BLACK_PIP   = { 20,  20,  24, 255};
static const Color HILITE      = {255, 235, 120, 255};
static const Color MENU_BG     = { 16,  40,  28, 255};
static const Color TEXT_LIGHT  = {235, 235, 225, 255};
static const Color TEXT_DIM    = {170, 190, 175, 255};

// --------------------------------------------------------------------------
// Layout
// --------------------------------------------------------------------------
typedef struct {
    int left;       // x of the board's left edge
    int col_x[7];   // x of each column / top-row slot
    int top_y;      // y of the top row
    int tab_y;      // y where the tableau fans begin
    int view_h;     // viewport height (for the status bar + bounce floor)
} Layout;

static Layout layout_for(int view_w, int view_h) {
    Layout L;
    L.left = (view_w - CONTENT_W) / 2;
    if (L.left < MARGIN_X) L.left = MARGIN_X;
    for (int c = 0; c < 7; c++) L.col_x[c] = L.left + c * (CARD_W + COL_GAP);
    L.top_y  = TITLEBAR_H + MARGIN_TOP;
    L.tab_y  = TITLEBAR_H + MARGIN_TOP + CARD_H + ROW_GAP;
    L.view_h = view_h;
    return L;
}

// y position of each card in a tableau column (accumulated fan offsets).
static int tab_card_ys(const Pile* p, int tab_y, int* ys) {
    int y = tab_y;
    for (int i = 0; i < p->count; i++) {
        ys[i] = y;
        y += p->cards[i].face_up ? FAN_UP : FAN_DOWN;
    }
    return y;
}

// --------------------------------------------------------------------------
// Card art (all vector-drawn — no asset files)
// --------------------------------------------------------------------------
static const char* RANK_STR[14] = {
    "", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"
};

// Fill a closed polygon as a triangle fan from `c`. Each triangle is drawn in
// both windings so it shows regardless of the outline's orientation. Works for
// any polygon that is star-shaped about `c` (true for all of our pip shapes).
static void fill_fan(Vector2 c, const Vector2* p, int n, Color col) {
    for (int i = 0; i < n; i++) {
        Vector2 a = p[i], b = p[(i + 1) % n];
        DrawTriangle(c, a, b, col);
        DrawTriangle(c, b, a, col);
    }
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
    // neck
    DrawTriangle(tl, ml, mr, col); DrawTriangle(tl, mr, ml, col);
    DrawTriangle(tl, mr, tr, col); DrawTriangle(tl, tr, mr, col);
    // flared foot
    DrawTriangle(ml, bl, br, col); DrawTriangle(ml, br, bl, col);
    DrawTriangle(ml, br, mr, col); DrawTriangle(ml, mr, br, col);
}

// Draw one suit pip centred at (cx,cy) with overall height s.
static void draw_pip(int cx, int cy, int s, int suit) {
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
            DrawCircle(cx,                   cy - (int)(s * 0.25f), cr, col);
            DrawCircle(cx - (int)(s*0.245f), cy + (int)(s * 0.11f), cr, col);
            DrawCircle(cx + (int)(s*0.245f), cy + (int)(s * 0.11f), cr, col);
            draw_stem(cx, cy + s * 0.16f, s, col);
            break;
        }
    }
}

static void draw_card_back(int x, int y) {
    DrawRectangleRounded((Rectangle){x, y, CARD_W, CARD_H}, 0.12f, 6, CARD_BACK);
    DrawRectangleRoundedLines((Rectangle){x, y, CARD_W, CARD_H}, 0.12f, 6, CARD_EDGE);
    // A bounded plaid panel inside the card (never spills past its edges).
    int m = 8;
    int ix = x + m, iy = y + m, iw = CARD_W - 2 * m, ih = CARD_H - 2 * m;
    DrawRectangleLines(ix, iy, iw, ih, CARD_BACK2);
    for (int gx = ix + 8; gx < ix + iw; gx += 8)
        DrawLine(gx, iy, gx, iy + ih, CARD_BACK2);
    for (int gy = iy + 8; gy < iy + ih; gy += 8)
        DrawLine(ix, gy, ix + iw, gy, CARD_BACK2);
}

static void draw_card_face(int x, int y, Card c, bool hilite) {
    DrawRectangleRounded((Rectangle){x, y, CARD_W, CARD_H}, 0.12f, 6, CARD_FACE);
    Color edge = hilite ? HILITE : CARD_EDGE;
    DrawRectangleRoundedLines((Rectangle){x, y, CARD_W, CARD_H}, 0.12f, 6, edge);
    if (hilite)
        DrawRectangleRoundedLines((Rectangle){x + 1, y + 1, CARD_W - 2, CARD_H - 2},
                                  0.12f, 6, edge);

    Color col = card_is_red(c) ? RED_PIP : BLACK_PIP;
    const char* rs = RANK_STR[c.rank];
    int fs = 18;
    // top-left corner index
    DrawText(rs, x + 6, y + 4, fs, col);
    draw_pip(x + 12, y + 4 + fs + 8, 12, c.suit);
    // bottom-right corner index (upright; simple and readable)
    int tw = MeasureText(rs, fs);
    DrawText(rs, x + CARD_W - 6 - tw, y + CARD_H - 4 - fs, fs, col);
    draw_pip(x + CARD_W - 12, y + CARD_H - 4 - fs - 8, 12, c.suit);
    // large center pip
    draw_pip(x + CARD_W / 2, y + CARD_H / 2, 34, c.suit);
}

// Empty pile placeholder. `hint` (1..4 for a suit, 0 = none, -1 = recycle).
static void draw_slot(int x, int y, int hint) {
    DrawRectangleRounded((Rectangle){x, y, CARD_W, CARD_H}, 0.12f, 6, SLOT_FILL);
    DrawRectangleRoundedLines((Rectangle){x, y, CARD_W, CARD_H}, 0.12f, 6, SLOT_LINE);
    if (hint == -1) {
        DrawCircleLines(x + CARD_W / 2, y + CARD_H / 2, CARD_W / 4, SLOT_LINE);
        DrawCircleLines(x + CARD_W / 2, y + CARD_H / 2, CARD_W / 4 - 2, SLOT_LINE);
    }
}

// --------------------------------------------------------------------------
// Board drawing (parameterized by viewport so it works for window + recorder)
// --------------------------------------------------------------------------
static bool is_dragged(const DragState* d, PileKind k, int idx, int card) {
    return d && d->active && d->src_kind == k && d->src_index == idx
        && card >= d->src_card;
}

static void draw_titlebar(int view_w) {
    DrawRectangle(0, 0, view_w, TITLEBAR_H, FELT_DARK);
    DrawLine(0, TITLEBAR_H, view_w, TITLEBAR_H, SLOT_LINE);
    const char* title = "OPENKLONDIKE";
    int fs = 22;
    int tw = MeasureText(title, fs);
    DrawText(title, view_w / 2 - tw / 2, (TITLEBAR_H - fs) / 2, fs, TEXT_LIGHT);
}

static void draw_status(const Game* g, const Layout* L, int view_w) {
    int y = L->view_h - STATUS_H + 4;
    DrawRectangle(0, L->view_h - STATUS_H, view_w, STATUS_H, FELT_DARK);
    char buf[64];
    snprintf(buf, sizeof buf, "Score %d", g->score);
    DrawText(buf, MARGIN_X, y, 18, TEXT_LIGHT);
    snprintf(buf, sizeof buf, "Time %d", g->timer_frames / 60);
    int tw = MeasureText(buf, 18);
    DrawText(buf, view_w / 2 - tw / 2, y, 18, TEXT_LIGHT);
    snprintf(buf, sizeof buf, "Moves %d", g->moves);
    tw = MeasureText(buf, 18);
    DrawText(buf, view_w - MARGIN_X - tw, y, 18, TEXT_LIGHT);
}

typedef struct { const Game* g; const DragState* drag; } BoardCtx;

static void draw_board(void* vctx, int view_w, int view_h) {
    BoardCtx* ctx = (BoardCtx*)vctx;
    const Game* g = ctx->g;
    const DragState* d = ctx->drag;
    ClearBackground(FELT);
    Layout L = layout_for(view_w, view_h);

    draw_titlebar(view_w);

    // --- Stock ---
    int sx = L.col_x[SLOT_STOCK], sy = L.top_y;
    if (g->stock.count > 0) draw_card_back(sx, sy);
    else                    draw_slot(sx, sy, -1);

    // --- Waste (fan up to the last 3 drawn) ---
    int wx = L.col_x[SLOT_WASTE], wy = L.top_y;
    if (g->waste.count == 0) {
        draw_slot(wx, wy, 0);
    } else {
        int fan = (g->draw_mode == DRAW_THREE) ? g->waste_drawn : 1;
        if (fan < 1) fan = 1;
        if (fan > g->waste.count) fan = g->waste.count;
        int first = g->waste.count - fan;        // first visible index
        for (int i = first; i < g->waste.count; i++) {
            bool top = (i == g->waste.count - 1);
            if (is_dragged(d, LOC_WASTE, 0, i)) { if (top) continue; }
            int cx = wx + (i - first) * WASTE_FAN;
            draw_card_face(cx, wy, g->waste.cards[i], false);
        }
    }

    // --- Foundations ---
    for (int f = 0; f < 4; f++) {
        int fx = L.col_x[SLOT_FOUND0 + f], fy = L.top_y;
        const Pile* p = &g->foundation[f];
        int n = p->count;
        if (is_dragged(d, LOC_FOUNDATION, f, n - 1)) n--;   // hide grabbed top
        if (n == 0) draw_slot(fx, fy, 0);
        else        draw_card_face(fx, fy, p->cards[n - 1], false);
    }

    // --- Tableau ---
    for (int c = 0; c < 7; c++) {
        const Pile* p = &g->tableau[c];
        int x = L.col_x[c];
        if (p->count == 0) {
            draw_slot(x, L.tab_y, 0);
            continue;
        }
        int ys[52];
        tab_card_ys(p, L.tab_y, ys);
        for (int i = 0; i < p->count; i++) {
            if (is_dragged(d, LOC_TABLEAU, c, i)) break;   // run + rest are floating
            if (p->cards[i].face_up) draw_card_face(x, ys[i], p->cards[i], false);
            else                     draw_card_back(x, ys[i]);
        }
    }

    // --- Floating drag stack ---
    if (d && d->active) {
        int dx = d->mouse_x - d->grab_dx;
        int dy = d->mouse_y - d->grab_dy;
        for (int i = 0; i < d->count; i++)
            draw_card_face(dx, dy + i * FAN_UP, d->cards[i], false);
    }

    draw_status(g, &L, view_w);
}

// --------------------------------------------------------------------------
// Presentation: draw to the window, and (when recording) to a fixed canvas.
// --------------------------------------------------------------------------
typedef void (*SceneFn)(void* ctx, int w, int h);

// SSAA factor for the capture path: the frame is drawn at SS× the encoder
// resolution and minified with bilinear filtering, so the MP4 is anti-aliased
// to match (and exceed) the window's MSAA.
#define SS 2

static RenderTexture2D rec_canvas;   // encoder-resolution frame (MIN_W x MIN_H)
static RenderTexture2D rec_super;     // SS× supersampled scratch frame
static bool rec_canvas_ready = false;

static void emit(SceneFn fn, void* ctx) {
    BeginDrawing();
    fn(ctx, GetScreenWidth(), GetScreenHeight());
    EndDrawing();

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
}

// --------------------------------------------------------------------------
// Lifecycle
// --------------------------------------------------------------------------
void render_init(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(MIN_W, MIN_H, "openklondike");
    SetWindowMinSize(MIN_W, MIN_H);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
    rec_canvas = LoadRenderTexture(MIN_W, MIN_H);
    rec_super  = LoadRenderTexture(SS * MIN_W, SS * MIN_H);
    SetTextureFilter(rec_super.texture, TEXTURE_FILTER_BILINEAR);  // smooth minify
    rec_canvas_ready = true;
}

void render_cleanup(void) {
    if (rec_canvas_ready) {
        UnloadRenderTexture(rec_canvas);
        UnloadRenderTexture(rec_super);
    }
    CloseWindow();
}

bool render_window_should_close(void) { return WindowShouldClose(); }

void render_toggle_fullscreen(void) {
    if (IsWindowFullscreen()) {
        ToggleFullscreen();
        SetWindowSize(MIN_W, MIN_H);
    } else {
        int m = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(m), GetMonitorHeight(m));
        ToggleFullscreen();
    }
}

// --------------------------------------------------------------------------
// Public scenes
// --------------------------------------------------------------------------
void render_frame(const Game* g, const DragState* drag) {
    BoardCtx ctx = { g, drag };
    emit(draw_board, &ctx);
}

typedef struct {
    const char* title; const char** labels;
    int count; int selected; int gap_before;
} MenuCtx;

static void draw_menu(void* vctx, int view_w, int view_h) {
    MenuCtx* m = (MenuCtx*)vctx;
    ClearBackground(FELT);

    int cx = view_w / 2;
    int line_h = 32;
    int extra = (m->gap_before >= 0) ? 1 : 0;
    int title_size = 46;
    int panel_w = 400;
    int content_h = title_size + 40 + (m->count + extra) * line_h;
    int panel_h = content_h + 60;
    int px = cx - panel_w / 2;
    int py = (view_h - panel_h) / 2;

    DrawRectangleRounded((Rectangle){px, py, panel_w, panel_h}, 0.05f, 8, MENU_BG);
    DrawRectangleRoundedLines((Rectangle){px, py, panel_w, panel_h}, 0.05f, 8, TEXT_DIM);

    DrawText(m->title, cx - MeasureText(m->title, title_size) / 2,
             py + 26, title_size, TEXT_LIGHT);

    int y = py + 26 + title_size + 26;
    for (int i = 0; i < m->count; i++) {
        if (m->gap_before == i) y += line_h;
        const char* label = m->labels[i];
        int size = 22;
        int lw = MeasureText(label, size);
        Color col = (i == m->selected) ? HILITE : TEXT_DIM;
        if (i == m->selected) {
            DrawText(">", cx - lw / 2 - 28, y, size, HILITE);
            DrawText("<", cx + lw / 2 + 14, y, size, HILITE);
        }
        DrawText(label, cx - lw / 2, y, size, col);
        y += line_h;
    }
}

void render_menu(const char* title, const char** labels, int count,
                 int selected, int gap_before) {
    MenuCtx ctx = { title, labels, count, selected, gap_before };
    emit(draw_menu, &ctx);
}

// --------------------------------------------------------------------------
// Hit testing (window viewport)
// --------------------------------------------------------------------------
static bool in_card(int mx, int my, int x, int y) {
    return mx >= x && mx < x + CARD_W && my >= y && my < y + CARD_H;
}

bool render_stock_hit(int mx, int my) {
    Layout L = layout_for(GetScreenWidth(), GetScreenHeight());
    return in_card(mx, my, L.col_x[SLOT_STOCK], L.top_y);
}

bool render_hit(const Game* g, int mx, int my,
                PileKind* kind, int* index, int* card) {
    Layout L = layout_for(GetScreenWidth(), GetScreenHeight());

    // Waste: only the top card is interactive.
    if (g->waste.count > 0) {
        int fan = (g->draw_mode == DRAW_THREE) ? g->waste_drawn : 1;
        if (fan < 1) fan = 1;
        if (fan > g->waste.count) fan = g->waste.count;
        int first = g->waste.count - fan;
        int tx = L.col_x[SLOT_WASTE] + (g->waste.count - 1 - first) * WASTE_FAN;
        if (in_card(mx, my, tx, L.top_y)) {
            *kind = LOC_WASTE; *index = 0; *card = g->waste.count - 1;
            return true;
        }
    }
    // Foundations: the top card.
    for (int f = 0; f < 4; f++) {
        if (g->foundation[f].count == 0) continue;
        if (in_card(mx, my, L.col_x[SLOT_FOUND0 + f], L.top_y)) {
            *kind = LOC_FOUNDATION; *index = f;
            *card = g->foundation[f].count - 1;
            return true;
        }
    }
    // Tableau: topmost card under the cursor (scan top-down).
    for (int c = 0; c < 7; c++) {
        const Pile* p = &g->tableau[c];
        if (p->count == 0) continue;
        int ys[52];
        tab_card_ys(p, L.tab_y, ys);
        for (int i = p->count - 1; i >= 0; i--) {
            int h = (i == p->count - 1) ? CARD_H
                  : (p->cards[i].face_up ? FAN_UP : FAN_DOWN);
            if (mx >= L.col_x[c] && mx < L.col_x[c] + CARD_W
                && my >= ys[i] && my < ys[i] + h) {
                *kind = LOC_TABLEAU; *index = c; *card = i;
                return true;
            }
        }
    }
    return false;
}

bool render_card_pos(const Game* g, PileKind kind, int index, int card,
                     int* x, int* y) {
    Layout L = layout_for(GetScreenWidth(), GetScreenHeight());
    switch (kind) {
        case LOC_WASTE: {
            int fan = (g->draw_mode == DRAW_THREE) ? g->waste_drawn : 1;
            if (fan < 1) fan = 1;
            if (fan > g->waste.count) fan = g->waste.count;
            int first = g->waste.count - fan;
            *x = L.col_x[SLOT_WASTE] + (card - first) * WASTE_FAN;
            *y = L.top_y;
            return true;
        }
        case LOC_FOUNDATION:
            *x = L.col_x[SLOT_FOUND0 + index];
            *y = L.top_y;
            return true;
        case LOC_TABLEAU: {
            const Pile* p = &g->tableau[index];
            if (card < 0 || card >= p->count) return false;
            int ys[52];
            tab_card_ys(p, L.tab_y, ys);
            *x = L.col_x[index];
            *y = ys[card];
            return true;
        }
        default:
            return false;
    }
}

bool render_drop_target(const Game* g, const DragState* d,
                        PileKind* kind, int* index) {
    Layout L = layout_for(GetScreenWidth(), GetScreenHeight());
    // Use the dragged card's center to pick an overlapping pile.
    int dx = d->mouse_x - d->grab_dx + CARD_W / 2;
    int dy = d->mouse_y - d->grab_dy + CARD_H / 2;

    // Foundations (single-card runs only, but report the target regardless).
    for (int f = 0; f < 4; f++) {
        int fx = L.col_x[SLOT_FOUND0 + f], fy = L.top_y;
        if (dx >= fx && dx < fx + CARD_W && dy >= fy && dy < fy + CARD_H) {
            *kind = LOC_FOUNDATION; *index = f; return true;
        }
    }
    // Tableau: match by column x, with vertical slack across the fan.
    for (int c = 0; c < 7; c++) {
        int x = L.col_x[c];
        if (dx < x || dx >= x + CARD_W) continue;
        const Pile* p = &g->tableau[c];
        int ys[52];
        int end = tab_card_ys(p, L.tab_y, ys);   // y just past the last card's top
        int bottom = (p->count == 0) ? L.tab_y + CARD_H : end + CARD_H;
        if (dy >= L.tab_y && dy < bottom) {
            *kind = LOC_TABLEAU; *index = c; return true;
        }
    }
    return false;
}

// --------------------------------------------------------------------------
// Win cascade (bouncing cards)
// --------------------------------------------------------------------------
#define GRAVITY   0.55f
#define DAMP      0.82f

static struct {
    bool  active;
    RenderTexture2D canvas;
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
    B.canvas = LoadRenderTexture(B.W, B.H);
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

    // Paint the final board as the static backdrop the cards bounce over.
    BoardCtx ctx = { g, NULL };
    BeginTextureMode(B.canvas);
    draw_board(&ctx, B.W, B.H);
    EndTextureMode();
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

bool render_bounce_step(void) {
    if (!B.active) return true;

    if (!B.flying && !B.done) {
        if (!spawn_next()) B.done = true;
    }

    if (B.flying) {
        B.vy += GRAVITY;
        B.x += B.vx;
        B.y += B.vy;
        float floor = (float)(B.H - STATUS_H - CARD_H);
        if (B.y > floor) { B.y = floor; B.vy = -B.vy * DAMP; }
        // Paint the card onto the accumulating canvas (leaves a trail).
        BeginTextureMode(B.canvas);
        draw_card_face((int)B.x, (int)B.y, B.card, false);
        EndTextureMode();
        if (B.x < -CARD_W || B.x > B.W) B.flying = false;
    }

    // Blit the trail canvas to the window (textures are stored bottom-up).
    BeginDrawing();
    Rectangle src = {0, 0, (float)B.W, -(float)B.H};
    DrawTextureRec(B.canvas.texture, src, (Vector2){0, 0}, WHITE);
    if (B.done) {
        int cx = B.W / 2, cy = B.H / 2;
        DrawRectangle(cx - 160, cy - 50, 320, 100, (Color){0, 0, 0, 160});
        DrawRectangleLines(cx - 160, cy - 50, 320, 100, TEXT_LIGHT);
        const char* msg = "YOU WIN";
        DrawText(msg, cx - MeasureText(msg, 40) / 2, cy - 34, 40, HILITE);
        const char* sub = "Press any key";
        DrawText(sub, cx - MeasureText(sub, 18) / 2, cy + 14, 18, TEXT_DIM);
    }
    EndDrawing();
    return B.done;
}

void render_bounce_end(void) {
    if (!B.active) return;
    UnloadRenderTexture(B.canvas);
    B.active = false;
}
