#ifndef OPENKLONDIKE_RENDER_H
#define OPENKLONDIKE_RENDER_H

#include "game.h"
#include "platform.h"
#include "ok_types.h"
#include <stdbool.h>

// Desktop card metrics. On the landscape (desktop) layout the cards are exactly
// this size and never scale: the board is laid out at a fixed pixel size and
// centred in the window, and only the surrounding margins flex when resized.
// The window enforces MIN_W x MIN_H -- just big enough to show the board
// comfortably -- so the fixed size always fits. Both dimensions are multiples of
// 16 so the recorder can capture them.
//
// The portrait (touch) layout has no fixed card size: it fits the seven columns
// to the live screen width and derives every other metric from the result. See
// render_portrait.c.
#define CARD_W   80
#define CARD_H   112
#define MIN_W    704
#define MIN_H    704

// A run of cards picked up by the pointer. Owned by main.c, drawn by render.
typedef struct {
    bool     active;
    PileKind src_kind;
    int      src_index;
    int      src_card;   // index of the grabbed card within its source pile
    int      count;      // number of cards in the run
    Card     cards[13];  // snapshot of the dragged run (bottom..top)
    int      grab_dx;    // pointer offset from the grabbed card's top-left
    int      grab_dy;
    int      mouse_x, mouse_y;
    int      origin_x, origin_y;  // where the pointer was when the run was grabbed
    // Touch only: how far to raise the floating run above the finger so the card
    // being moved is not hidden under it. Zero until the pointer has travelled
    // far enough to be a drag rather than a tap, so a tap never twitches.
    int      lift;
} DragState;

void render_init(void);
void render_cleanup(void);
bool render_window_should_close(void);
void render_toggle_fullscreen(void);
// True while the app window holds input focus. Used to fall back to the menu
// when a touch build is sent to the background (Android suspend, browser tab).
bool render_window_focused(void);

// Scenes -------------------------------------------------------------------
void render_frame(const Game* g, const DragState* drag);
void render_menu(const char* title, const char** labels, int count,
                 int selected, int gap_before);
// Menu item index at screen point `p`, or -1. Uses the item rectangles captured
// by the last render_menu() call (touch menus).
int  render_menu_hit_test(Vector2 p);

// Win cascade: snapshot the board, then animate bouncing cards. Advances `steps`
// fixed simulation steps and draws once; returns true when every foundation card
// has been launched off-screen.
void render_bounce_begin(const Game* g);
bool render_bounce_step(int steps);
void render_bounce_end(void);

// Hit testing (uses the live window viewport) ------------------------------
// Topmost card under the point. Fills *kind/*index/*card, returns true on hit.
bool render_hit(const Game* g, int mx, int my,
                PileKind* kind, int* index, int* card);
// True if the point is on the stock pile (deal/recycle).
bool render_stock_hit(int mx, int my);
// Top-left pixel of a specific card (for computing a drag grab offset).
bool render_card_pos(const Game* g, PileKind kind, int index, int card,
                     int* x, int* y);
// Best drop target for the dragged run (foundation or tableau). The dragged
// card's position decides; returns true and fills *kind/*index when one is hit.
bool render_drop_target(const Game* g, const DragState* drag,
                        PileKind* kind, int* index);

// Card width of the live layout. The touch layer scales its gesture thresholds
// (tap slop, drag lift) off it, so they feel the same on every screen density.
int render_card_width(void);

// Active layout selection. Native builds have exactly one, so
// render_use_portrait() is a compile-time constant there (true on Android/iOS,
// false on desktop). The web build compiles both and picks at runtime:
// render_set_portrait(true) = touch layout, false = desktop layout.
void render_set_portrait(bool portrait);
bool render_use_portrait(void);

#endif
