#ifndef OPENKLONDIKE_TICK_H
#define OPENKLONDIKE_TICK_H

// Fixed-timestep accumulator. The simulation (game.c) advances in whole 1/60 s
// steps -- the play clock and its -2/10s time penalty are counted in frames, and
// so is the win cascade -- so this converts a variable real-frame delta into the
// right number of steps to run this frame. The game then keeps the same wall
// clock regardless of the display's refresh (60, 120, 144 Hz, or an irregular
// browser rAF): a 60 Hz frame runs one step, a 120 Hz frame runs one step every
// other frame, a slow 30 Hz frame runs two. Without it the timer on a 120 Hz
// phone would run at double speed. The backlog is clamped so a long stall (a
// breakpoint, a backgrounded tab) skips ahead in time instead of triggering a
// runaway catch-up ("spiral of death"). Pure and platform-free -- the caller
// supplies the real time delta -- so the loop driver stays unit-testable without
// a window.

#define SIM_HZ        60
#define SIM_DT        (1.0 / SIM_HZ)  // seconds per simulation step
#define SIM_MAX_STEPS 5               // most steps run in a single rendered frame

typedef struct {
    double accum; // unspent real seconds carried into the next frame
} SimClock;

// Fold `dt_real` seconds into the accumulator and return how many fixed steps to
// run this frame (0..SIM_MAX_STEPS), banking the sub-step remainder for next
// time. A non-positive dt (a stalled or backward clock) contributes nothing. On
// hitting the step cap the backlog is dropped, so time skips forward rather than
// spiraling.
int sim_clock_advance(SimClock* clock, double dt_real);

// Discard any banked time. Called when leaving gameplay so the menu can't bank a
// burst of catch-up steps that all fire the instant play resumes.
void sim_clock_reset(SimClock* clock);

#endif // OPENKLONDIKE_TICK_H
