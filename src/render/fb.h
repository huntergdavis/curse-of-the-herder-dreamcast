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
void herder_fb_set_tint(double hour);
void herder_fb_set_anim(int frame);
void herder_fb_reset_camera(void);
void herder_fb_test_herder(uint16_t *fb,int x,int y,int facing,int carrying,int moving,int level,int mode);
void herder_fb_test_sheep(uint16_t *fb,int x,int y,int black,int facing,int moving,int named,int crowned,int pose);
void herder_fb_test_dog(uint16_t *fb,int x,int y,int facing,int moving);
void herder_fb_draw_world(uint16_t *fb, const HerderWorld *w);
void herder_fb_minimap(uint16_t *fb, const HerderWorld *w);
void herder_fb_weather(uint16_t *fb, const HerderWorld *w);
void herder_fb_rainbow(uint16_t *fb, int alpha8);
void herder_fb_gravestone(uint16_t *fb);
void herder_fb_title(uint16_t *fb);
int herder_fb_text(uint16_t *fb, int x, int y, const char *str, uint16_t color, int scale);
int herder_fb_text_w(const char *str, int scale);
void herder_fb_text_wrap(uint16_t *fb, int x, int y, const char *str, uint16_t color, int scale, int maxw, int maxlines);
void herder_fb_bubble(uint16_t *fb, const HerderWorld *w, const char *text);
void herder_fb_fill(uint16_t *fb, int x, int y, int w, int h, uint16_t c);
void herder_fb_bar(uint16_t *fb, int x, int y, int w, int h, double frac);
void herder_fb_hud(uint16_t *fb, const HerderWorld *w, int fast);
void herder_fb_curse_banner(uint16_t *fb, const char *line);

extern uint16_t HERDER_C_hud, HERDER_C_panel, HERDER_C_ink, HERDER_C_bar_bg, HERDER_C_bar;
#endif
