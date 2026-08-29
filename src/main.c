#include "game.h"
#include "render.h"
#include "input.h"
#include "sound.h"
#include "recorder.h"
#include "app.h"
#include "tick.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#endif

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

// Upper bound on labels[]/actions[]: one slot per MenuAction. Each action
// appears at most once, so build_menu can never overflow.
#define MAX_MENU_ITEMS 6

static void play_event_sounds(unsigned ev) {
    if (ev & EV_WIN)        { sound_play(SFX_WIN); return; }
    if (ev & EV_INVALID)      sound_play(SFX_INVALID);
    if (ev & EV_FOUNDATION)   sound_play(SFX_FOUNDATION);
    else if (ev & EV_MOVE)    sound_play(SFX_MOVE);
    if (ev & EV_FLIP)         sound_play(SFX_FLIP);
    if (ev & EV_DRAW)         sound_play(SFX_DRAW);
    if (ev & EV_RECYCLE)      sound_play(SFX_RECYCLE);
}

// Fill labels[]/actions[] with the current menu. Returns the item count and
// sets *gap_before to the index that should have a blank line above it -- Exit,
// which is set apart from the rest -- or -1 when this build has no Exit item at
// all (mobile and web, where the OS or the browser tab owns the lifecycle).
// Passing a fixed index instead put the gap above whatever happened to be last,
// which on those builds was an ordinary setting.
static int build_menu(bool resumable, DrawMode draw,
                      const char** labels, MenuAction* actions, int* gap_before) {
    int n = 0;
    *gap_before = -1;
    if (resumable) { labels[n] = "Resume Game";                     actions[n++] = ACT_RESUME; }
    labels[n] = "New Game";                                         actions[n++] = ACT_NEW;
    labels[n] = (draw == DRAW_THREE) ? "Draw: Three" : "Draw: One"; actions[n++] = ACT_DRAW;
    labels[n] = sound_is_enabled() ? "Sound: On" : "Sound: Off";    actions[n++] = ACT_SOUND;
#ifndef OK_TOUCH
    // The mp4 recorder is a desktop-only feature (stubbed out on mobile/web), so
    // the toggle would do nothing there -- omit it.
    labels[n] = recorder_active()  ? "Record: On" : "Record: Off";  actions[n++] = ACT_RECORD;
#endif
#if defined(PLATFORM_WEB)
    // A browser tab can't be closed from code, so no Exit on web.
#elif !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
    // Mobile apps don't self-terminate (the OS owns the lifecycle), so no Exit
    // on either.
    *gap_before = n;
    labels[n] = "Exit";                                             actions[n++] = ACT_EXIT;
#endif
    return n;
}

// App state carried across frames. Kept in one struct so the web and iOS builds
// can drive the loop from a per-frame callback (neither can block).
typedef struct {
    Game*     game;
    DragState drag;
    AppState  state;
    int       selected;
    DrawMode  draw_mode;
    bool      quit;
    SimClock  clock;    // fixed-timestep accumulator (drained on the menu)
    double    prev_time;// GetTime() at the previous frame; 0 before the first
    bool      had_focus;// window focus on the previous frame (touch auto-pause)
} AppCtx;

// Try to begin a drag from the card under the pointer. Returns true if a run
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
    drag->mouse_x  = in->mouse_x;
    drag->mouse_y  = in->mouse_y;
    drag->origin_x = in->mouse_x;
    drag->origin_y = in->mouse_y;
    drag->lift     = 0;
    return true;
}

// Auto-move whatever sits under a point. Used by the desktop's double-click /
// right-click and by a touch tap.
static void auto_move_at(Game* g, int x, int y) {
    PileKind kind; int index, card;
    if (render_hit(g, x, y, &kind, &index, &card))
        game_auto_move(g, kind, index, card);
}

// One iteration of the game loop. `arg` is an AppCtx* (void* to match the
// emscripten_set_main_loop callback signature).
static void frame_step(void* arg) {
    AppCtx* c = (AppCtx*)arg;

    // Real seconds since the previous frame, feeding the fixed-timestep
    // accumulator so the play clock advances at 60 Hz on any display refresh.
    // The first frame (prev_time == 0) is treated as exactly one step. The menu
    // drains the clock so it cannot hoard a burst of catch-up steps that all
    // fire the instant play resumes.
    double now = GetTime();
    double dt = (c->prev_time > 0.0) ? now - c->prev_time : SIM_DT;
    c->prev_time = now;
    if (c->state == STATE_MENU) sim_clock_reset(&c->clock);
    int steps = sim_clock_advance(&c->clock, dt);

#ifdef OK_TOUCH
    // Window focus, tracked every frame rather than only while playing: the
    // edge is what matters, and sampling it in one state only would let a
    // stale "was focused" survive across a menu visit and fire spuriously on
    // the first frame of the next game.
    bool focused = render_window_focused();
    bool focus_lost = c->had_focus && !focused;
    c->had_focus = focused;
#endif

    Input in = input_poll();
    if (in.fullscreen_toggle) render_toggle_fullscreen();

    bool resumable = (c->game != NULL && c->game->phase == PHASE_PLAY);
    const char* labels[MAX_MENU_ITEMS];
    MenuAction actions[MAX_MENU_ITEMS];
    int gap_before = -1;
    int menu_count = build_menu(resumable, c->draw_mode, labels, actions, &gap_before);
    if (c->selected >= menu_count) c->selected = 0;

    switch (c->state) {
    case STATE_MENU: {
        if (in.escape_pressed) {
            // Escape backs out: resume an in-progress game, else exit.
            if (resumable) { c->state = STATE_PLAYING; break; }
            c->quit = true; return;
        }
        if (in.menu_up) {
            c->selected = (c->selected + menu_count - 1) % menu_count;
            sound_play(SFX_MENU_MOVE);
        }
        if (in.menu_down) {
            c->selected = (c->selected + 1) % menu_count;
            sound_play(SFX_MENU_MOVE);
        }
        // Touch: a tap directly on a row selects it. A keyboard select activates
        // whatever is highlighted.
        bool do_select = in.select_pressed;
        if (in.tap) {
            int hit = render_menu_hit_test((Vector2){in.tap_x, in.tap_y});
            if (hit >= 0 && hit < menu_count) { c->selected = hit; do_select = true; }
        }
        if (do_select) {
            sound_play(SFX_MENU_SELECT);
            switch (actions[c->selected]) {
            case ACT_RESUME:
                c->state = STATE_PLAYING;
                break;
            case ACT_NEW:
                if (c->game) game_destroy(c->game);
                c->game = game_create(c->draw_mode);
                if (recorder_active()) { recorder_stop(); recorder_start(NULL); }
                sound_play(SFX_DEAL);
                c->drag.active = false;
                c->state = STATE_PLAYING;
                break;
            case ACT_DRAW:
                c->draw_mode = (c->draw_mode == DRAW_ONE) ? DRAW_THREE : DRAW_ONE;
                break;
            case ACT_SOUND:
                sound_toggle();
                sound_play(SFX_MENU_SELECT);
                break;
            case ACT_RECORD:
                recorder_toggle();
                break;
            case ACT_EXIT:
                c->quit = true; return;
            }
        }
        break;
    }

    case STATE_PLAYING: {
        if (!c->game) { c->state = STATE_MENU; break; }
#ifdef OK_TOUCH
        // Fall back to the menu when the app is backgrounded (Android) or the
        // browser tab loses focus, so the clock is not still running against a
        // player who has put the phone down. The game stays resumable.
        //
        // On the transition, not on the level: a host that never reports focus
        // at all -- a headless X server, an embedded webview -- would otherwise
        // bounce the player back to the menu on every frame, making the app
        // look broken rather than merely unfocused.
        if (focus_lost) {
            c->state = STATE_MENU;
            c->selected = 0;
            c->drag.active = false;
            break;
        }
#endif
        if (in.escape_pressed) {
            c->state = STATE_MENU;
            c->selected = 0;
            c->drag.active = false;   // an in-flight run returns to its pile
            break;
        }

        game_frame_begin(c->game);

        if (!c->drag.active) {
            if (in.auto_move_pressed) {
                // Desktop shortcut: double-click or right-click sends the card
                // under the cursor home.
                auto_move_at(c->game, in.mouse_x, in.mouse_y);
            } else if (in.left_pressed) {
                if (render_stock_hit(in.mouse_x, in.mouse_y)) game_draw(c->game);
                else                                          begin_drag(c->game, &in, &c->drag);
            } else if (in.tap) {
                // A touch tap that grabbed nothing (a face-down card, or the
                // felt) still asks for an auto-move at that point.
                auto_move_at(c->game, (int)in.tap_x, (int)in.tap_y);
            }
        } else {
            c->drag.mouse_x = in.mouse_x;
            c->drag.mouse_y = in.mouse_y;
            // Raise the run clear of the fingertip, but only once the pointer
            // has moved far enough to be a drag -- otherwise a tap would twitch
            // the card upward before flying it home. The threshold is the same
            // tap slop input.c uses to classify the gesture, so the card lifts
            // exactly when the gesture stops being a tap. A mouse never occludes
            // what it is dragging, so this is touch-only.
            if (render_use_portrait() && c->drag.lift == 0) {
                int cw  = render_card_width();
                int thr = cw / 6;
                if (thr < 8) thr = 8;
                int dx = in.mouse_x - c->drag.origin_x;
                int dy = in.mouse_y - c->drag.origin_y;
                if (dx * dx + dy * dy > thr * thr)
                    c->drag.lift = cw * CARD_H / CARD_W * 35 / 100;
            }
            if (in.left_released) {
                // Only commit when the drop is actually legal; otherwise the run
                // snaps back silently (no invalid-move buzz on a misdrop).
                PileKind dkind; int dindex;
                bool landed = false;
                if (render_drop_target(c->game, &c->drag, &dkind, &dindex)
                    && game_can_drop(c->game, c->drag.src_kind, c->drag.src_index,
                                     c->drag.src_card, dkind, dindex)) {
                    game_move(c->game, c->drag.src_kind, c->drag.src_index,
                              c->drag.src_card, dkind, dindex);
                    landed = true;
                }
                // The gesture turned out to be a tap, not a drag: send the card
                // home instead of dropping it back where it started.
                if (!landed && in.tap)
                    game_auto_move(c->game, c->drag.src_kind, c->drag.src_index,
                                   c->drag.src_card);
                c->drag.active = false;
            }
        }

        for (int s = 0; s < steps; s++) game_tick(c->game);
        play_event_sounds(c->game->events);

        if (c->game->phase == PHASE_WON) {
            c->drag.active = false;
            render_bounce_begin(c->game);
            c->state = STATE_BOUNCING;
        }
        break;
    }

    case STATE_BOUNCING:
        if (in.any_pressed) {
            render_bounce_end();
            c->state = STATE_MENU;
            c->selected = 0;
        }
        break;
    }

    // Render exactly once per frame, after the update. Every frame must reach
    // gfx_end_frame() (inside these calls) so the backend presents and polls
    // input for the next frame -- a frame that skipped it would leak the current
    // key edges into the next.
    switch (c->state) {
    case STATE_MENU:
        render_menu("OPENKLONDIKE", labels, menu_count, c->selected, gap_before);
        break;
    case STATE_PLAYING:
        render_frame(c->game, &c->drag);
        break;
    case STATE_BOUNCING:
        render_bounce_step(steps);
        break;
    }
}

static void app_ctx_init(AppCtx* c) {
    c->game      = NULL;
    c->state     = STATE_MENU;
    c->selected  = 0;
    c->draw_mode = DRAW_ONE;
    c->quit      = false;
    c->drag.active = false;
    c->had_focus = true;
    sim_clock_reset(&c->clock);
    c->prev_time = 0.0;
}

#if defined(PLATFORM_IOS)

// iOS: UIKit provides main() and the run loop, so the normal main() below is
// compiled out. The app shell (ios_main.mm) sets up the Metal layer, calls
// ok_app_init() once, then ok_app_frame() from a CADisplayLink each frame.
static AppCtx ios_ctx;

void ok_app_init(void) {
    srand((unsigned int)time(NULL));
    render_init();   // no-op on iOS (UIKit owns the window)
    sound_init();
    app_ctx_init(&ios_ctx);
}

void ok_app_frame(void) { frame_step(&ios_ctx); }

#else

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

    render_init();
    sound_init();

#ifdef PLATFORM_WEB
    // Pick the layout by the primary pointer: coarse (phone/tablet) -> the touch
    // board; fine (desktop / 2-in-1 laptop) -> the fixed desktop board and a
    // mouse, matching the native app. Only pointer:coarse is used -- the
    // maxTouchPoints/ontouchstart backstops wrongly flip touchscreen laptops.
    render_set_portrait(emscripten_run_script_int(
        "(window.matchMedia && window.matchMedia('(pointer: coarse)').matches) ? 1 : 0"));
#endif

    if (cli_record) recorder_start(cli_record_path);

    // Static so the pointer handed to emscripten stays valid after main()'s
    // stack is unwound on the web build.
    static AppCtx ctx;
    app_ctx_init(&ctx);

#ifdef PLATFORM_WEB
    // Browsers drive the loop via a per-frame callback; with the infinite-loop
    // flag this call does not return, so the native cleanup below never runs on
    // web (the browser tab owns the lifetime).
    emscripten_set_main_loop_arg(frame_step, &ctx, 0, 1);
#else
    while (!render_window_should_close() && !ctx.quit) {
        frame_step(&ctx);
    }
    recorder_stop();
    if (ctx.game) game_destroy(ctx.game);
    sound_shutdown();
    render_cleanup();
#endif
    return 0;
}

#endif // PLATFORM_IOS (main() is compiled out on iOS)
