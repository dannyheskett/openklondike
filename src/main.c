#include "game.h"
#include "render.h"
#include "input.h"
#include "sound.h"
#include "recorder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum {
    STATE_MENU,
    STATE_PLAYING,
    STATE_BOUNCING,
} AppState;

typedef enum {
    ACT_RESUME,
    ACT_NEW,
    ACT_DRAW,
    ACT_SOUND,
    ACT_RECORD,
    ACT_EXIT,
} MenuAction;

#define MAX_MENU_ITEMS 8

static void play_event_sounds(unsigned ev) {
    if (ev & EV_WIN)        { sound_play(SFX_WIN); return; }
    if (ev & EV_INVALID)      sound_play(SFX_INVALID);
    if (ev & EV_FOUNDATION)   sound_play(SFX_FOUNDATION);
    else if (ev & EV_MOVE)    sound_play(SFX_MOVE);
    if (ev & EV_FLIP)         sound_play(SFX_FLIP);
    if (ev & EV_DRAW)         sound_play(SFX_DRAW);
    if (ev & EV_RECYCLE)      sound_play(SFX_RECYCLE);
}

static int build_menu(bool resumable, DrawMode draw,
                      const char** labels, MenuAction* actions) {
    int n = 0;
    if (resumable) { labels[n] = "Resume Game";                  actions[n++] = ACT_RESUME; }
    labels[n] = "New Game";                                      actions[n++] = ACT_NEW;
    labels[n] = (draw == DRAW_THREE) ? "Draw: Three" : "Draw: One"; actions[n++] = ACT_DRAW;
    labels[n] = sound_is_enabled() ? "Sound: On" : "Sound: Off"; actions[n++] = ACT_SOUND;
    labels[n] = recorder_active()  ? "Record: On" : "Record: Off"; actions[n++] = ACT_RECORD;
    labels[n] = "Exit";                                          actions[n++] = ACT_EXIT;
    return n;
}

// Try to begin a drag from the card under the cursor. Returns true if a run
// was grabbed.
static bool begin_drag(Game* g, const Input* in, DragState* drag) {
    PileKind kind; int index, card;
    if (!render_hit(g, in->mouse_x, in->mouse_y, &kind, &index, &card)) return false;
    if (!game_can_grab(g, kind, index, card)) return false;

    int cx, cy;
    if (!render_card_pos(g, kind, index, card, &cx, &cy)) return false;

    Pile* p = (kind == LOC_WASTE) ? &g->waste
            : (kind == LOC_FOUNDATION) ? &g->foundation[index]
            : &g->tableau[index];

    drag->active    = true;
    drag->src_kind  = kind;
    drag->src_index = index;
    drag->src_card  = card;
    drag->count     = p->count - card;
    for (int i = 0; i < drag->count; i++) drag->cards[i] = p->cards[card + i];
    drag->grab_dx = in->mouse_x - cx;
    drag->grab_dy = in->mouse_y - cy;
    drag->mouse_x = in->mouse_x;
    drag->mouse_y = in->mouse_y;
    return true;
}

int main(int argc, char** argv) {
    srand((unsigned int)time(NULL));

    bool cli_record = false;
    const char* cli_record_path = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--record") == 0) {
            cli_record = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') cli_record_path = argv[++i];
        }
    }

    DrawMode draw_mode = DRAW_ONE;

    render_init();
    sound_init();
    if (cli_record) recorder_start(cli_record_path);

    Game* game = NULL;
    DragState drag = {0};
    AppState state = STATE_MENU;
    int selected = 0;

    while (!render_window_should_close()) {
        Input in = input_poll();
        if (in.fullscreen_toggle) render_toggle_fullscreen();

        bool resumable = (game != NULL && game->phase == PHASE_PLAY);
        const char* labels[MAX_MENU_ITEMS];
        MenuAction actions[MAX_MENU_ITEMS];
        int menu_count = build_menu(resumable, draw_mode, labels, actions);
        if (selected >= menu_count) selected = 0;

        switch (state) {
        case STATE_MENU:
            if (in.escape_pressed) {
                // Escape backs out: resume an in-progress game, else exit.
                if (resumable) { state = STATE_PLAYING; break; }
                goto quit;
            }
            if (in.menu_up) {
                selected = (selected + menu_count - 1) % menu_count;
                sound_play(SFX_MENU_MOVE);
            }
            if (in.menu_down) {
                selected = (selected + 1) % menu_count;
                sound_play(SFX_MENU_MOVE);
            }
            if (in.select_pressed) {
                sound_play(SFX_MENU_SELECT);
                switch (actions[selected]) {
                case ACT_RESUME:
                    state = STATE_PLAYING;
                    break;
                case ACT_NEW:
                    if (game) game_destroy(game);
                    game = game_create(draw_mode);
                    if (recorder_active()) { recorder_stop(); recorder_start(NULL); }
                    sound_play(SFX_DEAL);
                    drag.active = false;
                    state = STATE_PLAYING;
                    break;
                case ACT_DRAW:
                    draw_mode = (draw_mode == DRAW_ONE) ? DRAW_THREE : DRAW_ONE;
                    break;
                case ACT_SOUND:
                    sound_toggle();
                    sound_play(SFX_MENU_SELECT);
                    break;
                case ACT_RECORD:
                    recorder_toggle();
                    break;
                case ACT_EXIT:
                    goto quit;
                }
            }
            break;

        case STATE_PLAYING: {
            if (!game) { state = STATE_MENU; break; }
            if (in.escape_pressed) { state = STATE_MENU; selected = 0; drag.active = false; break; }

            game_frame_begin(game);

            if (!drag.active) {
                if (in.double_click || in.right_pressed) {
                    // Quick auto-move: search for a home for the card (or run)
                    // under the cursor — foundation first, then the tableau.
                    PileKind kind; int index, card;
                    if (render_hit(game, in.mouse_x, in.mouse_y, &kind, &index, &card))
                        game_auto_move(game, kind, index, card);
                } else if (in.left_pressed) {
                    if (render_stock_hit(in.mouse_x, in.mouse_y)) {
                        game_draw(game);
                    } else {
                        begin_drag(game, &in, &drag);
                    }
                }
            } else {
                // Dragging
                drag.mouse_x = in.mouse_x;
                drag.mouse_y = in.mouse_y;
                if (in.left_released) {
                    // Only commit when the drop is actually legal; otherwise the
                    // run snaps back silently (no invalid-move buzz on a misdrop).
                    PileKind dkind; int dindex;
                    if (render_drop_target(game, &drag, &dkind, &dindex)
                        && game_can_drop(game, drag.src_kind, drag.src_index,
                                         drag.src_card, dkind, dindex)) {
                        game_move(game, drag.src_kind, drag.src_index, drag.src_card,
                                  dkind, dindex);
                    }
                    drag.active = false;
                }
            }

            game_tick(game);
            play_event_sounds(game->events);

            if (game->phase == PHASE_WON) {
                render_bounce_begin(game);
                state = STATE_BOUNCING;
            }
            break;
        }

        case STATE_BOUNCING:
            if (in.any_pressed) {
                render_bounce_end();
                state = STATE_MENU;
                selected = 0;
            }
            break;
        }

        // Render exactly once per frame, after the update. Every frame must reach
        // EndDrawing() (inside these calls) so raylib polls input next frame —
        // a frame that skips it would leak the current key edges into the next.
        switch (state) {
        case STATE_MENU:
            render_menu("OPENKLONDIKE", labels, menu_count, selected, menu_count - 1);
            break;
        case STATE_PLAYING:
            render_frame(game, &drag);
            break;
        case STATE_BOUNCING:
            render_bounce_step();
            break;
        }
    }

quit:
    recorder_stop();
    if (game) game_destroy(game);
    sound_shutdown();
    render_cleanup();
    return 0;
}
