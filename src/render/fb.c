#include "render/fb.h"
#include "core/map/terrain.h"

static uint16_t rgb565(int r, int g, int b){ return (uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3)); }
#define HEX(h) rgb565(((h)>>16)&0xff, ((h)>>8)&0xff, (h)&0xff)

static uint16_t TERRAIN_COL[16];
static uint16_t C_pen, C_tree, C_boulder, C_house, C_houseR, C_lib, C_well, C_scare, C_stump;
static uint16_t C_sheep, C_black, C_herder;
uint16_t HERDER_C_hud, HERDER_C_panel, HERDER_C_ink, HERDER_C_bar_bg, HERDER_C_bar;

void herder_fb_palette_init(void){
    TERRAIN_COL[T_Water]=HEX(0x4f8fc9); TERRAIN_COL[T_Sand]=HEX(0xe3d29a);
    TERRAIN_COL[T_Grass]=HEX(0x7cb548); TERRAIN_COL[T_Meadow]=HEX(0x8fbf50);
    TERRAIN_COL[T_Farm]=HEX(0xc9a35a); TERRAIN_COL[T_Forest]=HEX(0x5a9a44);
    TERRAIN_COL[T_Mud]=HEX(0x8d6f4e); TERRAIN_COL[T_Rock]=HEX(0x9a958c);
    TERRAIN_COL[T_Snow]=HEX(0xf2f4f7); TERRAIN_COL[T_Road]=HEX(0xd8c398);
    TERRAIN_COL[T_Bridge]=HEX(0xa8804f);
    C_pen=HEX(0x6b4a2b); C_tree=HEX(0x2f6f3a); C_boulder=HEX(0x7a746b);
    C_house=HEX(0x8c4a3a); C_houseR=HEX(0xb03a3a); C_lib=HEX(0xe0b33c);
    C_well=HEX(0x8a8578); C_scare=HEX(0x8a6238); C_stump=HEX(0x7d5a35);
    C_sheep=HEX(0xf6f2e6); C_black=HEX(0x3a3532); C_herder=HEX(0x7a5a3a);
    HERDER_C_ink=HEX(0x2b2620); HERDER_C_hud=HEX(0x28241e); HERDER_C_panel=HEX(0xf2ecdc);
    HERDER_C_bar_bg=HEX(0x50483f); HERDER_C_bar=HEX(0xc05040);
}

void herder_fb_fill(uint16_t *fb, int x, int y, int w, int h, uint16_t c){
    for(int j=y;j<y+h;j++){ if((unsigned)j>=HERDER_SCRH)continue; for(int i=x;i<x+w;i++){ if((unsigned)i<HERDER_SCRW) fb[j*HERDER_SCRW+i]=c; } }
}
void herder_fb_bar(uint16_t *fb, int x, int y, int w, int h, double frac){
    herder_fb_fill(fb,x,y,w,h,HERDER_C_bar_bg); int fwd=(int)(w*frac); if(fwd<0)fwd=0; if(fwd>w)fwd=w; if(fwd) herder_fb_fill(fb,x,y,fwd,h,HERDER_C_bar);
}

static int deco_col(int d, uint16_t *out){
    switch(d){
        case D_Tree: case D_Tree2: *out=C_tree; return 1;
        case D_Boulder: *out=C_boulder; return 1;
        case D_House: *out=C_house; return 1;
        case D_HouseRed: *out=C_houseR; return 1;
        case D_Library: *out=C_lib; return 1;
        case D_Well: *out=C_well; return 1;
        case D_Scarecrow: *out=C_scare; return 1;
        case D_Stump: *out=C_stump; return 1;
        case D_Fence: case D_PenGround: *out=C_pen; return 1;
        default: return 0;
    }
}

#define VIEWW (HERDER_SCRW / HERDER_TS)
#define VIEWH ((HERDER_SCRH - HERDER_TOP - HERDER_BOT) / HERDER_TS)

void herder_fb_draw_world(uint16_t *fb, const HerderWorld *w){
    const HerderMap *m = w->map;
    int cx=(int)(w->h.x+0.5), cy=(int)(w->h.y+0.5);
    int ox=cx-VIEWW/2, oy=cy-VIEWH/2;
    for(int ty=0;ty<VIEWH;ty++){ int my=oy+ty;
        for(int tx=0;tx<VIEWW;tx++){ int mx=ox+tx; uint16_t c;
            if(mx<0||my<0||mx>=m->size||my>=m->size) c=HERDER_C_ink;
            else { int i=my*m->size+mx; c=TERRAIN_COL[m->terrain[i]]; uint16_t dc; if(deco_col(m->deco[i],&dc)) c=dc; }
            herder_fb_fill(fb, tx*HERDER_TS, HERDER_TOP+ty*HERDER_TS, HERDER_TS, HERDER_TS, c);
        }
    }
    for(int i=0;i<w->lib_count;i++){ if(w->lib[i].taken) continue; int sx=(w->lib[i].x-ox)*HERDER_TS, sy=HERDER_TOP+(w->lib[i].y-oy)*HERDER_TS; if(sx>=0&&sy>=HERDER_TOP) herder_fb_fill(fb,sx,sy,HERDER_TS,HERDER_TS,C_lib); }
    for(int i=0;i<w->sheep_count;i++){ const HerderSheep*s=&w->sheep[i]; if(s->mode==2) continue; int sx=(int)((s->x-ox)*HERDER_TS), sy=HERDER_TOP+(int)((s->y-oy)*HERDER_TS); herder_fb_fill(fb,sx,sy,HERDER_TS-1,HERDER_TS-1, s->black?C_black:C_sheep); }
    { int sx=(int)((w->h.x-ox)*HERDER_TS), sy=HERDER_TOP+(int)((w->h.y-oy)*HERDER_TS);
      herder_fb_fill(fb,sx-1,sy-1,HERDER_TS+1,HERDER_TS+1,C_herder);
      int fxo=w->h.facing==0?HERDER_TS:w->h.facing==2?-2:0, fyo=w->h.facing==1?HERDER_TS:w->h.facing==3?-2:0;
      herder_fb_fill(fb,sx+fxo,sy+fyo,2,2,HERDER_C_ink);
    }
}
