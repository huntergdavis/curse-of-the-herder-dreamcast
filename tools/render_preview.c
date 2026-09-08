/* Host-only: render a world frame to preview.rgb565 for visual inspection. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "render/fb.h"
#include "core/map/generate.h"
#include "core/sim/world.h"
int main(int argc, char **argv){
    const char *seed = argc>1?argv[1]:"seed";
    int ticks = argc>2?atoi(argv[2]):40000;
    static uint16_t fb[HERDER_SCRW*HERDER_SCRH];
    for(int i=0;i<HERDER_SCRW*HERDER_SCRH;i++) fb[i]=0;
    herder_fb_palette_init();
    HerderMap m; herder_generate_map(seed,576,&m);
    HerderWorld w; herder_world_init(&w,&m,seed);
    for(int i=0;i<ticks && !w.finished;i++) herder_step(&w);
    /* HUD + panel backgrounds so the framing reads like the game */
    herder_fb_fill(fb,0,0,HERDER_SCRW,HERDER_TOP,HERDER_C_hud);
    herder_fb_bar(fb,HERDER_SCRW-180,8,150,12,w.frustration/100.0);
    herder_fb_draw_world(fb,&w);
    herder_fb_fill(fb,0,HERDER_SCRH-HERDER_BOT,HERDER_SCRW,HERDER_BOT,HERDER_C_panel);
    herder_fb_fill(fb,0,HERDER_SCRH-HERDER_BOT,HERDER_SCRW,2,HERDER_C_ink);
    FILE *f=fopen(argc>3?argv[3]:"/tmp/preview.rgb565","wb"); fwrite(fb,2,HERDER_SCRW*HERDER_SCRH,f); fclose(f);
    fprintf(stderr,"rendered %s @ tick %d  penned %d/%d  frustration %.0f\n", seed, w.tick, w.sheepPenned, w.sheep_count, w.frustration);
    return 0;
}
