/* Curse of the Herder, Dreamcast port.
 * The byte-exact ported simulation and language pipeline, drawn to the
 * framebuffer via the portable renderer in render/fb.c, with the herder's
 * generated curses shown through the BIOS font. Start exits; D-pad reseeds.
 */
#include <kos.h>
#include <stdlib.h>
#include <string.h>
#include "core/map/generate.h"
#include "core/map/terrain.h"
#include "core/sim/world.h"
#include "core/lang/speech.h"
#include "core/lang/grammar.h"
#include "core/progression.h"
#include "render/fb.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

static uint16 *fb;

static void draw_hud(const HerderWorld *w){
    herder_fb_fill(fb,0,0,HERDER_SCRW,HERDER_TOP,HERDER_C_hud);
    char line[96];
    double hours=w->tick/14400.0;
    int hh=9+(int)hours, mm=(int)((hours-(int)hours)*60);
    int level=herder_level_for(herder_erudition(w->booksRead,w->sheepPenned,hours));
    snprintf(line,sizeof(line),"%02d:%02d  Lv%d  Pen %d/%d  Books %d", hh,mm,level,w->sheepPenned,w->sheep_count,w->booksRead);
    bfont_set_foreground_color(0xef7b);
    bfont_set_background_color(HERDER_C_hud);
    bfont_draw_str(fb+2*HERDER_SCRW+6, HERDER_SCRW, 0, line);
    herder_fb_bar(fb, HERDER_SCRW-180, 8, 150, 12, w->frustration/100.0);
}

static void draw_speech(const char *text){
    int panelY=HERDER_SCRH-HERDER_BOT;
    herder_fb_fill(fb,0,panelY,HERDER_SCRW,HERDER_BOT,HERDER_C_panel);
    herder_fb_fill(fb,0,panelY,HERDER_SCRW,2,HERDER_C_ink);
    if(!text||!text[0]) return;
    bfont_set_foreground_color(HERDER_C_ink);
    bfont_set_background_color(HERDER_C_panel);
    char buf[256]; snprintf(buf,sizeof(buf),"%s",text);
    int per=50, len=(int)strlen(buf);
    char l1[64]={0}, l2[64]={0};
    if(len<=per) snprintf(l1,sizeof(l1),"%s",buf);
    else { int cut=per; while(cut>0 && buf[cut]!=' ') cut--; if(cut==0) cut=per; strncpy(l1,buf,cut); l1[cut]=0; snprintf(l2,sizeof(l2),"%.*s",per,buf+cut+1); }
    bfont_draw_str(fb+(panelY+8)*HERDER_SCRW+8, HERDER_SCRW, 0, l1);
    if(l2[0]) bfont_draw_str(fb+(panelY+34)*HERDER_SCRW+8, HERDER_SCRW, 0, l2);
}

static const char *SEEDS[]={"seed","grudge","payoff","curse-of-the-herder","tom"};
static int seed_idx=0;
static HerderMap g_map;
static HerderWorld g_world;
static int g_inited=0;

static void new_day(void){
    if(g_inited){ herder_world_free(&g_world); herder_map_free(&g_map); }
    g_inited=1;
    herder_generate_map(SEEDS[seed_idx],576,&g_map);
    herder_world_init(&g_world,&g_map,SEEDS[seed_idx]);
}

int main(int argc, char **argv){
    (void)argc;(void)argv;
    vid_set_mode(DM_640x480, PM_RGB565);
    fb=vram_s;
    herder_fb_palette_init();
    herder_grammar_init();
    new_day();

    char curline[512]="The Curse of the Herder.";
    int lineUntil=240;
    int nextIdle=g_world.tick + herder_next_idle_curse_ticks(&g_world);
    int lastSeq=-1, frame=0, prevBtns=0;
    const int TPF=10;

    for(;;){
        maple_device_t *cont=maple_enum_type(0,MAPLE_FUNC_CONTROLLER);
        int btns=0; if(cont){ cont_state_t *st=(cont_state_t*)maple_dev_status(cont); if(st) btns=st->buttons; }
        if(btns & CONT_START) break;
        int pressed=btns & ~prevBtns; prevBtns=btns;
        if(pressed & CONT_DPAD_RIGHT){ seed_idx=(seed_idx+1)%5; new_day(); lastSeq=-1; nextIdle=herder_next_idle_curse_ticks(&g_world); snprintf(curline,sizeof(curline),"A new day: %s.",SEEDS[seed_idx]); lineUntil=frame+180; }
        if(pressed & CONT_DPAD_LEFT){ seed_idx=(seed_idx+4)%5; new_day(); lastSeq=-1; nextIdle=herder_next_idle_curse_ticks(&g_world); snprintf(curline,sizeof(curline),"A new day: %s.",SEEDS[seed_idx]); lineUntil=frame+180; }

        if(!g_world.finished){
            for(int k=0;k<TPF && !g_world.finished;k++){
                herder_step(&g_world);
                while(g_world.event_count > lastSeq+1){
                    int seq=++lastSeq;
                    HerderUtterance u=herder_speak_for_event(&g_world,&g_world.events[seq],4);
                    if(u.ok){ snprintf(curline,sizeof(curline),"%s",u.text); lineUntil=frame+(int)(u.seconds*60); }
                }
                if(g_world.tick>=nextIdle){
                    HerderUtterance u=herder_speak_idle(&g_world,4);
                    nextIdle=g_world.tick+herder_next_idle_curse_ticks(&g_world);
                    if(u.ok){ snprintf(curline,sizeof(curline),"%s",u.text); lineUntil=frame+(int)(u.seconds*60); }
                }
            }
        } else if(frame>lineUntil){
            HerderUtterance u=herder_speak_epitaph(&g_world,4); snprintf(curline,sizeof(curline),"%s",u.text); lineUntil=frame+600;
        }

        herder_fb_draw_world(fb,&g_world);
        draw_hud(&g_world);
        draw_speech(frame<lineUntil?curline:"");
        vid_waitvbl();
        frame++;
    }
    return 0;
}
