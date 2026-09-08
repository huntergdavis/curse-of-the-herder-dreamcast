/* Host-only: render a full game frame (world+HUD+speech) to a raw buffer. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "render/fb.h"
#include "core/map/generate.h"
#include "core/sim/world.h"
#include "core/lang/speech.h"
#include "core/lang/grammar.h"
#include "core/progression.h"
int main(int argc, char **argv){
    const char *seed=argc>1?argv[1]:"seed"; int ticks=argc>2?atoi(argv[2]):40000;
    static uint16_t fb[HERDER_SCRW*HERDER_SCRH];
    herder_fb_palette_init(); herder_grammar_init();
    herder_fb_set_season(getenv("SEASON")?getenv("SEASON"):"autumn");
    HerderMap m; herder_generate_map(seed,576,&m);
    HerderWorld w; herder_world_init(&w,&m,seed);
    char line[512]="The Curse of the Herder."; int lastSeq=-1;
    for(int i=0;i<ticks && !w.finished;i++){ herder_step(&w);
      while(w.event_count>lastSeq+1){ int seq=++lastSeq; HerderUtterance u=herder_speak_for_event(&w,&w.events[seq],4); if(u.ok) snprintf(line,sizeof(line),"%s",u.text); } }
    herder_fb_set_anim(6); herder_fb_set_tint(9.0+w.tick/14400.0);
    herder_fb_draw_world(fb,&w); herder_fb_weather(fb,&w); herder_fb_minimap(fb,&w);
    herder_fb_hud(fb,&w,1);
    herder_fb_bubble(fb,&w,line);
    FILE*f=fopen(argc>3?argv[3]:"/tmp/preview.rgb565","wb"); fwrite(fb,2,HERDER_SCRW*HERDER_SCRH,f); fclose(f);
    fprintf(stderr,"%s tick %d penned %d/%d\n",seed,w.tick,w.sheepPenned,w.sheep_count);
    return 0;
}
