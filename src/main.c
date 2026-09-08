/* Curse of the Herder, Dreamcast port.
 * The byte-exact ported simulation and language pipeline, drawn to the
 * framebuffer (the path proven to render under reios), with the herder's
 * generated curses shown via the BIOS font. Start exits; D-pad reseeds.
 */
#include <kos.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "core/map/generate.h"
#include "core/map/terrain.h"
#include "core/sim/world.h"
#include "core/lang/speech.h"
#include "core/lang/grammar.h"
#include "core/progression.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#define SCRW 640
#define SCRH 480
#define TS 5                 /* pixels per tile in the viewport */
#define VIEWW (SCRW / TS)    /* tiles across */
#define VIEWH ((SCRH - 96) / TS) /* tiles down (96px reserved for HUD+bubble) */

static uint16 rgb565(int r, int g, int b){ return (uint16)(((r>>3)<<11)|((g>>2)<<5)|(b>>3)); }

/* terrain palette, indexed by T_* */
static uint16 TERRAIN_COL[16];
static uint16 C_pen, C_house, C_houseR, C_tree, C_lib, C_well, C_boulder, C_scare;
static uint16 C_sheep, C_black, C_herder, C_ink, C_panel, C_hud, C_bar, C_barbg;

static void pal_init(void){
    TERRAIN_COL[T_Water]  = rgb565(0x3f,0x6f,0x9f);
    TERRAIN_COL[T_Sand]   = rgb565(0xd8,0xc8,0x90);
    TERRAIN_COL[T_Grass]  = rgb565(0x8c,0xa8,0x50);
    TERRAIN_COL[T_Meadow] = rgb565(0x9c,0xb8,0x5c);
    TERRAIN_COL[T_Farm]   = rgb565(0xa9,0x8b,0x52);
    TERRAIN_COL[T_Forest] = rgb565(0x4d,0x6b,0x3a);
    TERRAIN_COL[T_Rock]   = rgb565(0x8a,0x86,0x80);
    TERRAIN_COL[T_Snow]   = rgb565(0xe8,0xec,0xf0);
    TERRAIN_COL[T_Mud]    = rgb565(0x6f,0x5a,0x3f);
    TERRAIN_COL[T_Road]   = rgb565(0xb0,0x9c,0x74);
    TERRAIN_COL[T_Bridge] = rgb565(0x8a,0x6a,0x45);
    C_pen=rgb565(0x6b,0x4a,0x2b); C_house=rgb565(0xb8,0x9a,0x6a); C_houseR=rgb565(0xa6,0x5a,0x44);
    C_tree=rgb565(0x35,0x55,0x2c); C_lib=rgb565(0xc9,0x9a,0x3c); C_well=rgb565(0x55,0x6a,0x86);
    C_boulder=rgb565(0x9a,0x96,0x90); C_scare=rgb565(0x8a,0x6a,0x35);
    C_sheep=rgb565(0xf6,0xf2,0xe6); C_black=rgb565(0x30,0x2c,0x28); C_herder=rgb565(0x2a,0x40,0x86);
    C_bar=rgb565(0xc0,0x50,0x40); C_barbg=rgb565(0x50,0x48,0x40);
    C_panel=rgb565(0xf2,0xec,0xdc);
}

static uint16 *fb;
static void fill(int x,int y,int w,int h,uint16 c){ for(int j=y;j<y+h;j++){ if((unsigned)j>=SCRH)continue; for(int i=x;i<x+w;i++){ if((unsigned)i<SCRW) fb[j*SCRW+i]=c; } } }

static uint16 deco_col(int d, int *drawn){
    *drawn=1;
    switch(d){
        case D_House: return C_house; case D_HouseRed: return C_houseR;
        case D_Tree: case D_Tree2: return C_tree; case D_Library: return C_lib; case D_Well: return C_well;
        case D_Boulder: return C_boulder; case D_Scarecrow: return C_scare;
        default: *drawn=0; return 0;
    }
}

/* draw the world viewport centred on the herder */
static void draw_world(const HerderWorld *w){
    const HerderMap *m = w->map;
    int cx = (int)(w->h.x+0.5), cy = (int)(w->h.y+0.5);
    int ox = cx - VIEWW/2, oy = cy - VIEWH/2;
    for(int ty=0; ty<VIEWH; ty++){
        int my = oy+ty;
        for(int tx=0; tx<VIEWW; tx++){
            int mx = ox+tx;
            uint16 c;
            if(mx<0||my<0||mx>=m->size||my>=m->size) c=C_ink;
            else {
                int i=my*m->size+mx;
                c = TERRAIN_COL[m->terrain[i]];
                int drawn; uint16 dc=deco_col(m->deco[i],&drawn); if(drawn) c=dc;
                if(m->deco[i]==D_Fence||m->deco[i]==D_PenGround) c=C_pen;
            }
            fill(tx*TS, 32+ty*TS, TS, TS, c);
        }
    }
    /* libraries not-yet-taken as bright markers */
    for(int i=0;i<w->lib_count;i++){ if(w->lib[i].taken) continue; int sx=(w->lib[i].x-ox)*TS, sy=32+(w->lib[i].y-oy)*TS; if(sx>=0&&sy>=32) fill(sx,sy,TS,TS,C_lib); }
    /* sheep */
    for(int i=0;i<w->sheep_count;i++){ const HerderSheep*s=&w->sheep[i]; if(s->mode==2) continue; int sx=(int)((s->x-ox)*TS), sy=32+(int)((s->y-oy)*TS); uint16 col=s->black?C_black:C_sheep; fill(sx,sy,TS-1,TS-1,col); }
    /* herder */
    { int sx=(int)((w->h.x-ox)*TS), sy=32+(int)((w->h.y-oy)*TS); fill(sx-1,sy-1,TS+1,TS+1,C_herder);
      /* facing pip */
      int fxo = w->h.facing==0?TS:w->h.facing==2?-2:0, fyo = w->h.facing==1?TS:w->h.facing==3?-2:0;
      fill(sx+fxo, sy+fyo, 2,2, C_ink);
    }
}

static void draw_bar(int x,int y,int w,int h,double frac,uint16 c){ fill(x,y,w,h,C_barbg); int fwd=(int)(w*frac); if(fwd>0) fill(x,y,fwd,h,c); }

static void draw_hud(const HerderWorld *w){
    fill(0,0,SCRW,32,C_hud);
    char line[96];
    double hours = w->tick/14400.0;
    int hh = 9+(int)hours, mm=(int)((hours-(int)hours)*60);
    int level = herder_level_for(herder_erudition(w->booksRead, w->sheepPenned, hours));
    snprintf(line,sizeof(line),"%02d:%02d  Lv%d  Pen %d/%d  Books %d", hh,mm, level, w->sheepPenned, w->sheep_count, w->booksRead);
    bfont_set_foreground_color(rgb565(0xf0,0xea,0xda));
    bfont_set_background_color(C_hud);
    bfont_draw_str(fb+2*SCRW+6, SCRW, 0, line);
    /* frustration bar */
    draw_bar(SCRW-180, 8, 150, 12, w->frustration/100.0, C_bar);
}

/* word-wrapped speech in the bottom panel */
static void draw_speech(const char *text){
    int panelY = SCRH-64;
    fill(0,panelY,SCRW,64,C_panel);
    fill(0,panelY,SCRW,2,C_ink);
    bfont_set_foreground_color(C_ink);
    bfont_set_background_color(C_panel);
    /* wrap at ~50 chars, up to 2 lines */
    char buf[256]; snprintf(buf,sizeof(buf),"%s",text);
    int per=50; int len=(int)strlen(buf);
    char l1[64]={0}, l2[64]={0};
    if(len<=per){ snprintf(l1,sizeof(l1),"%s",buf); }
    else{ int cut=per; while(cut>0 && buf[cut]!=' ') cut--; if(cut==0) cut=per; strncpy(l1,buf,cut); l1[cut]=0; snprintf(l2,sizeof(l2),"%.*s",per,buf+cut+1); }
    bfont_draw_str(fb+(panelY+8)*SCRW+8, SCRW, 0, l1);
    if(l2[0]) bfont_draw_str(fb+(panelY+34)*SCRW+8, SCRW, 0, l2);
}

static const char *SEEDS[] = {"seed","grudge","payoff","curse-of-the-herder","tom"};
static int seed_idx = 0;
static HerderMap g_map;
static HerderWorld g_world;

static void new_day(void){
    static int inited=0;
    if(inited){ herder_world_free(&g_world); herder_map_free(&g_map); }
    inited=1;
    herder_generate_map(SEEDS[seed_idx], 576, &g_map);
    herder_world_init(&g_world, &g_map, SEEDS[seed_idx]);
}

int main(int argc, char **argv){
    (void)argc;(void)argv;
    vid_set_mode(DM_640x480, PM_RGB565);
    fb = vram_s;
    pal_init();
    herder_grammar_init();
    new_day();

    char curline[512] = "The Curse of the Herder.";
    int lineUntil = 240;
    int nextIdle = g_world.tick + herder_next_idle_curse_ticks(&g_world);
    int lastSeq = -1;
    int frame = 0;
    int prevBtns = 0;
    const int TICKS_PER_FRAME = 10;

    for(;;){
        maple_device_t *cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        int btns=0;
        if(cont){ cont_state_t *st=(cont_state_t*)maple_dev_status(cont); if(st) btns=st->buttons; }
        if(btns & CONT_START) break;
        int pressed = btns & ~prevBtns; prevBtns=btns;
        if(pressed & CONT_DPAD_RIGHT){ seed_idx=(seed_idx+1)%5; new_day(); lastSeq=-1; nextIdle=herder_next_idle_curse_ticks(&g_world); snprintf(curline,sizeof(curline),"A new day: %s.",SEEDS[seed_idx]); lineUntil=frame+180; }
        if(pressed & CONT_DPAD_LEFT){ seed_idx=(seed_idx+4)%5; new_day(); lastSeq=-1; nextIdle=herder_next_idle_curse_ticks(&g_world); snprintf(curline,sizeof(curline),"A new day: %s.",SEEDS[seed_idx]); lineUntil=frame+180; }

        /* advance the sim */
        if(!g_world.finished){
            for(int k=0;k<TICKS_PER_FRAME && !g_world.finished;k++){
                herder_step(&g_world);
                /* speak new events */
                while(g_world.event_count > lastSeq+1){
                    int seq=++lastSeq;
                    HerderUtterance u = herder_speak_for_event(&g_world, &g_world.events[seq], 4);
                    if(u.ok){ snprintf(curline,sizeof(curline),"%s",u.text); lineUntil=frame + (int)(u.seconds*60); }
                }
                /* idle curses */
                if(g_world.tick >= nextIdle){
                    HerderUtterance u = herder_speak_idle(&g_world, 4);
                    nextIdle = g_world.tick + herder_next_idle_curse_ticks(&g_world);
                    if(u.ok){ snprintf(curline,sizeof(curline),"%s",u.text); lineUntil=frame + (int)(u.seconds*60); }
                }
            }
        } else {
            /* day over: a gravestone line, then hold */
            if(frame > lineUntil){ HerderUtterance u=herder_speak_epitaph(&g_world,4); snprintf(curline,sizeof(curline),"%s",u.text); lineUntil=frame+600; }
        }

        draw_world(&g_world);
        draw_hud(&g_world);
        draw_speech(frame<lineUntil?curline:"");
        vid_waitvbl();
        frame++;
    }
    return 0;
}
