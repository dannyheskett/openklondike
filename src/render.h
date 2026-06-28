#ifndef OPENKLONDIKE_RENDER_H
#define OPENKLONDIKE_RENDER_H

#include "game.h"
#include <stdbool.h>

// Fixed card metrics — cards NEVER scale, regardless of window size.
#define CARD_W   80
#define CARD_H   112

// The board (top row + 7 tableau columns) is laid out at a fixed pixel size
// and centered in the window; only the surrounding margins flex when resized.
// The minimum window size is just big enough to show the board comfortably.
// Both dimensions are multiples of 16 so the recorder can capture them.
#define MIN_W    704
#define MIN_H    656

// A run of cards picked up by the mouse. Owned by main.c, drawn by render.
typedef struct {
    bool     active;
    PileKind src_kind;
    int      src_index;
    int      src_card;   // index of the grabbed card within its source pile
    int      count;      // number of cards in the run
    Card     cards[13];  // snapshot of the dragged run (bottom..top)
    int      grab_dx;    // mouse offset from the grabbed card's top-left
    int      grab_dy;
    int      mouse_x, mouse_y;
} DragState;

void render_init(void);
void render_cleanup(void);
bool render_window_should_close(void);
void render_toggle_fullscreen(void);

// Scenes -------------------------------------------------------------------
void render_frame(const Game* g, const DragState* drag);
void render_menu(const char* title, const char** labels, int count,
                 int selected, int gap_before);

// Win cascade: snapshot the board, then animate bouncing cards. render_bounce_step
// returns true once every foundation card has been launched off-screen.
void render_bounce_begin(const Game* g);
bool render_bounce_step(void);
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

#endif
