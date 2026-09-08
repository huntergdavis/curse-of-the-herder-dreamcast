/* Portable framebuffer renderer for the world viewport. Writes RGB565 into any
 * caller buffer (Dreamcast vram_s, or a host buffer for previews). */
#ifndef HERDER_FB_H
#define HERDER_FB_H
#include <stdint.h>
#include "core/sim/world.h"

#define HERDER_SCRW 640
#define HERDER_SCRH 480
#define HERDER_TS   22
#define HERDER_TOP  32                 /* HUD height */
#define HERDER_BOT  64                 /* speech panel height */

void herder_fb_palette_init(void);
void herder_fb_draw_world(uint16_t *fb, const HerderWorld *w);
void herder_fb_fill(uint16_t *fb, int x, int y, int w, int h, uint16_t c);
void herder_fb_bar(uint16_t *fb, int x, int y, int w, int h, double frac);

extern uint16_t HERDER_C_hud, HERDER_C_panel, HERDER_C_ink, HERDER_C_bar_bg, HERDER_C_bar;
#endif
