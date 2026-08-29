// Unit tests for the board geometry — the two layouts and the hit testing built
// on them. No raylib, no window, no device.
//
// PLATFORM_IOS makes ok_types.h supply the geometry types itself instead of
// pulling in <raylib.h>, which is what lets the board be computed off-device.
// That would normally leave only the touch layout compiled, so OK_FIXED is
// forced on as well; platform.h then sees both and turns on OK_RUNTIME_LAYOUT,
// exactly as the web build does, so one binary can switch between the fixed
// desktop board and the scaled touch board with render_set_scaled().
//
// Throughout: "fixed" and "scaled" are the two BOARDS. Where a test says
// landscape or portrait it means the device orientation, which the scaled board
// supports both of.
//
// What is checked is what a player would notice: the desktop board is exactly
// the fixed size it has always been, the touch board fits whatever screen it is
// handed, the deepest possible tableau column stays on screen at either size,
// and -- the part that decides whether a scaled board is playable at all -- a
// press on a card picks up that card and no other.
#define PLATFORM_IOS 1
#define OK_FIXED 1

#include "../src/game.c"
#include "../src/render.c"
#include "../src/render_fixed.c"
#include "../src/render_scaled.c"
#include "../src/safe_area.c"

#include <stdio.h>
#include <stdlib.h>

#define PASS(name) printf("PASS: %s\n", name)
#define FAIL(name, msg) do { fprintf(stderr, "FAIL: %s — %s\n", name, msg); exit(1); } while(0)

// --- The surface render.c draws and queries through -------------------------
// Geometry is all this test cares about, so the primitives are inert and the
// screen size is scripted. gfx_measure_text returns a plausible proportional
// width so any centring arithmetic that ran would not divide by zero.
static int g_screen_w = MIN_W, g_screen_h = MIN_H;
int  GetScreenWidth(void)  { return g_screen_w; }
int  GetScreenHeight(void) { return g_screen_h; }
bool IsWindowFocused(void) { return true; }
bool WindowShouldClose(void) { return false; }

void gfx_font_init(void) { }
void gfx_begin_frame(void) { }
void gfx_end_frame(void) { }
void gfx_clear(Color c) { (void)c; }
void gfx_rect(int x, int y, int w, int h, Color c) { (void)x;(void)y;(void)w;(void)h;(void)c; }
void gfx_rect_lines(int x, int y, int w, int h, Color c) { (void)x;(void)y;(void)w;(void)h;(void)c; }
void gfx_line(int a, int b, int c, int d, Color e) { (void)a;(void)b;(void)c;(void)d;(void)e; }
void gfx_rect_rounded(int x, int y, int w, int h, float r, Color c) { (void)x;(void)y;(void)w;(void)h;(void)r;(void)c; }
void gfx_rect_rounded_lines(int x, int y, int w, int h, float r, Color c) { (void)x;(void)y;(void)w;(void)h;(void)r;(void)c; }
void gfx_circle(float x, float y, float r, Color c) { (void)x;(void)y;(void)r;(void)c; }
void gfx_circle_lines(float x, float y, float r, Color c) { (void)x;(void)y;(void)r;(void)c; }
void gfx_triangle(Vector2 a, Vector2 b, Vector2 c, Color d) { (void)a;(void)b;(void)c;(void)d; }
void gfx_text(const char* t, int x, int y, int fs, Color c) { (void)t;(void)x;(void)y;(void)fs;(void)c; }
int  gfx_measure_text(const char* t, int fs) {
    int n = 0; while (t[n]) n++;
    return n * fs / 2;
}

// Point the live layout at a screen size and a board style.
static void use_screen(int w, int h, bool scaled) {
    g_screen_w = w; g_screen_h = h;
    render_set_scaled(scaled);
}

// The board's right edge: the last column plus a card.
static int board_right(const Layout* L) { return L->tab_x[6] + L->card_w; }

// A column's bottom, at the natural (uncompressed) fan.
static int natural_bottom(const Layout* L, int face_down, int face_up) {
    return L->tab_y + face_down * L->fan_down + (face_up - 1) * L->fan_up + L->card_h;
}

// --------------------------------------------------------------------------
// The desktop board never scales. At the minimum window size the layout must
// reproduce the original fixed metrics exactly, and a larger window must move
// only the margins.
// --------------------------------------------------------------------------
static void test_fixed_board_never_scales(void) {
    Layout L = layout_fixed(MIN_W, MIN_H);
    if (L.card_w != CARD_W || L.card_h != CARD_H)
        FAIL("fixed_board", "cards are not 80x112 at the minimum size");
    if (L.col_gap != 16 || L.fan_up != 28 || L.fan_down != 12 || L.waste_fan != 24)
        FAIL("fixed_board", "gap/fan metrics drifted");
    if (L.titlebar_h != 44 || L.status_h != 28)
        FAIL("fixed_board", "bar heights drifted");
    if (L.tab_x[0] != 24)
        FAIL("fixed_board", "the board is not flush against the 24px margin");
    if (board_right(&L) != MIN_W - 24)
        FAIL("fixed_board", "the board does not span the minimum width");

    // A bigger window: identical cards, board still centred.
    Layout W = layout_fixed(1600, 1000);
    if (W.card_w != CARD_W || W.card_h != CARD_H)
        FAIL("fixed_board", "cards scaled up in a large window");
    int content = board_right(&W) - W.tab_x[0];
    if (W.tab_x[0] != (1600 - content) / 2)
        FAIL("fixed_board", "the board is not centred");
    PASS("fixed_board");
}

// A browser window is not bound by SetWindowMinSize, so below the minimum the
// desktop layout shrinks to fit rather than running off the edge.
static void test_fixed_board_shrinks_below_minimum(void) {
    Layout L = layout_fixed(MIN_W / 2, MIN_H / 2);
    if (L.card_w >= CARD_W) FAIL("fixed_shrink", "cards did not shrink");
    if (board_right(&L) > MIN_W / 2)
        FAIL("fixed_shrink", "the board still overflows the viewport");
    if (L.tab_y + L.card_h + L.status_h > MIN_H / 2)
        FAIL("fixed_shrink", "the top row and status bar do not fit");
    PASS("fixed_shrink");
}

// --------------------------------------------------------------------------
// The touch board fits the screen it is given. Across a spread of real device
// shapes -- tall phone, small phone, tablet, and a phone browser held sideways
// -- every column must sit inside the viewport horizontally, and the top row
// plus a normal tableau column must fit vertically above the status bar.
// --------------------------------------------------------------------------
static void test_scaled_board_fits_every_screen(void) {
    const struct { int w, h; const char* name; } screens[] = {
        { 1080, 2400, "modern phone" },
        {  720, 1280, "small phone" },
        { 1170, 2532, "iPhone 13/14" },
        { 1536, 2048, "tablet" },
        {  800,  400, "phone browser, sideways" },
        { 2400, 1080, "phone, landscape" },
        { 2048, 1536, "tablet, landscape" },
        {  320,  480, "smallest sane screen" },
    };
    for (unsigned i = 0; i < sizeof screens / sizeof screens[0]; i++) {
        int w = screens[i].w, h = screens[i].h;
        Layout L = layout_scaled(w, h);

        if (L.card_w <= 0 || L.card_h <= 0) FAIL("scaled_fits", screens[i].name);
        // Inside the viewport, and inside its margins: a board that merely
        // "fits" by running edge to edge has silently eaten them.
        if (L.tab_x[0] < 0 || board_right(&L) > w)
            FAIL("scaled_fits", screens[i].name);
        // The top row must clear the title bar and leave the status bar alone.
        if (L.stock_y < L.titlebar_h) FAIL("scaled_fits", screens[i].name);
        if (L.tab_y + L.card_h > L.tab_bottom)
            FAIL("scaled_fits", screens[i].name);
        // A typical opening column (six face-down under one face-up) fits
        // without needing the draw-time fan compression.
        if (natural_bottom(&L, 6, 1) > L.tab_bottom)
            FAIL("scaled_fits", screens[i].name);
        // The gaps must stay positive, or columns would touch.
        if (L.col_gap <= 0 || L.fan_up <= 0 || L.fan_down <= 0)
            FAIL("scaled_fits", screens[i].name);
    }
    PASS("scaled_fits");
}

// The touch board scales with the screen: a tablet gets bigger cards than a
// phone, and both keep the 80:112 card proportion the desktop board uses.
static void test_scaled_board_scales_with_the_screen(void) {
    Layout phone  = layout_scaled(1080, 2400);
    Layout tablet = layout_scaled(1536, 2048);
    if (tablet.card_w <= phone.card_w)
        FAIL("scaled_scales", "a tablet did not get larger cards than a phone");
    if (phone.card_w <= CARD_W)
        FAIL("scaled_scales", "a 1080px-wide phone got desktop-sized cards");

    Layout all[] = { phone, tablet, layout_scaled(720, 1280) };
    for (unsigned i = 0; i < sizeof all / sizeof all[0]; i++) {
        int want = all[i].card_w * CARD_H / CARD_W;
        if (all[i].card_h != want)
            FAIL("scaled_scales", "card aspect ratio drifted from 80:112");
    }
    PASS("scaled_scales");
}

// The touch fan is spread wider than the desktop ratio, because a fingertip
// needs a bigger target than the 28/80 sliver a mouse can hit -- but never so
// wide that a run stops looking stacked.
static void test_scaled_board_widens_the_fan_for_fingers(void) {
    const struct { int w, h; } screens[] = {
        {1080, 2400}, {720, 1280}, {1536, 2048}, {2400, 1080}, {2048, 1536} };
    for (unsigned i = 0; i < sizeof screens / sizeof screens[0]; i++) {
        Layout L = layout_scaled(screens[i].w, screens[i].h);
        if (L.fan_up < L.card_w * 28 / 80)
            FAIL("scaled_fan", "the touch fan is tighter than the desktop one");
        if (L.fan_up > L.card_h * 2 / 5)
            FAIL("scaled_fan", "the touch fan spread past two fifths of a card");
        // The deepest possible column -- six face-down under a full thirteen-card
        // run -- may need the draw-time compression, but a busy one must not.
        if (natural_bottom(&L, 3, 6) > L.tab_bottom)
            FAIL("scaled_fan", "a busy column no longer fits at the wider fan");
    }
    PASS("scaled_fan");
}

// Held sideways, the board must actually use the screen. The first cut of the
// touch layout capped the card size on the short axis and then centred seven
// narrow columns in a wide screen, leaving well over half the width as bare
// felt and the cards smaller than the same phone gives in portrait. Both of
// those are asserted against here.
static void test_landscape_uses_the_width(void) {
    const struct { int w, h; const char* name; } screens[] = {
        { 2400, 1080, "phone, landscape" },
        { 1920,  900, "narrow phone, landscape" },
        { 2048, 1536, "tablet, landscape" },
    };
    for (unsigned i = 0; i < sizeof screens / sizeof screens[0]; i++) {
        Layout L = layout_scaled(screens[i].w, screens[i].h);
        int usable  = screens[i].w - 2 * L.margin_x;
        int content = board_right(&L) - L.tab_x[0];
        if (content * 10 < usable * 6)
            FAIL("landscape_width", "the board uses under 60% of the usable width");
        if (content > usable)
            FAIL("landscape_width", "the board overflows the usable width");
    }

    // Rotating a phone must not shrink the cards.
    Layout up   = layout_scaled(1080, 2400);
    Layout side = layout_scaled(2400, 1080);
    if (side.card_w < up.card_w)
        FAIL("landscape_width", "turning the phone sideways made the cards smaller");
    PASS("landscape_width");
}

// No two piles may occupy the same pixels. This is the invariant the rails
// arrangement could most easily break -- ten columns of piles placed by hand,
// where the old single row of seven could not overlap by construction.
static bool overlaps(int ax, int ay, int bx, int by, int w, int h) {
    return ax < bx + w && bx < ax + w && ay < by + h && by < ay + h;
}

static void test_piles_never_overlap(void) {
    const struct { int w, h; const char* name; } screens[] = {
        { 1170, 2289, "iPhone 12 portrait" },
        { 2250, 1107, "iPhone 12 landscape" },
        { 1080, 2400, "phone portrait" },
        { 2400, 1080, "phone landscape" },
        { 1536, 2048, "tablet portrait" },
        { 2048, 1536, "tablet landscape" },
        {  320,  480, "smallest sane screen" },
    };
    for (unsigned i = 0; i < sizeof screens / sizeof screens[0]; i++) {
        Layout L = layout_scaled(screens[i].w, screens[i].h);
        int xs[13], ys[13], n = 0;
        xs[n] = L.stock_x; ys[n++] = L.stock_y;
        xs[n] = L.waste_x; ys[n++] = L.waste_y;
        for (int f = 0; f < 4; f++) { xs[n] = L.found_x[f]; ys[n++] = L.found_y[f]; }
        for (int c = 0; c < 7; c++) { xs[n] = L.tab_x[c];   ys[n++] = L.tab_y; }
        for (int a = 0; a < n; a++) {
            // Every pile must sit inside the viewport and below the HUD band.
            if (xs[a] < 0 || xs[a] + L.card_w > screens[i].w)
                FAIL("no_overlap", screens[i].name);
            if (ys[a] < L.hud_y + L.hud_h) FAIL("no_overlap", screens[i].name);
            if (ys[a] + L.card_h > L.tab_bottom) FAIL("no_overlap", screens[i].name);
            for (int b = a + 1; b < n; b++)
                if (overlaps(xs[a], ys[a], xs[b], ys[b], L.card_w, L.card_h))
                    FAIL("no_overlap", screens[i].name);
        }
    }
    PASS("no_overlap");
}

// The touch board has no bottom bar -- on a phone that lands on the home
// indicator, cramped and in the way -- and puts the stats in a band under the
// wordmark instead. The desktop board keeps its bottom bar.
static void test_touch_board_has_no_bottom_bar(void) {
    Layout t = layout_scaled(1170, 2289);
    if (t.status_h != 0)  FAIL("no_bottom_bar", "the touch board still reserves a bottom bar");
    if (t.hud_h <= 0)     FAIL("no_bottom_bar", "the touch board has no stats band");
    if (t.hud_y < t.titlebar_h)
        FAIL("no_bottom_bar", "the stats band overlaps the wordmark bar");
    if (t.tab_bottom > 2289) FAIL("no_bottom_bar", "the tableau runs off the bottom");

    Layout d = layout_fixed(MIN_W, MIN_H);
    if (d.status_h <= 0) FAIL("no_bottom_bar", "the desktop board lost its status bar");
    PASS("no_bottom_bar");
}

// Sideways, the wordmark bar must be sized from the SCREEN, not the card. Sizing
// it from the card is circular and inflated it to 96px on an iPhone 12, eating
// the scarcest axis.
static void test_chrome_is_sized_from_the_screen(void) {
    Layout a = layout_scaled(2250, 1107);
    Layout b = layout_scaled(2250, 1107);
    if (a.titlebar_h != b.titlebar_h) FAIL("chrome", "not deterministic");
    if (a.titlebar_h > 1107 / 20)
        FAIL("chrome", "the wordmark bar is too tall for a sideways phone");
    // Same height, very different width -> same chrome, because it follows height.
    Layout narrow = layout_scaled(1400, 1107);
    if (narrow.titlebar_h != a.titlebar_h)
        FAIL("chrome", "the wordmark bar still depends on the card size");
    PASS("chrome");
}

// Rails are for phones on their side, not for anything merely wider than tall.
// They cost three extra columns of width; on a tablet at ~1.33:1 that made the
// cards SMALLER than the same tablet gives upright, which is the opposite of
// the point.
static bool uses_rails(const Layout* L) { return L->stock_x < L->tab_x[0]; }

static void test_rails_only_where_they_pay(void) {
    Layout phone = layout_scaled(2250, 1107);   // 2.03:1
    if (!uses_rails(&phone)) FAIL("rails_gate", "a phone on its side did not get rails");

    Layout tablet = layout_scaled(2048, 1536);  // 1.33:1
    if (uses_rails(&tablet)) FAIL("rails_gate", "a tablet on its side got rails");

    Layout upright = layout_scaled(1536, 2048);
    if (tablet.card_w < upright.card_w)
        FAIL("rails_gate", "turning a tablet sideways made the cards smaller");

    Layout portrait = layout_scaled(1170, 2289);
    if (uses_rails(&portrait)) FAIL("rails_gate", "an upright phone got rails");
    PASS("rails_gate");
}

// A display cutout pushes the title bar (and therefore the whole board) down,
// so nothing is ever drawn under the front camera.
static void test_scaled_board_clears_a_display_cutout(void) {
    Layout plain = layout_scaled(1080, 2400);
    // safe_area.c has no setter off Android; the tests reach its state directly,
    // which is the point of including the translation unit.
    s_top = 140;
    s_cutout_left = 460;
    s_cutout_right = 620;
    Layout notched = layout_scaled(1080, 2400);
    if (notched.titlebar_h < 140)
        FAIL("scaled_cutout", "the title bar does not clear the safe inset");
    if (notched.stock_y <= plain.stock_y)
        FAIL("scaled_cutout", "the board was not pushed below the cutout");
    s_top = s_cutout_left = s_cutout_right = 0;
    PASS("scaled_cutout");
}

// --------------------------------------------------------------------------
// The deepest column the game can produce -- six face-down cards under a full
// King-to-Ace run -- is taller than the board at the natural fan on every
// layout, so the fan compresses. It must never run past the status bar, and the
// cards must stay in order with no two sharing a top edge.
// --------------------------------------------------------------------------
static void deep_column(Pile* p) {
    p->count = 0;
    for (int i = 0; i < 6; i++) p->cards[p->count++] = (Card){5, 0, 0};
    for (int r = 13; r >= 1; r--)
        p->cards[p->count++] = (Card){(uint8_t)r, (uint8_t)((13 - r) % 2 ? 2 : 3), 1};
}

static void test_deep_column_stays_on_screen(void) {
    const struct { int w, h; bool scaled; const char* name; } cases[] = {
        { MIN_W, MIN_H, false, "desktop minimum" },
        { 1600, 1000,   false, "desktop maximised" },
        { 1080, 2400,   true,  "phone" },
        {  720, 1280,   true,  "small phone" },
        { 1536, 2048,   true,  "tablet" },
        { 2400, 1080,   true,  "phone, landscape" },
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        use_screen(cases[i].w, cases[i].h, cases[i].scaled);
        Layout L = layout_for(cases[i].w, cases[i].h);
        Pile p;
        deep_column(&p);
        if (p.count != 19) FAIL("deep_column", "the fixture is not 19 cards");

        int ys[52];
        int bottom = tab_card_ys(&L, &p, ys);
        if (bottom > L.tab_bottom)
            FAIL("deep_column", cases[i].name);
        for (int c = 1; c < p.count; c++)
            if (ys[c] <= ys[c - 1]) FAIL("deep_column", "cards share a top edge");
    }
    PASS("deep_column");
}

// --------------------------------------------------------------------------
// Hit testing is what makes a scaled board playable: pressing a card must pick
// up that card and no other. Every visible card is asked for its own position
// and then probed at it, on both layouts and across device shapes -- so a
// layout change that moved the art without moving the hit boxes would fail here
// rather than in someone's hands.
// --------------------------------------------------------------------------
static Game* mid_game(void) {
    srand(11);
    Game* g = game_create(DRAW_THREE);
    // Turn some cards up so the waste and a foundation are populated too.
    game_draw(g);
    g->foundation[0].count = 0;
    g->foundation[0].cards[g->foundation[0].count++] = (Card){1, 0, 1};
    return g;
}

static void probe(const Game* g, const Layout* L, PileKind kind, int index, int card,
                  const char* where) {
    int x, y;
    if (!card_pos_at(L, g, kind, index, card, &x, &y))
        FAIL("hit_test", "a visible card reported no position");

    // The middle of the card's exposed strip: horizontally centred, and two
    // pixels below its top edge, which is inside even the tightest fan sliver.
    PileKind hk; int hi, hc;
    if (!hit_at(L, g, x + L->card_w / 2, y + 2, &hk, &hi, &hc))
        FAIL("hit_test", where);
    if (hk != kind || hi != index || hc != card)
        FAIL("hit_test", where);
}

static void test_hit_testing_round_trips(void) {
    const struct { int w, h; bool scaled; const char* name; } cases[] = {
        { MIN_W, MIN_H, false, "desktop minimum" },
        { 1600, 1000,   false, "desktop maximised" },
        { 1080, 2400,   true,  "phone" },
        { 1536, 2048,   true,  "tablet" },
        { 2400, 1080,   true,  "phone, landscape" },
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        use_screen(cases[i].w, cases[i].h, cases[i].scaled);
        Layout L = layout_for(cases[i].w, cases[i].h);
        Game* g = mid_game();

        // Every tableau card, including the deepest column the game can make.
        deep_column(&g->tableau[0]);
        for (int c = 0; c < 7; c++)
            for (int i2 = 0; i2 < g->tableau[c].count; i2++)
                probe(g, &L, LOC_TABLEAU, c, i2, cases[i].name);

        // The waste's top card and a foundation's top card.
        probe(g, &L, LOC_WASTE, 0, g->waste.count - 1, cases[i].name);
        probe(g, &L, LOC_FOUNDATION, 0, 0, cases[i].name);

        // The stock is a click target of its own, not a card.
        if (!render_stock_hit(L.stock_x + L.card_w / 2, L.stock_y + L.card_h / 2))
            FAIL("hit_test", "the stock is not clickable");
        if (render_stock_hit(L.stock_x - 4, L.stock_y + L.card_h / 2))
            FAIL("hit_test", "the stock swallows a click beside it");

        // Bare felt below the board hits nothing.
        PileKind k; int idx, cd;
        if (hit_at(&L, g, L.tab_x[3] + L.card_w / 2, L.tab_bottom + 1,
                   &k, &idx, &cd))
            FAIL("hit_test", "empty felt reported a card");

        game_destroy(g);
    }
    PASS("hit_test");
}

// A run held over a pile must resolve to that pile, at any card size -- the drop
// target is computed from the dragged card's centre, so it has to track the
// lift the touch build applies.
static void test_drop_targeting(void) {
    use_screen(1080, 2400, true);
    Layout L = layout_for(1080, 2400);
    Game* g = mid_game();

    DragState d = {0};
    d.active = true;
    d.src_kind = LOC_WASTE;
    d.src_index = 0;
    d.src_card = g->waste.count - 1;
    d.count = 1;
    d.grab_dx = L.card_w / 2;
    d.grab_dy = L.card_h / 2;
    d.lift = L.card_h * 35 / 100;

    // Held over the second foundation, with the finger below the lifted card.
    d.mouse_x = L.found_x[1] + d.grab_dx;
    d.mouse_y = L.found_y[1] + d.grab_dy + d.lift;
    PileKind k; int idx;
    if (!drop_target_at(&L, g, &d, &k, &idx) || k != LOC_FOUNDATION || idx != 1)
        FAIL("drop_target", "a run over a foundation did not target it");

    // Held over the fifth tableau column.
    d.mouse_x = L.tab_x[4] + d.grab_dx;
    d.mouse_y = L.tab_y + d.grab_dy + d.lift;
    if (!drop_target_at(&L, g, &d, &k, &idx) || k != LOC_TABLEAU || idx != 4)
        FAIL("drop_target", "a run over a column did not target it");

    // Held out over the felt below every column: no target, so the run returns.
    d.mouse_x = L.view_w - 1;
    d.mouse_y = L.tab_bottom + L.card_h;
    if (drop_target_at(&L, g, &d, &k, &idx))
        FAIL("drop_target", "bare felt was reported as a drop target");

    game_destroy(g);
    PASS("drop_target");
}

int main(void) {
    test_fixed_board_never_scales();
    test_fixed_board_shrinks_below_minimum();
    test_scaled_board_fits_every_screen();
    test_scaled_board_scales_with_the_screen();
    test_scaled_board_widens_the_fan_for_fingers();
    test_landscape_uses_the_width();
    test_rails_only_where_they_pay();
    test_piles_never_overlap();
    test_touch_board_has_no_bottom_bar();
    test_chrome_is_sized_from_the_screen();
    test_scaled_board_clears_a_display_cutout();
    test_deep_column_stays_on_screen();
    test_hit_testing_round_trips();
    test_drop_targeting();
    printf("\nAll tests passed.\n");
    return 0;
}
