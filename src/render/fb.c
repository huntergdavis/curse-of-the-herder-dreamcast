#include "render/fb.h"
#include "core/map/terrain.h"

static uint16_t rgb565(int r, int g, int b){ return (uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3)); }
#define HEX(h) rgb565(((h)>>16)&0xff, ((h)>>8)&0xff, (h)&0xff)

static uint16_t TERRAIN_COL[16];
static uint16_t C_pen, C_penground, C_tree, C_boulder, C_house, C_houseR, C_lib, C_well, C_scare, C_stump;
static uint16_t C_sheep, C_black, C_herder;
uint16_t HERDER_C_hud, HERDER_C_panel, HERDER_C_ink, HERDER_C_bar_bg, HERDER_C_bar;

void herder_fb_palette_init(void){
    TERRAIN_COL[T_Water]=HEX(0x4f8fc9); TERRAIN_COL[T_Sand]=HEX(0xe3d29a);
    TERRAIN_COL[T_Grass]=HEX(0x7cb548); TERRAIN_COL[T_Meadow]=HEX(0x8fbf50);
    TERRAIN_COL[T_Farm]=HEX(0xc9a35a); TERRAIN_COL[T_Forest]=HEX(0x5a9a44);
    TERRAIN_COL[T_Mud]=HEX(0x8d6f4e); TERRAIN_COL[T_Rock]=HEX(0x9a958c);
    TERRAIN_COL[T_Snow]=HEX(0xf2f4f7); TERRAIN_COL[T_Road]=HEX(0xd8c398);
    TERRAIN_COL[T_Bridge]=HEX(0xa8804f);
    C_pen=HEX(0x6b4a2b); C_penground=HEX(0x93894f); C_tree=HEX(0x2f6f3a); C_boulder=HEX(0x7a746b);
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

#define VIEWW (HERDER_SCRW / HERDER_TS + 2)
#define VIEWH ((HERDER_SCRH - HERDER_TOP - HERDER_BOT) / HERDER_TS + 2)

static void disc(uint16_t *fb, int cx, int cy, int r, uint16_t c){
    for(int dy=-r;dy<=r;dy++) for(int dx=-r;dx<=r;dx++) if(dx*dx+dy*dy<=r*r){ int x=cx+dx,y=cy+dy; if((unsigned)x<HERDER_SCRW&&(unsigned)y<HERDER_SCRH) fb[y*HERDER_SCRW+x]=c; }
}
static void rrect(uint16_t *fb,int x,int y,int w,int h,uint16_t c){ herder_fb_fill(fb,x+1,y,w-2,h,c); herder_fb_fill(fb,x,y+1,w,h-2,c); }

/* subtle per-tile texture fleck, echoing the web's grass/meadow speckle */
static void tex_fleck(uint16_t *fb,int px,int py,int mx,int my,int t){
    if(t!=T_Grass&&t!=T_Meadow&&t!=T_Mud&&t!=T_Farm) return;
    unsigned h=(unsigned)(mx*37+my*101);
    if(h%5==0){ int ox=(int)(h%7)%(HERDER_TS-2), oy=(int)((h/7)%7)%(HERDER_TS-2); uint16_t base=fb[(py+oy)*HERDER_SCRW+px+ox]; uint16_t d=(uint16_t)((base>>1)&0x7bef); herder_fb_fill(fb,px+ox,py+oy,2,2,d); }
}

static void draw_sheep(uint16_t *fb,int cx,int cy,int black,int facing){
    uint16_t wool=black?C_black:C_sheep; uint16_t face=black?HEX(0xe8e2d4):HEX(0x3a2f2a);
    /* legs */
    herder_fb_fill(fb,cx-5,cy+5,2,4,face); herder_fb_fill(fb,cx+3,cy+5,2,4,face);
    /* body */
    disc(fb,cx,cy+1,7,wool); disc(fb,cx-4,cy,5,wool); disc(fb,cx+4,cy,5,wool);
    /* head to the facing side */
    int hx=facing==2?-8:8; disc(fb,cx+hx,cy-1,3,face);
}
static void draw_herder(uint16_t *fb,int cx,int cy,int facing,int carrying){
    uint16_t coat=C_herder, skin=HEX(0xe8b98a), hat=HERDER_C_ink;
    herder_fb_fill(fb,cx-4,cy-2,8,12,coat);         /* body */
    disc(fb,cx,cy-8,4,skin);                        /* head */
    herder_fb_fill(fb,cx-5,cy-11,10,3,hat);         /* hat brim */
    herder_fb_fill(fb,cx-3,cy-14,6,3,hat);          /* hat crown */
    int px=facing==0?4:facing==2?-6:-1;             /* crook / facing */
    herder_fb_fill(fb,cx+px,cy-6,2,12,HEX(0x8a6a3a));
    if(carrying) disc(fb,cx,cy-3,4,C_sheep);        /* a sheep on the shoulders */
}
static void draw_tree(uint16_t *fb,int cx,int cy){ herder_fb_fill(fb,cx-1,cy,3,7,HEX(0x6b4a2b)); disc(fb,cx,cy-3,7,C_tree); }
static void draw_house(uint16_t *fb,int px,int py,int red){
    uint16_t wall=red?HEX(0xd8b28a):HEX(0xe8dcc3), roof=red?HEX(0xb03a3a):HEX(0x8c4a3a);
    herder_fb_fill(fb,px+3,py+HERDER_TS/2,HERDER_TS-6,HERDER_TS/2-1,wall);
    for(int r=0;r<HERDER_TS/2;r++) herder_fb_fill(fb,px+2+r/2,py+r,HERDER_TS-4-r,1,roof);
}

void herder_fb_draw_world(uint16_t *fb, const HerderWorld *w){
    const HerderMap *m=w->map;
    int cx=(int)(w->h.x+0.5), cy=(int)(w->h.y+0.5);
    int ox=cx-VIEWW/2, oy=cy-VIEWH/2;
    int vh=(HERDER_SCRH-HERDER_TOP-HERDER_BOT);
    for(int ty=0;ty<VIEWH;ty++){ int my=oy+ty; int py=HERDER_TOP+ty*HERDER_TS; if(py>=HERDER_TOP+vh) break;
        for(int tx=0;tx<VIEWW;tx++){ int mx=ox+tx; int px=tx*HERDER_TS;
            uint16_t c; int t=T_Water,d=D_None;
            if(mx<0||my<0||mx>=m->size||my>=m->size) c=HERDER_C_ink;
            else { int i=my*m->size+mx; t=m->terrain[i]; d=m->deco[i]; c=TERRAIN_COL[t];
                   if(d==D_PenGround) c=C_penground; else if(d==D_Fence) c=C_pen; }
            herder_fb_fill(fb,px,py,HERDER_TS,HERDER_TS,c);
            if(mx>=0&&my>=0&&mx<m->size&&my<m->size){ tex_fleck(fb,px,py,mx,my,t);
                switch(d){
                    case D_Tree: case D_Tree2: draw_tree(fb,px+HERDER_TS/2,py+HERDER_TS/2); break;
                    case D_House: draw_house(fb,px,py,0); break;
                    case D_HouseRed: draw_house(fb,px,py,1); break;
                    case D_Boulder: disc(fb,px+HERDER_TS/2,py+HERDER_TS/2,HERDER_TS/3,C_boulder); break;
                    case D_Well: disc(fb,px+HERDER_TS/2,py+HERDER_TS/2,HERDER_TS/3,C_well); break;
                    case D_Scarecrow: herder_fb_fill(fb,px+HERDER_TS/2-1,py+2,2,HERDER_TS-4,C_scare); herder_fb_fill(fb,px+3,py+HERDER_TS/3,HERDER_TS-6,2,C_scare); break;
                    case D_Stump: disc(fb,px+HERDER_TS/2,py+HERDER_TS/2,4,C_stump); break;
                    default: break;
                }
            }
        }
    }
    /* library book-boxes */
    for(int i=0;i<w->lib_count;i++){ if(w->lib[i].taken) continue; int sx=(w->lib[i].x-ox)*HERDER_TS+HERDER_TS/2, sy=HERDER_TOP+(w->lib[i].y-oy)*HERDER_TS+HERDER_TS/2; rrect(fb,sx-6,sy-5,12,10,C_lib); herder_fb_fill(fb,sx-6,sy-1,12,2,HEX(0x9a6a2a)); }
    /* sheep */
    for(int i=0;i<w->sheep_count;i++){ const HerderSheep*s=&w->sheep[i]; if(s->mode==2) continue; int sx=(int)((s->x-ox)*HERDER_TS)+HERDER_TS/2, sy=HERDER_TOP+(int)((s->y-oy)*HERDER_TS)+HERDER_TS/2; if(sx<-20||sy<HERDER_TOP-20||sx>HERDER_SCRW+20||sy>HERDER_SCRH-HERDER_BOT+20) continue; draw_sheep(fb,sx,sy,s->black,s->x<w->h.x?2:0); }
    /* herder */
    { int sx=(int)((w->h.x-ox)*HERDER_TS)+HERDER_TS/2, sy=HERDER_TOP+(int)((w->h.y-oy)*HERDER_TS)+HERDER_TS/2; draw_herder(fb,sx,sy,w->h.facing,w->h.carrying>=0); }
}
