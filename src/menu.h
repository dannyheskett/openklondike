#ifndef MENU_H
#define MENU_H

// The menu every game in this family draws: a title over a list of items, one
// highlighted, in a rounded panel centred on the view. This file is identical
// in every game; only the colours (MenuTheme) come from the game.
//
// Geometry is derived from the view on every call, from its long edge, so the
// menu is the same size upright and sideways and grows with the window:
//   line height ref/20, item text ref/28, title ref/16 (shrunk to fit the
//   panel), panel width 82% of the short edge.
// If the rows would not fit the view height, everything shrinks together until
// they do.

#include "gfx.h"
#include <stdbool.h>

typedef struct {
    Color background;   // the screen behind the panel
    Color panel;        // panel fill
    Color edge;         // panel outline
    Color title;        // title text
    Color item;         // unselected items
    Color selected;     // the selected item and its > < markers
} MenuTheme;

// Clear the view and draw the menu at view_w x view_h. gap_before, if >= 0,
// inserts a blank line above that item. `capture` records each row's rectangle
// for menu_hit_test(); pass it for the window pass only (not for a recording
// pass drawn at another size).
void menu_draw(const MenuTheme* theme, int view_w, int view_h, const char* title,
               const char* const* labels, int count, int selected, int gap_before,
               bool capture);

// One whole frame showing the menu: present() (present.h) of menu_draw, so the
// window and, while recording, the capture both get it. This is how every game
// shows its menu and Options screens.
void menu_show(const MenuTheme* theme, const char* title, const char* const* labels,
               int count, int selected, int gap_before);

// The end-of-game notice drawn over a finished board: the view dimmed, then a
// panel (78% x 30% of the short edge) with `title` in the selected colour and
// `subtitle` under it ("Tap to continue" / "Press any key").
void menu_draw_notice(const MenuTheme* theme, int view_w, int view_h,
                      const char* title, const char* subtitle);

// Item index at a window point, or -1. Uses the rows captured by the last
// menu_draw() with capture set. Rows span the panel's full width, so a finger
// or a click lands on an item and not between two of them.
int menu_hit_test(Vector2 p);

#endif // MENU_H
