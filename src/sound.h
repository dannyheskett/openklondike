#ifndef OPENKLONDIKE_SOUND_H
#define OPENKLONDIKE_SOUND_H

#include <stdbool.h>

typedef enum {
    SFX_DEAL = 0,
    SFX_DRAW,
    SFX_FLIP,
    SFX_MOVE,
    SFX_FOUNDATION,
    SFX_RECYCLE,
    SFX_INVALID,
    SFX_WIN,
    SFX_MENU_MOVE,
    SFX_MENU_SELECT,
    SFX_COUNT,
} SfxId;

void sound_init(void);
void sound_shutdown(void);
bool sound_is_enabled(void);
void sound_toggle(void);
void sound_play(SfxId id);

#endif
