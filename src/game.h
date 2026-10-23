#ifndef ZELDA3_GAME_H_
#define ZELDA3_GAME_H_
#include "types.h"
bool ZeldaGameLoadAssets(const char *path, char *error, size_t error_size);
void ZeldaGameInitialize(uint8 extra_left_right, uint8 msu_mode, const char *language);
void ZeldaGameSetWantedFeatures(uint32 wanted_features);
bool ZeldaGameUpdate(uint16 input_state);
void ZeldaGameRender(uint8 *pixels, size_t pitch, uint32 render_flags);
#endif
