#include "tick.h"

int sim_clock_advance(SimClock* clock, double dt_real) {
    if (dt_real > 0.0) clock->accum += dt_real; // ignore zero / backward deltas

    int steps = 0;
    while (clock->accum >= SIM_DT) {
        clock->accum -= SIM_DT;
        if (++steps >= SIM_MAX_STEPS) {
            clock->accum = 0.0; // drop the backlog: skip ahead instead of spiraling
            break;
        }
    }
    return steps;
}

void sim_clock_reset(SimClock* clock) {
    clock->accum = 0.0;
}
