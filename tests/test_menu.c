// Tests for the family menu (src/menu.c), which is identical in every game:
// the panel stays inside the view at every shape, it is the same size upright
// and sideways, it grows with the window, and a click or tap on a row picks
// that row. Compiled -DPLATFORM_IOS so the types header needs no raylib; the
// gfx primitives are stubbed and record the panel and row geometry.
#define PLATFORM_IOS 1
#include "../src/menu.c"
#include "../src/present.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PASS(name) printf("PASS: %s\n", name)
#define FAIL(name, msg) do { fprintf(stderr, "FAIL: %s -- %s\n", name, msg); exit(1); } while (0)

static int g_screen_w = 960, g_screen_h = 720;
int  GetScreenWidth(void)  { return g_screen_w; }
int  GetScreenHeight(void) { return g_screen_h; }

// The panel is the first rounded rectangle drawn; remember it.
static int s_px, s_py, s_pw, s_ph, s_rounded_calls;
static int s_max_text_fs;
void gfx_begin_frame(void) { }
void gfx_end_frame(void) { }
void gfx_clear(Color c) { (void)c; s_rounded_calls = 0; s_max_text_fs = 0; }
void gfx_rect(int x, int y, int w, int h, Color c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
void gfx_rect_rounded(int x, int y, int w, int h, float r, Color c) {
    (void)r; (void)c;
    if (s_rounded_calls++ == 0) { s_px = x; s_py = y; s_pw = w; s_ph = h; }
}
void gfx_rect_rounded_lines(int x, int y, int w, int h, float r, Color c) {
    (void)x; (void)y; (void)w; (void)h; (void)r; (void)c;
}
void gfx_text(const char* t, int x, int y, int fs, Color c) {
    (void)t; (void)x; (void)y; (void)c;
    if (fs > s_max_text_fs) s_max_text_fs = fs;
}
// Roughly Nunito's advance: about 0.6 of the size per character.
int gfx_measure_text(const char* t, int fs) { return (int)strlen(t) * fs * 6 / 10; }
void gfx_circle(float x, float y, float r, Color c) { (void)x; (void)y; (void)r; (void)c; }
bool recorder_active(void) { return false; }

static const MenuTheme THEME = { {0}, {0}, {0}, {0}, {0}, {0} };
static const char* ITEMS[] = { "Resume Game", "New Game", "Options", "Sound: Off",
                               "Record: Off", "Exit" };
#define N_ITEMS 6
#define GAP 5

static void draw_at(int w, int h) {
    g_screen_w = w; g_screen_h = h;
    menu_draw(&THEME, w, h, "OPENKLONDIKE", ITEMS, N_ITEMS, 0, GAP, true);
}

static void test_fits_every_shape(void) {
    static const int shapes[][2] = {
        {640, 480}, {960, 720}, {1280, 720}, {1920, 1080}, {2560, 1080},
        {3440, 1440}, {390, 844}, {844, 390}, {820, 1180}, {1180, 820},
        {1080, 2400}, {2400, 1080}, {704, 704},
    };
    for (size_t i = 0; i < sizeof shapes / sizeof shapes[0]; i++) {
        int w = shapes[i][0], h = shapes[i][1];
        draw_at(w, h);
        if (s_px < 0 || s_py < 0 || s_px + s_pw > w || s_py + s_ph > h) {
            fprintf(stderr, "%dx%d: panel %d,%d %dx%d\n", w, h, s_px, s_py, s_pw, s_ph);
            FAIL("fits", "the panel leaves the view");
        }
    }
    PASS("fits");
}

static void test_same_size_when_turned(void) {
    draw_at(390, 844);
    int pw = s_pw, ph = s_ph, fs = s_max_text_fs;
    draw_at(844, 390);
    // Sideways the rows may have to shrink to fit the short height, but never
    // grow; upright and sideways share the long edge, so the width is equal.
    if (s_pw != pw) FAIL("turned", "the panel width changed on rotation");
    if (s_ph > ph || s_max_text_fs > fs) FAIL("turned", "the menu grew on rotation");
    draw_at(820, 1180);
    pw = s_pw; ph = s_ph; fs = s_max_text_fs;
    draw_at(1180, 820);
    if (s_pw != pw || s_ph != ph || s_max_text_fs != fs)
        FAIL("turned", "an iPad menu changed size on rotation");
    PASS("turned");
}

static void test_grows_with_window(void) {
    draw_at(640, 480);
    int small_w = s_pw, small_fs = s_max_text_fs;
    draw_at(1920, 1440);
    if (s_pw <= small_w || s_max_text_fs <= small_fs)
        FAIL("grows", "the menu did not grow with the window");
    PASS("grows");
}

static void test_hit_rows(void) {
    draw_at(960, 720);
    int cx = s_px + s_pw / 2;
    int prev = -1, found = 0;
    for (int y = s_py; y < s_py + s_ph; y++) {
        int hit = menu_hit_test((Vector2){ (float)cx, (float)y });
        if (hit >= 0 && hit != prev) {
            if (hit != found) FAIL("hit", "rows are not hit in order");
            found++;
        }
        prev = hit;
    }
    if (found != N_ITEMS) FAIL("hit", "not every row can be hit");
    if (menu_hit_test((Vector2){ (float)(s_px - 1), (float)(s_py + s_ph / 2) }) != -1)
        FAIL("hit", "a point left of the panel hit a row");
    PASS("hit");
}

int main(void) {
    test_fits_every_shape();
    test_same_size_when_turned();
    test_grows_with_window();
    test_hit_rows();
    printf("All menu tests passed.\n");
    return 0;
}
