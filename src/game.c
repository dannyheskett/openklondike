#include "game.h"
#include <stdlib.h>
#include <string.h>

// --------------------------------------------------------------------------
// Win95 "Standard" scoring
// --------------------------------------------------------------------------
#define SC_WASTE_TO_TABLEAU      5
#define SC_TO_FOUNDATION        10
#define SC_TURN_OVER_TABLEAU     5
#define SC_FOUNDATION_TO_TABLEAU (-15)
#define SC_DRAW1_RECYCLE      (-100)   // applied per pass once draw-one recycling
#define SC_TIME_STEP            (-2)   // every 10 seconds of play
#define TICKS_PER_TIME_STEP    600     // 10s at 60fps

static void score_add(Game* g, int delta) {
    g->score += delta;
    if (g->score < 0) g->score = 0;   // Win95 never lets the score go negative
}

// --------------------------------------------------------------------------
// Pile helpers
// --------------------------------------------------------------------------
static Card* pile_top(Pile* p) { return p->count ? &p->cards[p->count - 1] : NULL; }

static void pile_push(Pile* p, Card c) { p->cards[p->count++] = c; }

static Card pile_pop(Pile* p) { return p->cards[--p->count]; }

static Pile* loc_pile(Game* g, PileKind kind, int index) {
    switch (kind) {
        case LOC_STOCK:      return &g->stock;
        case LOC_WASTE:      return &g->waste;
        case LOC_FOUNDATION: return &g->foundation[index];
        case LOC_TABLEAU:    return &g->tableau[index];
    }
    return NULL;
}

// --------------------------------------------------------------------------
// Deal
// --------------------------------------------------------------------------
Game* game_create(DrawMode mode) {
    Game* g = calloc(1, sizeof(Game));
    if (!g) return NULL;
    g->draw_mode = mode;
    g->phase     = PHASE_PLAY;

    // Build an ordered 52-card deck.
    Card deck[52];
    int n = 0;
    for (int suit = 0; suit < 4; suit++)
        for (int rank = 1; rank <= 13; rank++)
            deck[n++] = (Card){ (uint8_t)rank, (uint8_t)suit, 0 };

    // Fisher-Yates shuffle.
    for (int i = 51; i > 0; i--) {
        int j = rand() % (i + 1);
        Card t = deck[i]; deck[i] = deck[j]; deck[j] = t;
    }

    // Deal the tableau: column c gets c+1 cards, only the top one face up.
    int d = 0;
    for (int c = 0; c < 7; c++) {
        for (int r = 0; r <= c; r++) {
            Card card = deck[d++];
            card.face_up = (r == c) ? 1 : 0;
            pile_push(&g->tableau[c], card);
        }
    }
    // The rest forms the stock, all face down.
    while (d < 52) {
        Card card = deck[d++];
        card.face_up = 0;
        pile_push(&g->stock, card);
    }
    return g;
}

void game_destroy(Game* g) { free(g); }

void game_frame_begin(Game* g) { g->events = 0; }

void game_tick(Game* g) {
    if (g->phase != PHASE_PLAY) return;
    g->timer_frames++;
    // Standard timed scoring: -2 points every 10 seconds.
    int steps = g->timer_frames / TICKS_PER_TIME_STEP;
    while (g->time_penalty_steps < steps) {
        score_add(g, SC_TIME_STEP);
        g->time_penalty_steps++;
    }
}

// --------------------------------------------------------------------------
// Stock / waste
// --------------------------------------------------------------------------
bool game_draw(Game* g) {
    if (g->phase != PHASE_PLAY) return false;

    if (g->stock.count == 0) {
        if (g->waste.count == 0) return false;   // nothing to recycle
        // Recycle: pour the waste back onto the stock, reversing order and
        // turning everything face down again.
        while (g->waste.count) {
            Card c = pile_pop(&g->waste);
            c.face_up = 0;
            pile_push(&g->stock, c);
        }
        g->stock_passes++;
        if (g->draw_mode == DRAW_ONE) score_add(g, SC_DRAW1_RECYCLE);
        g->waste_drawn = 0;
        g->events |= EV_RECYCLE;
        return true;
    }

    int want = (g->draw_mode == DRAW_THREE) ? 3 : 1;
    int got = 0;
    while (got < want && g->stock.count) {
        Card c = pile_pop(&g->stock);
        c.face_up = 1;
        pile_push(&g->waste, c);
        got++;
    }
    g->waste_drawn = got;
    g->events |= EV_DRAW;
    return true;
}

// --------------------------------------------------------------------------
// Legality
// --------------------------------------------------------------------------
// A run grabbed from a tableau must be a face-up, descending, alternating-color
// sequence. From waste/foundation only the single top card is grabbable.
bool game_can_grab(const Game* g, PileKind kind, int index, int card) {
    if (g->phase != PHASE_PLAY) return false;
    const Pile* p = loc_pile((Game*)g, kind, index);
    if (!p || card < 0 || card >= p->count) return false;

    if (kind == LOC_STOCK) return false;
    if (kind == LOC_WASTE || kind == LOC_FOUNDATION)
        return card == p->count - 1;   // only the very top card

    // Tableau: the grabbed card and everything above it must be face up and
    // form a valid descending alternating-color run.
    if (!p->cards[card].face_up) return false;
    for (int i = card; i < p->count - 1; i++) {
        Card a = p->cards[i], b = p->cards[i + 1];
        if (b.rank != a.rank - 1) return false;
        if (card_is_red(a) == card_is_red(b)) return false;
    }
    return true;
}

static bool foundation_accepts(const Pile* f, Card c) {
    if (f->count == 0) return c.rank == 1;            // empty: only an Ace
    Card top = f->cards[f->count - 1];
    return c.suit == top.suit && c.rank == top.rank + 1;
}

static bool tableau_accepts(const Pile* t, Card c) {
    if (t->count == 0) return c.rank == 13;           // empty: only a King
    Card top = t->cards[t->count - 1];
    if (!top.face_up) return false;
    return c.rank == top.rank - 1 && card_is_red(c) != card_is_red(top);
}

bool game_can_drop(const Game* g, PileKind kind, int index, int card,
                   PileKind dkind, int dindex) {
    if (!game_can_grab(g, kind, index, card)) return false;
    const Pile* src = loc_pile((Game*)g, kind, index);
    int run_len = src->count - card;
    Card moving = src->cards[card];

    if (dkind == LOC_FOUNDATION) {
        if (run_len != 1) return false;               // foundations take one card
        return foundation_accepts(&g->foundation[dindex], moving);
    }
    if (dkind == LOC_TABLEAU) {
        // Can't drop a pile onto itself.
        if (kind == LOC_TABLEAU && index == dindex) return false;
        return tableau_accepts(&g->tableau[dindex], moving);
    }
    return false;   // stock/waste are never drop targets
}

// --------------------------------------------------------------------------
// Moves
// --------------------------------------------------------------------------
static void check_win(Game* g) {
    for (int i = 0; i < 4; i++)
        if (g->foundation[i].count != 13) return;
    g->phase = PHASE_WON;
    g->events |= EV_WIN;
}

bool game_move(Game* g, PileKind kind, int index, int card,
               PileKind dkind, int dindex) {
    if (!game_can_drop(g, kind, index, card, dkind, dindex)) {
        g->events |= EV_INVALID;
        return false;
    }
    Pile* src = loc_pile(g, kind, index);
    Pile* dst = loc_pile(g, dkind, dindex);
    int run_len = src->count - card;

    // Move the run, preserving order.
    for (int i = 0; i < run_len; i++)
        pile_push(dst, src->cards[card + i]);
    src->count -= run_len;
    g->moves++;

    // Scoring.
    if (dkind == LOC_FOUNDATION) {
        score_add(g, SC_TO_FOUNDATION);
    } else { // dkind == LOC_TABLEAU
        if (kind == LOC_WASTE)          score_add(g, SC_WASTE_TO_TABLEAU);
        else if (kind == LOC_FOUNDATION) score_add(g, SC_FOUNDATION_TO_TABLEAU);
        // tableau -> tableau scores nothing
    }

    // Auto-flip a newly exposed tableau card.
    if (kind == LOC_TABLEAU && src->count > 0) {
        Card* top = pile_top(src);
        if (!top->face_up) {
            top->face_up = 1;
            score_add(g, SC_TURN_OVER_TABLEAU);
            g->events |= EV_FLIP;
        }
    }

    g->events |= (dkind == LOC_FOUNDATION) ? EV_FOUNDATION : EV_MOVE;
    check_win(g);
    return true;
}

bool game_auto_move(Game* g, PileKind kind, int index, int card) {
    if (g->phase != PHASE_PLAY) return false;
    Pile* p = loc_pile(g, kind, index);
    if (!p || card < 0 || card >= p->count) return false;
    if (!game_can_grab(g, kind, index, card)) { g->events |= EV_INVALID; return false; }

    int run_len = p->count - card;

    // A single card: try to send it up to a foundation first (leftmost that
    // accepts, which routes an Ace to the first empty foundation).
    if (run_len == 1) {
        for (int f = 0; f < 4; f++)
            if (game_can_drop(g, kind, index, card, LOC_FOUNDATION, f))
                return game_move(g, kind, index, card, LOC_FOUNDATION, f);
    }
    // Otherwise search the tableau. Prefer landing on an existing build before
    // resorting to an empty column, so cards aren't shuffled around pointlessly.
    for (int t = 0; t < 7; t++)
        if (g->tableau[t].count > 0
            && game_can_drop(g, kind, index, card, LOC_TABLEAU, t))
            return game_move(g, kind, index, card, LOC_TABLEAU, t);
    for (int t = 0; t < 7; t++)
        if (g->tableau[t].count == 0
            && game_can_drop(g, kind, index, card, LOC_TABLEAU, t))
            return game_move(g, kind, index, card, LOC_TABLEAU, t);

    g->events |= EV_INVALID;
    return false;
}
