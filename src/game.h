#ifndef OPENKLONDIKE_GAME_H
#define OPENKLONDIKE_GAME_H

#include <stdint.h>
#include <stdbool.h>

// --------------------------------------------------------------------------
// Cards
//   rank: 1 = Ace .. 13 = King
//   suit: 0 = clubs, 1 = diamonds, 2 = hearts, 3 = spades
//         diamonds + hearts are red; clubs + spades are black.
// --------------------------------------------------------------------------
typedef struct {
    uint8_t rank;     // 1..13
    uint8_t suit;     // 0..3
    uint8_t face_up;  // 0/1
} Card;

static inline bool card_is_red(Card c) { return c.suit == 1 || c.suit == 2; }

// A pile is an ordered stack; cards[0] is the bottom, cards[count-1] the top.
typedef struct {
    Card cards[52];
    int  count;
} Pile;

typedef enum {
    DRAW_ONE   = 0,
    DRAW_THREE = 1,
} DrawMode;

typedef enum {
    PHASE_PLAY = 0,
    PHASE_WON,
} GamePhase;

// Which kind of pile a location refers to. `index` selects among the
// foundations (0..3) or tableau columns (0..6); ignored for stock/waste.
typedef enum {
    LOC_STOCK = 0,
    LOC_WASTE,
    LOC_FOUNDATION,
    LOC_TABLEAU,
} PileKind;

// Per-frame event flags consumed by main.c to drive sound.
enum {
    EV_DRAW       = 1 << 0,  // stock -> waste
    EV_RECYCLE    = 1 << 1,  // waste -> stock
    EV_MOVE       = 1 << 2,  // a legal tableau/waste move
    EV_FOUNDATION = 1 << 3,  // a card landed on a foundation
    EV_FLIP       = 1 << 4,  // a tableau card was turned face up
    EV_INVALID    = 1 << 5,  // an attempted move was rejected
    EV_WIN        = 1 << 6,
    EV_MENU_MOVE  = 1 << 7,
    EV_MENU_SEL   = 1 << 8,
};

typedef struct {
    Pile stock;
    Pile waste;
    Pile foundation[4];
    Pile tableau[7];

    DrawMode  draw_mode;
    GamePhase phase;

    int  score;
    int  timer_frames;   // frames since the deal (drives the clock + time penalty)
    int  time_penalty_steps;  // how many 10s penalties already applied
    int  moves;
    int  stock_passes;   // number of completed passes through the stock
    int  waste_drawn;    // cards turned in the most recent draw (1..3) — for the fan

    unsigned events;     // EV_* set this frame, cleared by game_frame_begin
} Game;

// Lifecycle ----------------------------------------------------------------
Game* game_create(DrawMode mode);   // shuffled deal, ready to play
void  game_destroy(Game* g);
void  game_frame_begin(Game* g);    // clear per-frame events
void  game_tick(Game* g);           // advance clock + apply timed scoring

// Stock --------------------------------------------------------------------
// Click the stock: deal draw_mode cards to the waste, or recycle the waste
// back into the stock when the stock is empty. Returns true if anything moved.
bool game_draw(Game* g);

// Queries ------------------------------------------------------------------
// Can the run starting at cards[card] of (kind,index) be picked up as a unit?
bool game_can_grab(const Game* g, PileKind kind, int index, int card);
// Would moving that run onto (dkind,dindex) be legal right now?
bool game_can_drop(const Game* g, PileKind kind, int index, int card,
                   PileKind dkind, int dindex);

// Moves --------------------------------------------------------------------
// Perform the move if legal: relocates the run, auto-flips an exposed tableau
// top, scores it, and checks for a win. Returns true on success.
bool game_move(Game* g, PileKind kind, int index, int card,
               PileKind dkind, int dindex);
// Double-click helper: find a home for the card (or run) starting at
// (kind,index,card) and move it there. A single card prefers a foundation
// (leftmost that accepts — so an Ace goes to the first empty one); otherwise it
// searches the tableau, preferring an existing build over an empty column.
// Returns true if a move was made.
bool game_auto_move(Game* g, PileKind kind, int index, int card);

#endif
