#include <stdint.h>
#include "../src/util.h"
bool zelda_xenon_renderer_open(uint32_t width, uint32_t height);
void zelda_xenon_renderer_begin(int width, int height, uint8_t **pixels, int *pitch);
void zelda_xenon_renderer_end(void);
void zelda_xenon_renderer_close(void);
void zelda_xenon_renderer_bind(struct RendererFuncs *funcs);
