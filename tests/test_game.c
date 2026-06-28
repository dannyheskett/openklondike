// Unit tests for openklondike's game logic — no raylib, no window. game.c is
// included directly so its file-static helpers are visible to the tests.
// Built and run by `make test`; a non-zero exit means a failure.
#include "../src/game.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#define PASS(name) printf("PASS: %s\n", name)
#define FAIL(name, msg) do { fprintf(stderr, "FAIL: %s — %s\n", name, msg); exit(1); } while(0)

static Card mk(int rank, int suit, int up) {
    return (Card){ (uint8_t)rank, (uint8_t)suit, (uint8_t)up };
}
static void push(Pile* p, Card c) { p->cards[p->count++] = c; }

// --------------------------------------------------------------------------
// The opening deal: 28 cards in the tableau (1..7 per column, top face up),
// 24 in the stock, nothing in waste/foundations, all 52 cards present once.
// --------------------------------------------------------------------------
static void test_deal(void) {
    srand(7);
    Game* g = game_create(DRAW_ONE);
    assert(g);

    int seen[4][14] = {{0}};
    int total = 0;
    for (int c = 0; c < 7; c++) {
        if (g->tableau[c].count != c + 1) FAIL("deal", "wrong column height");
        for (int i = 0; i <= c; i++) {
            Card cd = g->tableau[c].cards[i];
            int want_up = (i == c) ? 1 : 0;
            if (cd.face_up != want_up) FAIL("deal", "wrong face-up state");
            seen[cd.suit][cd.rank]++; total++;
        }
    }
    if (g->stock.count != 24) FAIL("deal", "stock should hold 24");
    if (g->waste.count != 0)  FAIL("deal", "waste should be empty");
    for (int f = 0; f < 4; f++)
        if (g->foundation[f].count != 0) FAIL("deal", "foundation not empty");
    for (int i = 0; i < g->stock.count; i++) {
        Card cd = g->stock.cards[i];
        if (cd.face_up) FAIL("deal", "stock card face up");
        seen[cd.suit][cd.rank]++; total++;
    }
    if (total != 52) FAIL("deal", "not 52 cards");
    for (int s = 0; s < 4; s++)
        for (int r = 1; r <= 13; r++)
            if (seen[s][r] != 1) FAIL("deal", "duplicate or missing card");

    game_destroy(g);
    PASS("deal");
}

// --------------------------------------------------------------------------
// Tableau legality: descending rank + alternating color; empty takes only Kings.
// --------------------------------------------------------------------------
static void test_tableau_moves(void) {
    Game g = {0};
    push(&g.tableau[0], mk(13, 3, 1));   // King of spades (black)
    push(&g.tableau[1], mk(12, 2, 1));   // Queen of hearts (red)
    push(&g.tableau[2], mk(12, 3, 1));   // Queen of spades (black)

    if (!game_can_drop(&g, LOC_TABLEAU, 1, 0, LOC_TABLEAU, 0))
        FAIL("tableau_moves", "red Q should drop on black K");
    if (game_can_drop(&g, LOC_TABLEAU, 2, 0, LOC_TABLEAU, 0))
        FAIL("tableau_moves", "black Q must not drop on black K");

    if (!game_move(&g, LOC_TABLEAU, 1, 0, LOC_TABLEAU, 0))
        FAIL("tableau_moves", "legal move rejected");
    if (g.tableau[0].count != 2 || g.tableau[1].count != 0)
        FAIL("tableau_moves", "cards not relocated");

    // Empty column accepts only a King.
    Game e = {0};
    push(&e.tableau[3], mk(7, 1, 1));
    if (game_can_drop(&e, LOC_TABLEAU, 3, 0, LOC_TABLEAU, 0))
        FAIL("tableau_moves", "non-King dropped on empty column");
    PASS("tableau_moves");
}

// --------------------------------------------------------------------------
// A valid multi-card run moves as a unit; a non-sequence run is not grabbable.
// --------------------------------------------------------------------------
static void test_run_move(void) {
    Game g = {0};
    push(&g.tableau[0], mk(13, 3, 1));               // K spades
    push(&g.tableau[1], mk(12, 2, 1));               // Q hearts (red)
    push(&g.tableau[1], mk(11, 3, 1));               // J spades (black) -> valid run
    if (!game_can_grab(&g, LOC_TABLEAU, 1, 0))
        FAIL("run_move", "valid run not grabbable");
    if (!game_move(&g, LOC_TABLEAU, 1, 0, LOC_TABLEAU, 0))
        FAIL("run_move", "valid run move rejected");
    if (g.tableau[0].count != 3 || g.tableau[1].count != 0)
        FAIL("run_move", "run not relocated");

    Game b = {0};
    push(&b.tableau[1], mk(12, 2, 1));               // Q hearts
    push(&b.tableau[1], mk(11, 1, 1));               // J diamonds (also red) -> invalid
    if (game_can_grab(&b, LOC_TABLEAU, 1, 0))
        FAIL("run_move", "same-color run should not be grabbable");
    PASS("run_move");
}

// --------------------------------------------------------------------------
// Foundations: Ace first, then ascending in the same suit.
// --------------------------------------------------------------------------
static void test_foundation(void) {
    Game g = {0};
    push(&g.waste, mk(1, 2, 1));    // Ace hearts
    if (!game_can_drop(&g, LOC_WASTE, 0, 0, LOC_FOUNDATION, 0))
        FAIL("foundation", "Ace rejected on empty foundation");
    game_move(&g, LOC_WASTE, 0, 0, LOC_FOUNDATION, 0);

    push(&g.waste, mk(2, 1, 1));    // 2 diamonds (wrong suit)
    if (game_can_drop(&g, LOC_WASTE, 0, 0, LOC_FOUNDATION, 0))
        FAIL("foundation", "wrong-suit accepted");
    push(&g.waste, mk(2, 2, 1));    // 2 hearts (right)
    if (!game_can_drop(&g, LOC_WASTE, 1, 1, LOC_FOUNDATION, 0))
        FAIL("foundation", "correct ascending same-suit rejected");
    PASS("foundation");
}

// --------------------------------------------------------------------------
// Removing the top of a tableau column flips the newly exposed card (+5).
// --------------------------------------------------------------------------
static void test_auto_flip(void) {
    Game g = {0};
    push(&g.tableau[0], mk(5, 2, 0));    // face-down 5 hearts
    push(&g.tableau[0], mk(13, 3, 1));   // King spades on top
    int before = g.score;
    if (!game_move(&g, LOC_TABLEAU, 0, 1, LOC_TABLEAU, 1))
        FAIL("auto_flip", "king to empty column rejected");
    if (!g.tableau[0].cards[0].face_up)
        FAIL("auto_flip", "exposed card not flipped");
    if (g.score != before + 5)
        FAIL("auto_flip", "turn-over not scored +5");
    PASS("auto_flip");
}

// --------------------------------------------------------------------------
// Stock: draw-one moves one card; draw-three moves three; recycling refills it.
// --------------------------------------------------------------------------
static void test_stock(void) {
    srand(1);
    Game* g = game_create(DRAW_ONE);
    int s0 = g->stock.count;
    game_draw(g);
    if (g->stock.count != s0 - 1 || g->waste.count != 1)
        FAIL("stock", "draw-one wrong counts");
    while (g->stock.count) game_draw(g);
    if (g->waste.count != s0) FAIL("stock", "waste should hold whole stock");
    game_draw(g);             // recycle
    if (g->stock.count != s0 || g->waste.count != 0)
        FAIL("stock", "recycle did not refill stock");
    game_destroy(g);

    srand(2);
    Game* t = game_create(DRAW_THREE);
    game_draw(t);
    if (t->waste.count != 3) FAIL("stock", "draw-three should move 3");
    game_destroy(t);
    PASS("stock");
}

// --------------------------------------------------------------------------
// Completing all four foundations wins.
// --------------------------------------------------------------------------
static void test_win(void) {
    Game g = {0};
    for (int f = 0; f < 4; f++)
        for (int r = 1; r <= 12; r++)            // A..Q already up
            push(&g.foundation[f], mk(r, f, 1));
    for (int f = 0; f < 4; f++)
        push(&g.tableau[f], mk(13, f, 1));       // the four kings

    for (int f = 0; f < 4; f++) {
        if (g.phase == PHASE_WON) FAIL("win", "won too early");
        game_move(&g, LOC_TABLEAU, f, 0, LOC_FOUNDATION, f);
    }
    if (g.phase != PHASE_WON) FAIL("win", "filling foundations did not win");
    PASS("win");
}

// --------------------------------------------------------------------------
// Quick auto-move (double-click): Ace -> first empty foundation; a card with no
// foundation home lands on an existing tableau build before an empty column.
// --------------------------------------------------------------------------
static void test_auto_move(void) {
    // An Ace in a tableau goes to the leftmost empty foundation.
    Game g = {0};
    g.foundation[0].count = 1; g.foundation[0].cards[0] = mk(1, 0, 1); // club Ace occupies f0
    push(&g.tableau[0], mk(1, 2, 1));            // Ace of hearts on a column
    if (!game_auto_move(&g, LOC_TABLEAU, 0, 0))
        FAIL("auto_move", "Ace was not auto-moved");
    if (g.foundation[1].count != 1 || g.foundation[1].cards[0].rank != 1)
        FAIL("auto_move", "Ace did not land on the first empty foundation");

    // A card with no foundation move lands on an existing build, not an empty col.
    Game t = {0};
    push(&t.tableau[0], mk(13, 3, 1));           // K spades (an existing build) in col 0
    // col 1 is empty; col 2 holds a lone red Queen to move.
    push(&t.tableau[2], mk(12, 2, 1));           // Q hearts (red)
    if (!game_auto_move(&t, LOC_TABLEAU, 2, 0))
        FAIL("auto_move", "Queen was not auto-moved");
    if (t.tableau[0].count != 2)
        FAIL("auto_move", "Queen did not prefer the existing build");
    if (t.tableau[1].count != 0)
        FAIL("auto_move", "Queen should not have gone to the empty column");

    // Nothing to do: a buried/illegal target returns false.
    Game n = {0};
    push(&n.tableau[0], mk(5, 0, 1));            // lone 5 clubs, no foundation/tableau home
    if (game_auto_move(&n, LOC_TABLEAU, 0, 0))
        FAIL("auto_move", "reported a move when none was possible");
    PASS("auto_move");
}

int main(void) {
    test_deal();
    test_tableau_moves();
    test_run_move();
    test_foundation();
    test_auto_flip();
    test_stock();
    test_auto_move();
    test_win();
    printf("\nAll tests passed.\n");
    return 0;
}
