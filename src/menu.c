#include "menu.h"
#include "present.h"

#define MENU_MAX_ROWS 12

static struct { float x, y, w, h; } s_rows[MENU_MAX_ROWS];
static int s_row_count = 0;

void menu_draw(const MenuTheme* t, int view_w, int view_h, const char* title,
               const char* const* labels, int count, int selected, int gap_before,
               bool capture) {
    int rows = count + ((gap_before >= 0) ? 1 : 0);
    int ref  = (view_w > view_h) ? view_w : view_h;   // the long edge
    int base = (view_w < view_h) ? view_w : view_h;   // the short edge

    // Title, spacing, rows and bottom padding total (title + (rows + 3) lines)
    // = ref/16 + (rows + 3) * ref/20. Shrink the reference until that fits in
    // 90% of the view height.
    int avail = view_h * 9 / 10;
    int need  = ref / 16 + (rows + 3) * (ref / 20);
    if (need > avail && need > 0) ref = (int)((long)ref * avail / need);

    int line_h  = ref / 20;
    int item_fs = ref / 28;
    int panel_w = base * 82 / 100;
    int title_fs = ref / 16;
    while (title_fs > 12 && gfx_measure_text(title, title_fs) > panel_w * 9 / 10)
        title_fs--;

    int panel_h = title_fs + (rows + 3) * line_h;
    int cx = view_w / 2;
    int px = cx - panel_w / 2;
    int py = (view_h - panel_h) / 2;
    if (py < 0) py = 0;

    gfx_clear(t->background);
    gfx_rect_rounded(px, py, panel_w, panel_h, 0.05f, t->panel);
    gfx_rect_rounded_lines(px, py, panel_w, panel_h, 0.05f, t->edge);
    gfx_text(title, cx - gfx_measure_text(title, title_fs) / 2, py + line_h,
             title_fs, t->title);

    if (capture) s_row_count = (count < MENU_MAX_ROWS) ? count : MENU_MAX_ROWS;
    int y = py + line_h + title_fs + line_h;
    for (int i = 0; i < count; i++) {
        if (i == gap_before) y += line_h;
        const char* label = labels[i];
        int lw = gfx_measure_text(label, item_fs);
        if (i == selected) {
            int gap = item_fs / 2;
            gfx_text(">", cx - lw / 2 - gap - gfx_measure_text(">", item_fs), y,
                     item_fs, t->selected);
            gfx_text("<", cx + lw / 2 + gap, y, item_fs, t->selected);
        }
        gfx_text(label, cx - lw / 2, y, item_fs, (i == selected) ? t->selected : t->item);
        if (capture && i < MENU_MAX_ROWS) {
            s_rows[i].x = (float)px;
            s_rows[i].y = (float)(y - (line_h - item_fs) / 2);
            s_rows[i].w = (float)panel_w;
            s_rows[i].h = (float)line_h;
        }
        y += line_h;
    }
}

void menu_draw_notice(const MenuTheme* t, int view_w, int view_h,
                      const char* title, const char* subtitle) {
    int base = (view_w < view_h) ? view_w : view_h;
    int pw = base * 78 / 100, ph = base * 30 / 100;
    int px = view_w / 2 - pw / 2, py = view_h / 2 - ph / 2;
    gfx_rect(0, 0, view_w, view_h, (Color){ 0, 0, 0, 150 });
    gfx_rect_rounded(px, py, pw, ph, 0.08f, t->panel);
    gfx_rect_rounded_lines(px, py, pw, ph, 0.08f, t->edge);
    int ts = ph * 26 / 100, ss = ph * 13 / 100;
    while (ts > 12 && gfx_measure_text(title, ts) > pw - ss * 2) ts -= 2;
    gfx_text(title, view_w / 2 - gfx_measure_text(title, ts) / 2,
             py + ph * 22 / 100, ts, t->selected);
    gfx_text(subtitle, view_w / 2 - gfx_measure_text(subtitle, ss) / 2,
             py + ph * 62 / 100, ss, t->item);
}

int menu_hit_test(Vector2 p) {
    for (int i = 0; i < s_row_count; i++) {
        if (p.x >= s_rows[i].x && p.x < s_rows[i].x + s_rows[i].w &&
            p.y >= s_rows[i].y && p.y < s_rows[i].y + s_rows[i].h) return i;
    }
    return -1;
}

typedef struct {
    const MenuTheme* theme; const char* title; const char* const* labels;
    int count, selected, gap_before;
} MenuScene;

static void menu_scene(void* vctx, int view_w, int view_h) {
    const MenuScene* m = (const MenuScene*)vctx;
    // Rows are captured only from the window pass (drawn at the live window
    // size), never from the recorder's fixed-size pass.
    bool window_pass = (view_w == GetScreenWidth() && view_h == GetScreenHeight());
    menu_draw(m->theme, view_w, view_h, m->title, m->labels, m->count,
              m->selected, m->gap_before, window_pass);
}

void menu_show(const MenuTheme* theme, const char* title, const char* const* labels,
               int count, int selected, int gap_before) {
    MenuScene m = { theme, title, labels, count, selected, gap_before };
    present(menu_scene, &m);
}
