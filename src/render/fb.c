#include "render/fb.h"
#include "core/map/terrain.h"
#include "render/font.h"
#include "core/progression.h"
#include "core/names.h"
#include <string.h>
#include <stdio.h>

static uint16_t rgb565(int r, int g, int b){ return (uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3)); }
#define HEX(h) rgb565(((h)>>16)&0xff, ((h)>>8)&0xff, (h)&0xff)

static uint16_t TERRAIN_COL[16];
static uint16_t C_pen, C_penground, C_tree, C_boulder, C_house, C_houseR, C_lib, C_well, C_scare, C_stump;
static uint16_t C_sheep, C_black, C_herder;
uint16_t HERDER_C_hud, HERDER_C_panel, HERDER_C_ink, HERDER_C_bar_bg, HERDER_C_bar;
static int g_anim = 0;
void herder_fb_set_anim(int frame){ g_anim = frame; }
static double g_camx=-1, g_camy=-1;
void herder_fb_reset_camera(void){ g_camx=-1; g_camy=-1; }

/* base (untinted) world colours, 0xRRGGBB */
static uint32_t bT0[16];          /* original (summer-base) terrain */
static uint32_t bT[16];           /* seasonal terrain (bT0 + season overrides) */
static uint32_t bpen,bpeng,btree,bboul,bhouse,bhouseR,blib,bwell,bscare,bstump,bsheep,bblack,bherder;
static uint32_t g_greens0[5]={0x3f8a3a,0x4b9a40,0x357a38,0x5aa34a,0x2f7a44}; /* seasonal tree foliage (base) */
static uint16_t g_greens[5];      /* day-tinted */
static uint16_t C_tree2;          /* the always-green short trees */
static uint32_t btree2=0x2f6f3a;
static char g_season[12]="summer";

/* palette.ts seasonalTerrain + seasonalGreens */
void herder_fb_set_season(const char *season){
    snprintf(g_season,sizeof(g_season),"%s",season?season:"summer");
    for(int i=0;i<16;i++) bT[i]=bT0[i];
    if(!strcmp(g_season,"winter")){ bT[T_Grass]=0xb9c3ad; bT[T_Meadow]=0xc8cfb8; bT[T_Forest]=0x8e9b84; bT[T_Farm]=0xb8a882; bT[T_Mud]=0x8f7d69; bT[T_Rock]=0xb7b3aa; bT[T_Water]=0x6a8fb5; }
    else if(!strcmp(g_season,"autumn")){ bT[T_Grass]=0x94ac4c; bT[T_Meadow]=0xb3b055; bT[T_Forest]=0x7a8c3e; bT[T_Farm]=0xc0955a; }
    else if(!strcmp(g_season,"summer")){ bT[T_Grass]=0x86ba4c; bT[T_Meadow]=0xa9c25a; }
    /* spring: keep base */
    static const uint32_t GA[5]={0xd9822b,0xc9502f,0xe0b33c,0xb8652c,0xa8452a};
    static const uint32_t GW[5]={0x6b7a66,0x5f6f5c,0x7a8a75,0x66765f,0x586a55};
    static const uint32_t GS[5]={0x5fb054,0x72c25f,0x4fa34a,0x86c96a,0x4b9a40};
    static const uint32_t GD[5]={0x3f8a3a,0x4b9a40,0x357a38,0x5aa34a,0x2f7a44};
    const uint32_t *g = !strcmp(g_season,"autumn")?GA: !strcmp(g_season,"winter")?GW: !strcmp(g_season,"spring")?GS: GD;
    for(int i=0;i<5;i++) g_greens0[i]=g[i];
    btree2 = !strcmp(g_season,"winter")?0x4d6a52:0x2f6f3a;
}

/* the web's day tint keyframes (palette.ts TINT_KEYS): hour -> (r,g,b,a) */
static void day_tint(double hour, int *tr, int *tg, int *tb, double *ta){
    static const double H[7]={9.0,10.5,15.0,16.5,17.6,18.3,19.5};
    static const int    R[7]={120,255,255,255,255,70,20}, G[7]={160,255,255,190,130,60,24}, B[7]={255,255,255,110,70,140,70};
    static const double A[7]={0.10,0.0,0.0,0.10,0.22,0.40,0.55};
    double h=hour; if(h<H[0])h=H[0]; if(h>H[6])h=H[6];
    for(int i=0;i<6;i++){ if(h>=H[i]&&h<=H[i+1]){ double t=(h-H[i])/(H[i+1]-H[i]);
        *tr=(int)(R[i]+(R[i+1]-R[i])*t); *tg=(int)(G[i]+(G[i+1]-G[i])*t); *tb=(int)(B[i]+(B[i+1]-B[i])*t); *ta=A[i]+(A[i+1]-A[i])*t; return; } }
    *tr=0;*tg=0;*tb=0;*ta=0;
}
static uint16_t blend565(uint32_t base, int tr, int tg, int tb, double a){
    int r=(base>>16)&0xff, g=(base>>8)&0xff, b=base&0xff;
    r=(int)(r+(tr-r)*a); g=(int)(g+(tg-g)*a); b=(int)(b+(tb-b)*a);
    return rgb565(r,g,b);
}
void herder_fb_set_tint(double hour){
    int tr,tg,tb; double ta; day_tint(hour,&tr,&tg,&tb,&ta);
    for(int i=0;i<16;i++) TERRAIN_COL[i]=blend565(bT[i],tr,tg,tb,ta);
    C_pen=blend565(bpen,tr,tg,tb,ta); C_penground=blend565(bpeng,tr,tg,tb,ta);
    C_tree=blend565(btree,tr,tg,tb,ta); C_boulder=blend565(bboul,tr,tg,tb,ta);
    C_house=blend565(bhouse,tr,tg,tb,ta); C_houseR=blend565(bhouseR,tr,tg,tb,ta);
    C_lib=blend565(blib,tr,tg,tb,ta); C_well=blend565(bwell,tr,tg,tb,ta);
    C_scare=blend565(bscare,tr,tg,tb,ta); C_stump=blend565(bstump,tr,tg,tb,ta);
    C_sheep=blend565(bsheep,tr,tg,tb,ta); C_black=blend565(bblack,tr,tg,tb,ta); C_herder=blend565(bherder,tr,tg,tb,ta);
    for(int i=0;i<5;i++) g_greens[i]=blend565(g_greens0[i],tr,tg,tb,ta);
    C_tree2=blend565(btree2,tr,tg,tb,ta); C_tree=g_greens[0];
}
void herder_fb_palette_init(void){
    bT0[T_Water]=0x4f8fc9; bT0[T_Sand]=0xe3d29a; bT0[T_Grass]=0x7cb548; bT0[T_Meadow]=0x8fbf50;
    bT0[T_Farm]=0xc9a35a; bT0[T_Forest]=0x5a9a44; bT0[T_Mud]=0x8d6f4e; bT0[T_Rock]=0x9a958c;
    bT0[T_Snow]=0xf2f4f7; bT0[T_Road]=0xd8c398; bT0[T_Bridge]=0xa8804f;
    herder_fb_set_season("summer");
    bpen=0x6b4a2b; bpeng=0x93894f; btree=0x2f6f3a; bboul=0x7a746b;
    bhouse=0x8c4a3a; bhouseR=0xb03a3a; blib=0xe0b33c; bwell=0x8a8578; bscare=0x8a6238; bstump=0x7d5a35;
    bsheep=0xf6f2e6; bblack=0x3a3532; bherder=0x7a5a3a;
    HERDER_C_ink=HEX(0x2b2620); HERDER_C_hud=HEX(0x28241e); HERDER_C_panel=HEX(0xf2ecdc);
    HERDER_C_bar_bg=HEX(0x50483f); HERDER_C_bar=HEX(0xc05040);
    herder_fb_set_tint(12.0);
}

void herder_fb_fill(uint16_t *fb, int x, int y, int w, int h, uint16_t c){
    for(int j=y;j<y+h;j++){ if((unsigned)j>=HERDER_SCRH)continue; for(int i=x;i<x+w;i++){ if((unsigned)i<HERDER_SCRW) fb[j*HERDER_SCRW+i]=c; } }
}
void herder_fb_bar(uint16_t *fb, int x, int y, int w, int h, double frac){
    herder_fb_fill(fb,x,y,w,h,HERDER_C_bar_bg); int fwd=(int)(w*frac); if(fwd<0)fwd=0; if(fwd>w)fwd=w; if(fwd) herder_fb_fill(fb,x,y,fwd,h,HERDER_C_bar);
}

#define VIEWW (HERDER_SCRW / HERDER_TS + 2)
#define VIEWH ((HERDER_SCRH - HERDER_TOP) / HERDER_TS + 2)

static void disc(uint16_t *fb, int cx, int cy, int r, uint16_t c){
    for(int dy=-r;dy<=r;dy++) for(int dx=-r;dx<=r;dx++) if(dx*dx+dy*dy<=r*r){ int x=cx+dx,y=cy+dy; if((unsigned)x<HERDER_SCRW&&(unsigned)y<HERDER_SCRH) fb[y*HERDER_SCRW+x]=c; }
}
static void rrect(uint16_t *fb,int x,int y,int w,int h,uint16_t c){ herder_fb_fill(fb,x+1,y,w-2,h,c); herder_fb_fill(fb,x,y+1,w,h-2,c); }

/* subtle per-tile texture fleck, echoing the web's grass/meadow speckle */
static void tex_fleck(uint16_t *fb,int px,int py,int mx,int my,int t){
    unsigned h=(unsigned)(mx*37+my*101);
    if(t==T_Grass||t==T_Meadow){ /* a small grass tuft, like the web */
        if(h%3==0){ int ox=(int)(h%9)%(HERDER_TS-2)+1, oy=(int)((h/9)%9)%(HERDER_TS-4)+2; uint16_t tc=HEX(0x5f8a34);
            herder_fb_fill(fb,px+ox,py+oy,1,3,tc); herder_fb_fill(fb,px+ox-1,py+oy+1,1,2,tc); herder_fb_fill(fb,px+ox+1,py+oy+1,1,2,tc); }
    } else if(t==T_Mud||t==T_Farm){ if(h%5==0){ int ox=(int)(h%7)%(HERDER_TS-2), oy=(int)((h/7)%7)%(HERDER_TS-2); uint16_t base=fb[(py+oy)*HERDER_SCRW+px+ox]; uint16_t d=(uint16_t)((base>>1)&0x7bef); herder_fb_fill(fb,px+ox,py+oy,2,2,d); } }
}

static void shadow(uint16_t *fb,int cx,int cy,int rx){ /* cheap dark ellipse */
    for(int dx=-rx;dx<=rx;dx++){ int w=(int)(rx*0.5*(1.0-(double)dx*dx/(rx*rx))); if(w<0)continue; int x=cx+dx,y=cy; for(int dy=-1;dy<=1;dy++){ int yy=y+dy; if((unsigned)x<HERDER_SCRW&&(unsigned)yy<HERDER_SCRH){ uint16_t c=fb[yy*HERDER_SCRW+x]; fb[yy*HERDER_SCRW+x]=(uint16_t)((c>>1)&0x7bef);} } }
}
static void draw_sheep(uint16_t *fb,int cx,int cy,int black,int facing,int moving,int named,int crowned,int pose){
    /* pose: 0 idle/walk, 1 graze (head down), 2 asleep */
    uint16_t wool=black?C_black:(pose==2?HEX(0xe9e4d3):C_sheep); uint16_t face=black?HEX(0xe8e2d4):HEX(0x3a2f2a);
    int bob = moving ? ((g_anim/4)%2 ? -1 : 0) : 0;
    shadow(fb,cx,cy+7,9);
    int sw = moving ? ((g_anim/4)%2 ? 1 : -1) : 0;   /* leg swing */
    if(pose==2){ herder_fb_fill(fb,cx-5,cy+6,10,3,face); }  /* asleep: folded legs (a bar) */
    else { herder_fb_fill(fb,cx-5,cy+5, 2, 4+sw, face); herder_fb_fill(fb,cx+3,cy+5, 2, 4-sw, face); }
    cy+=bob;
    disc(fb,cx,cy+1,7,wool); disc(fb,cx-4,cy,5,wool); disc(fb,cx+4,cy,5,wool); disc(fb,cx,cy-4,5,wool);
    int hx=facing==2?-8:8; int hd=(pose==1?3:0);   /* head drops when grazing */
    disc(fb,cx+hx,cy-1+hd,3,face);
    if(pose==2){ herder_fb_fill(fb,cx+hx-1,cy-1,3,1,HEX(0xf6f2e6)); } /* closed eye: a line */
    else herder_fb_fill(fb,cx+hx+(facing==2?-1:1),cy-2+hd,1,1,HEX(0xf6f2e6));
    if(crowned){ uint16_t g=HEX(0xe0b33c); int gx=cx+(facing==2?-6:6); herder_fb_fill(fb,gx-3,cy-8,7,2,g); herder_fb_fill(fb,gx-3,cy-11,2,3,g); herder_fb_fill(fb,gx,cy-11,2,3,g); herder_fb_fill(fb,gx+2,cy-11,2,3,g); }
    else if(named){ uint16_t r=HEX(0xc94f4f); int rx=cx+(facing==2?-5:5); herder_fb_fill(fb,rx-1,cy-7,3,3,r); }
}
static void draw_herder(uint16_t *fb,int cx,int cy,int facing,int carrying,int moving,int level,int winter,int mode){
    uint16_t coat=C_herder, skin=HEX(0xe8b98a), hat=HERDER_C_ink, boot=HEX(0x3b3a4a), belt=HEX(0x3a2f2a);
    int sitting = (mode==HM_READING||mode==HM_RESTING);
    int ranting = (mode==HM_RANTING||mode==HM_MISHAP);
    int gazing  = (mode==HM_GAZING);
    int sw = moving ? ((g_anim/4)%2 ? 2 : -2) : 0;  /* stride */
    int bob = moving ? ((g_anim/4)%2 ? -1 : 0) : 0;
    shadow(fb,cx,cy+13,10);
    if(sitting){
        cy+=6;                                       /* drop down onto the grass */
        herder_fb_fill(fb,cx-6,cy+9,5,3,boot); herder_fb_fill(fb,cx+2,cy+9,5,3,boot); /* folded legs forward */
    } else {
        herder_fb_fill(fb,cx-4,cy+8, 3, 5, boot); herder_fb_fill(fb,cx+1,cy+8, 3, 5, boot);
        if(moving){ herder_fb_fill(fb,cx-4+sw,cy+11,3,2,boot); herder_fb_fill(fb,cx+1-sw,cy+11,3,2,boot); }
    }
    cy+=bob;
    if(level>=4 && !carrying){ herder_fb_fill(fb,cx+(facing==2?4:-6),cy+1,3,5,HEX(0x7b2d3a)); } /* book under arm */
    herder_fb_fill(fb,cx-4,cy-2,8,12,coat);         /* tunic */
    herder_fb_fill(fb,cx-4,cy+5,8,2,belt);          /* belt */
    if(level>=8||winter){ herder_fb_fill(fb,cx-4,cy-2,8,2,HEX(0xb03a3a)); herder_fb_fill(fb,cx+2,cy-1,2,6,HEX(0xb03a3a)); } /* scarf */
    disc(fb,cx,cy-8,4,skin);                        /* head */
    if(facing==1){ herder_fb_fill(fb,cx-2,cy-8,1,1,HERDER_C_ink); herder_fb_fill(fb,cx+1,cy-8,1,1,HERDER_C_ink); }
    else if(facing==0){ herder_fb_fill(fb,cx+1,cy-8,1,1,HERDER_C_ink); }
    else if(facing==2){ herder_fb_fill(fb,cx-2,cy-8,1,1,HERDER_C_ink); }
    if(ranting){ /* the hat leaves his head */
        herder_fb_fill(fb,cx-5,cy-16,10,2,hat); herder_fb_fill(fb,cx-3,cy-19,6,3,hat);
        herder_fb_fill(fb,cx-7,cy-6,3,6,skin); herder_fb_fill(fb,cx+4,cy-6,3,6,skin); /* arms up */
    } else {
        herder_fb_fill(fb,cx-5,cy-11,10,2,hat);     /* hat brim */
        herder_fb_fill(fb,cx-3,cy-14,6,3,hat);      /* hat crown */
        if(facing==3) herder_fb_fill(fb,cx-4,cy-12,8,1,HEX(0x4a4438));
    }
    if(mode==HM_READING){ /* an open book in his hands */
        herder_fb_fill(fb,cx-6,cy-4,12,7,HEX(0xc94f4f)); herder_fb_fill(fb,cx-5,cy-3,5,5,HEX(0xfffdf5)); herder_fb_fill(fb,cx+1,cy-3,5,5,HEX(0xfffdf5)); herder_fb_fill(fb,cx-1,cy-4,2,7,HEX(0x8a3030));
    } else if(carrying){ herder_fb_fill(fb,cx-6,cy-9,3,6,skin); herder_fb_fill(fb,cx+3,cy-9,3,6,skin); disc(fb,cx,cy-13,5,C_sheep); herder_fb_fill(fb,cx-2,cy-13,1,1,HERDER_C_ink); }
    else if(!ranting && !sitting){ int px=facing==0?5:facing==2?-6:-1; herder_fb_fill(fb,cx+px,cy-10,2,18,HEX(0x8a6a3a)); herder_fb_fill(fb,cx+px-1,cy-11,4,2,HEX(0x8a6a3a)); } /* crook */
    (void)gazing;
}
static void draw_dog(uint16_t *fb,int cx,int cy,int facing,int moving){
    /* a black-and-white border collie, like the web */
    uint16_t black=HEX(0x28241f), white=HEX(0xf0ead8);
    int sw = moving ? ((g_anim/3)%2 ? 1 : -1) : 0;
    herder_fb_fill(fb,cx-4,cy+2,2,3+sw,black); herder_fb_fill(fb,cx+2,cy+2,2,3-sw,black); /* legs */
    herder_fb_fill(fb,cx-4,cy+4,2,1,white); herder_fb_fill(fb,cx+2,cy+4,2,1,white);       /* white socks */
    disc(fb,cx,cy,4,black);                          /* body */
    herder_fb_fill(fb,cx-1,cy,2,4,white);            /* white chest/belly stripe */
    int hx=facing==2?-5:5; disc(fb,cx+hx,cy-2,3,black); /* head */
    herder_fb_fill(fb,cx+hx+(facing==2?-2:1),cy-1,2,2,white); /* white muzzle */
    herder_fb_fill(fb,cx+hx+(facing==2?-1:0),cy-4,1,2,black); /* ear */
    int tx=cx-(facing==2?-6:6); herder_fb_fill(fb,tx,cy+1,3,1,black); herder_fb_fill(fb,tx+(facing==2?2:0),cy+1,1,1,white); /* tail w/ white tip */
}
static void draw_tree(uint16_t *fb,int cx,int cy,uint16_t foliage){ herder_fb_fill(fb,cx-1,cy,3,7,HEX(0x6b4a2b)); disc(fb,cx,cy-3,7,foliage); }
static void draw_house(uint16_t *fb,int px,int py,int red){
    uint16_t wall=red?HEX(0xd8b28a):HEX(0xe8dcc3), roof=red?HEX(0xb03a3a):HEX(0x8c4a3a);
    herder_fb_fill(fb,px+3,py+HERDER_TS/2,HERDER_TS-6,HERDER_TS/2-1,wall);
    for(int r=0;r<HERDER_TS/2;r++) herder_fb_fill(fb,px+2+r/2,py+r,HERDER_TS-4-r,1,roof);
}

static void draw_rival(uint16_t *fb, const HerderWorld *w);
void herder_fb_draw_world(uint16_t *fb, const HerderWorld *w){
    const HerderMap *m=w->map;
    /* lead the camera toward where he is going (web: +0.25 * toward path[min(4,rem-1)]) */
    double _tx=w->h.x, _ty=w->h.y; int _rem=w->h.path_len - w->h.path_head;
    if(_rem>0){ int _k=(_rem-1<4)?(_rem-1):4; int _wx=w->h.path[(w->h.path_head+_k)*2], _wy=w->h.path[(w->h.path_head+_k)*2+1]; _tx+=(_wx-w->h.x)*0.25; _ty+=(_wy-w->h.y)*0.25; }
    if(g_camx<0){ g_camx=_tx; g_camy=_ty; } else { g_camx+=(_tx-g_camx)*0.12; g_camy+=(_ty-g_camy)*0.12; }
    int cx=(int)(g_camx+0.5), cy=(int)(g_camy+0.5);
    int ox=cx-VIEWW/2, oy=cy-VIEWH/2;
    int vh=(HERDER_SCRH-HERDER_TOP);
    for(int ty=0;ty<VIEWH;ty++){ int my=oy+ty; int py=HERDER_TOP+ty*HERDER_TS; if(py>=HERDER_TOP+vh) break;
        for(int tx=0;tx<VIEWW;tx++){ int mx=ox+tx; int px=tx*HERDER_TS;
            uint16_t c; int t=T_Water,d=D_None;
            if(mx<0||my<0||mx>=m->size||my>=m->size) c=HERDER_C_ink;
            else { int i=my*m->size+mx; t=m->terrain[i]; d=m->deco[i]; c=TERRAIN_COL[t];
                   if(d==D_PenGround) c=C_penground; else if(d==D_Fence) c=C_pen; }
            herder_fb_fill(fb,px,py,HERDER_TS,HERDER_TS,c);
            if(mx>=0&&my>=0&&mx<m->size&&my<m->size){ tex_fleck(fb,px,py,mx,my,t);
                switch(d){
                    case D_Tree: draw_tree(fb,px+HERDER_TS/2,py+HERDER_TS/2, g_greens[(unsigned)(mx*7+my*13)%5]); break;
                    case D_Tree2: draw_tree(fb,px+HERDER_TS/2,py+HERDER_TS/2, C_tree2); break;
                    case D_House: case D_HouseRed: { draw_house(fb,px,py,d==D_HouseRed);
                        /* a couple of hens scratching by the door */
                        unsigned hh=(unsigned)(mx*17+my*31); for(int k=0;k<2;k++){ int hx=px+2+((hh>>(k*3))&7)%(HERDER_TS-3), hy=py+HERDER_TS-4-(k*2); disc(fb,hx,hy,2,HEX(0xf0ead8)); herder_fb_fill(fb,hx,hy-2,1,1,HEX(0xc94f4f)); herder_fb_fill(fb,hx+(k?2:-2),hy,1,1,HEX(0xe0b33c)); } } break;
                    case D_Boulder: disc(fb,px+HERDER_TS/2,py+HERDER_TS/2,HERDER_TS/3,C_boulder); break;
                    case D_Well: disc(fb,px+HERDER_TS/2,py+HERDER_TS/2,HERDER_TS/3,C_well); break;
                    case D_Scarecrow: herder_fb_fill(fb,px+HERDER_TS/2-1,py+2,2,HERDER_TS-4,C_scare); herder_fb_fill(fb,px+3,py+HERDER_TS/3,HERDER_TS-6,2,C_scare); break;
                    case D_Stump: disc(fb,px+HERDER_TS/2,py+HERDER_TS/2,4,C_stump); break;
                    case D_Tuft: { uint16_t tc=HEX(0x3a6a24); int bx=px+HERDER_TS/2, by=py+HERDER_TS/2+3; for(int b=-3;b<=3;b+=3){ herder_fb_fill(fb,bx+b,by-4,1,5,tc); herder_fb_fill(fb,bx+b-1,by-4,1,2,tc); } } break;
                    case D_Flowers: { static const uint32_t fl[4]={0xf2c14e,0xe86a92,0xf4f1e6,0xb58cf0}; unsigned h=(unsigned)(mx*29+my*71); for(int k=0;k<4;k++){ int fx=px+2+((h>>(k*2))&7)%(HERDER_TS-3), fy=py+2+((h>>(k*3))&7)%(HERDER_TS-3); uint16_t c=HEX(fl[(k+ (h&3))%4]); herder_fb_fill(fb,fx,fy,2,2,c); } } break;
                    default: break;
                }
            }
        }
    }
    /* library book-boxes */
    for(int i=0;i<w->lib_count;i++){ if(w->lib[i].taken) continue; int sx=(w->lib[i].x-ox)*HERDER_TS+HERDER_TS/2, sy=HERDER_TOP+(w->lib[i].y-oy)*HERDER_TS+HERDER_TS/2; rrect(fb,sx-6,sy-5,12,10,C_lib); herder_fb_fill(fb,sx-6,sy-1,12,2,HEX(0x9a6a2a)); }
    /* sheep */
    for(int i=0;i<w->sheep_count;i++){ const HerderSheep*s=&w->sheep[i]; if(s->mode==1) continue; /* carried: drawn with herder */
        int sx=(int)((s->x-ox)*HERDER_TS)+HERDER_TS/2, sy=HERDER_TOP+(int)((s->y-oy)*HERDER_TS)+HERDER_TS/2;
        if(sx<-20||sy<HERDER_TOP-20||sx>HERDER_SCRW+20||sy>HERDER_SCRH+20) continue;
        int smv=(s->mode==0)&&((s->tx!=s->x)||(s->ty!=s->y));
        int pose=0; if(!smv && s->mode==0){ if(s->temper==3 && ((s->id*7+w->tick/512)%5)==0) pose=2; else if(((s->id*13+w->tick/256)%3)==0) pose=1; }
        draw_sheep(fb,sx,sy,s->black,s->x<w->h.x?2:0,smv,s->named,s->nemesis,pose); }
    /* herder */
    { int sx=(int)((w->h.x-ox)*HERDER_TS)+HERDER_TS/2, sy=HERDER_TOP+(int)((w->h.y-oy)*HERDER_TS)+HERDER_TS/2;
      int hmv=(w->h.mode==HM_TOSHEEP||w->h.mode==HM_TOPEN||w->h.mode==HM_TOLIBRARY);
      int lvl=herder_level_for(herder_erudition(w->booksRead,w->sheepPenned,w->tick/14400.0));
      int winter=(w->season && strcmp(w->season,"winter")==0);
      /* the sheepdog trots a step behind, on the side away from his facing */
      int ddx=w->h.facing==0?1:w->h.facing==2?-1:0, ddy=w->h.facing==1?1:w->h.facing==3?-1:0;
      draw_dog(fb,sx-ddx*HERDER_TS,sy-ddy*HERDER_TS+HERDER_TS/2, w->h.facing, hmv);
      draw_herder(fb,sx,sy,w->h.facing,w->h.carrying>=0,hmv,lvl,winter,w->h.mode); }
    draw_rival(fb,w);
}


/* small overview map, top-right, echoing the web's minimap */
#define MM_SZ 110
#define MM_X (HERDER_SCRW - MM_SZ - 22)
#define MM_Y (HERDER_TOP + 14)
void herder_fb_minimap(uint16_t *fb, const HerderWorld *w){
    const HerderMap *m=w->map; int n=m->size;
    herder_fb_fill(fb, MM_X-2, MM_Y-2, MM_SZ+4, MM_SZ+4, HERDER_C_ink);
    for(int py=0;py<MM_SZ;py++){ int my=py*n/MM_SZ;
        for(int px=0;px<MM_SZ;px++){ int mx=px*n/MM_SZ; int i=my*n+mx;
            uint16_t c=TERRAIN_COL[m->terrain[i]];
            int d=m->deco[i]; if(d==D_PenGround||d==D_Fence) c=C_pen; else if(d==D_Tree||d==D_Tree2) c=C_tree;
            fb[(MM_Y+py)*HERDER_SCRW+(MM_X+px)]=c;
        }
    }
    /* pen marker */
    { int px=m->pen_x*MM_SZ/n, py=m->pen_y*MM_SZ/n; herder_fb_fill(fb,MM_X+px-1,MM_Y+py-1,3,3,C_pen); }
    /* loose sheep */
    for(int i=0;i<w->sheep_count;i++){ const HerderSheep*s=&w->sheep[i]; if(s->mode==2) continue; int px=(int)(s->x*MM_SZ/n), py=(int)(s->y*MM_SZ/n); if((unsigned)px<MM_SZ&&(unsigned)py<MM_SZ) fb[(MM_Y+py)*HERDER_SCRW+(MM_X+px)]= s->black?C_black:C_sheep; }
    /* herder */
    { int px=(int)(w->h.x*MM_SZ/n), py=(int)(w->h.y*MM_SZ/n); herder_fb_fill(fb,MM_X+px-1,MM_Y+py-1,3,3,HEX(0x2a4086)); }
}


/* rain and fog overlays, keyed to the world's weather ticks */
void herder_fb_weather(uint16_t *fb, const HerderWorld *w){
    int vy0=HERDER_TOP, vy1=HERDER_SCRH;
    if(w->fogUntilTick > w->tick){
        /* fog: lighten every few pixels toward white (cheap dithered veil) */
        for(int y=vy0;y<vy1;y+=2) for(int x=(y&2)?0:2;x<HERDER_SCRW;x+=4){ uint16_t c=fb[y*HERDER_SCRW+x]; int r=((c>>11)&0x1f),g=((c>>5)&0x3f),b=(c&0x1f); r+=(31-r)/2; g+=(63-g)/2; b+=(31-b)/2; fb[y*HERDER_SCRW+x]=(uint16_t)((r<<11)|(g<<5)|b); }
    }
    if(w->rainUntilTick > w->tick){
        uint16_t drop=rgb565(0xb8,0xc8,0xe0);
        for(int k=0;k<260;k++){
            int x=(k*61 + g_anim*4) % HERDER_SCRW;
            int y=vy0 + (int)((unsigned)(k*137 + g_anim*9) % (unsigned)(vy1-vy0-6));
            for(int t=0;t<5;t++){ int xx=x+t, yy=y+t*2; if((unsigned)xx<HERDER_SCRW && yy<vy1) fb[yy*HERDER_SCRW+xx]=drop; }
        }
    }
}


/* the title scene: a pasture, a big sheep, the herder and his dog. Text is
 * overlaid by the caller (bfont on the Dreamcast). */
void herder_fb_title(uint16_t *fb){
    herder_fb_set_tint(11.0);
    /* sky band, then grass */
    herder_fb_fill(fb,0,0,HERDER_SCRW,160, rgb565(0x9f,0xc4,0xe8));
    herder_fb_fill(fb,0,160,HERDER_SCRW,HERDER_SCRH-160, TERRAIN_COL[T_Grass]);
    /* a low hill */
    for(int x=0;x<HERDER_SCRW;x++){ int h=(int)(20*__builtin_sin(x*0.012)+24); herder_fb_fill(fb,x,160-h,1,h, TERRAIN_COL[T_Meadow]); }
    /* trees dotted along */
    for(int i=0;i<7;i++){ int tx=40+i*90, ty=150+((i*53)%40); herder_fb_fill(fb,tx-1,ty,3,8,rgb565(0x6b,0x4a,0x2b)); disc(fb,tx,ty-4,9,C_tree); }
    /* a big hero sheep, centre */
    int cx=HERDER_SCRW/2, cy=320;
    herder_fb_fill(fb,cx-22,cy+16,7,16,rgb565(0x3a,0x2f,0x2a)); herder_fb_fill(fb,cx+15,cy+16,7,16,rgb565(0x3a,0x2f,0x2a));
    disc(fb,cx,cy,34,C_sheep); disc(fb,cx-24,cy-4,22,C_sheep); disc(fb,cx+24,cy-4,22,C_sheep); disc(fb,cx,cy-20,24,C_sheep);
    disc(fb,cx+34,cy-6,15,rgb565(0x3a,0x2f,0x2a)); /* face */
    herder_fb_fill(fb,cx+40,cy-12,3,3,HERDER_C_ink); /* eye */
    /* the herder to the left, dog beside */
    draw_herder(fb,cx-90,cy+6,0,0,0,6,0,HM_IDLE);
    draw_dog(fb,cx-70,cy+22,0,0);
}


/* bitmap-font text. scale>=1 magnifies. Returns the x after the string. */
int herder_fb_text(uint16_t *fb, int x, int y, const char *str, uint16_t color, int scale){
    if(scale<1) scale=1;
    for(const unsigned char *p=(const unsigned char*)str; *p; p++){
        int c=*p; if(c<32||c>126){ x+=HERDER_FONT_W*scale; continue; }
        const uint8_t *g=HERDER_FONT[c-32];
        for(int gy=0;gy<HERDER_FONT_H;gy++){ uint8_t row=g[gy];
            for(int gx=0;gx<HERDER_FONT_W;gx++) if(row&(1<<gx)){
                int px=x+gx*scale, py=y+gy*scale;
                herder_fb_fill(fb,px,py,scale,scale,color);
            }
        }
        x+=HERDER_FONT_W*scale;
    }
    return x;
}
int herder_fb_text_w(const char *str, int scale){ int n=0; for(const char*p=str;*p;p++)n++; return n*HERDER_FONT_W*(scale<1?1:scale); }


/* word-wrapped text; draws up to maxlines lines within pixel width maxw. */
void herder_fb_text_wrap(uint16_t *fb, int x, int y, const char *str, uint16_t color, int scale, int maxw, int maxlines){
    int cw=HERDER_FONT_W*scale, lineh=(HERDER_FONT_H+2)*scale;
    int per = maxw/cw; if(per<1) per=1;
    char buf[512]; int n=0; for(const char*p=str; *p && n<511; p++) buf[n++]=*p; buf[n]=0;
    int start=0, line=0, len=n;
    while(start<len && line<maxlines){
        int end=start+per; if(end>=len){ end=len; }
        else { int cut=end; while(cut>start && buf[cut]!=' ') cut--; if(cut>start) end=cut; }
        char tmp[128]; int L=end-start; if(L>127)L=127; for(int i=0;i<L;i++) tmp[i]=buf[start+i]; tmp[L]=0;
        herder_fb_text(fb, x, y+line*lineh, tmp, color, scale);
        start = (end<len && buf[end]==' ')? end+1 : end;
        line++;
    }
}


/* a floating speech bubble above the herder, with a tail. Text wraps inside. */
void herder_fb_bubble(uint16_t *fb, const HerderWorld *w, const char *text){
    if(!text||!text[0]) return;
    /* herder screen position (same camera as draw_world) */
    int cx=(int)(g_camx<0?w->h.x:g_camx)+0, cy=(int)(g_camy<0?w->h.y:g_camy)+0;
    int ox=cx-VIEWW/2, oy=cy-VIEWH/2;
    int ax=(int)((w->h.x-ox)*HERDER_TS)+HERDER_TS/2;
    int ay=HERDER_TOP+(int)((w->h.y-oy)*HERDER_TS)+HERDER_TS/2 - 18;
    /* wrap to lines */
    int per=34; char lines[3][40]; int nl=0;
    int len=(int)0; for(const char*p=text;*p;p++) len++;
    int start=0;
    while(start<len && nl<3){ int end=start+per; if(end>=len) end=len; else { int cut=end; while(cut>start&&text[cut]!=' ')cut--; if(cut>start)end=cut; }
        int L=end-start; if(L>39)L=39; for(int i=0;i<L;i++) lines[nl][i]=text[start+i]; lines[nl][L]=0; nl++;
        start=(end<len&&text[end]==' ')?end+1:end; }
    int maxw=0; for(int i=0;i<nl;i++){ int wdt=herder_fb_text_w(lines[i],1); if(wdt>maxw)maxw=wdt; }
    int bw=maxw+14, bh=nl*13+10;
    int bx=ax-bw/2, by=ay-bh;
    if(bx<22)bx=22;
    if(bx>HERDER_SCRW-bw-22)bx=HERDER_SCRW-bw-22;
    if(by<HERDER_TOP+4)by=HERDER_TOP+4;
    /* bubble */
    herder_fb_fill(fb,bx-1,by-1,bw+2,bh+2,HERDER_C_ink);
    rrect(fb,bx,by,bw,bh,HERDER_C_panel);
    /* tail toward the herder */
    for(int t=0;t<8;t++){ int ty=by+bh+t; int half=8-t; if(half<1)half=1; herder_fb_fill(fb,ax-half,ty,half*2,1,(t==7)?HERDER_C_ink:HERDER_C_panel); }
    herder_fb_fill(fb,ax-9,by+bh,18,1,HERDER_C_panel);
    /* text */
    for(int i=0;i<nl;i++) herder_fb_text(fb,bx+7,by+6+i*13,lines[i],HERDER_C_ink,1);
}


/* a rainbow arc in the upper viewport, for a while after the rain stops */
void herder_fb_rainbow(uint16_t *fb, int alpha8){
    if(alpha8<=0) return;
    static const uint32_t band[7]={0xe0483a,0xe08a3a,0xe0c83a,0x4faa50,0x4f8fc9,0x5a5ac9,0x8a4fb0};
    int cx=HERDER_SCRW/2, cy=HERDER_TOP+ (HERDER_SCRH-HERDER_TOP); /* centre near horizon-ish */
    int r0=210;
    for(int b=0;b<7;b++){ int r=r0+b*6; uint16_t c=rgb565((band[b]>>16)&0xff,(band[b]>>8)&0xff,band[b]&0xff);
        for(int a=0;a<180;a++){ double ang=a*3.14159265/180.0; int x=cx+(int)(r*__builtin_cos(ang)); int y=cy-(int)(r*__builtin_sin(ang));
            if(x<0||x>=HERDER_SCRW||y<HERDER_TOP||y>=HERDER_SCRH) continue;
            /* dithered alpha */
            if(((x+y+b)&3) < (alpha8>>6)) fb[y*HERDER_SCRW+x]=c;
        }
    }
}


/* the induction scene: a dusk pasture and a gravestone. Text overlaid by caller. */
void herder_fb_gravestone(uint16_t *fb){
    herder_fb_set_tint(18.3); /* deep dusk palette */
    herder_fb_fill(fb,0,0,HERDER_SCRW,150, rgb565(0x46,0x40,0x74));      /* dusk sky */
    herder_fb_fill(fb,0,150,HERDER_SCRW,HERDER_SCRH-150, TERRAIN_COL[T_Grass]);
    for(int x=0;x<HERDER_SCRW;x++){ int h=(int)(16*__builtin_sin(x*0.01)+20); herder_fb_fill(fb,x,150-h,1,h,TERRAIN_COL[T_Meadow]); }
    /* a few distant trees */
    for(int i=0;i<6;i++){ int tx=60+i*110, ty=140+((i*47)%30); herder_fb_fill(fb,tx-1,ty,3,7,rgb565(0x4a,0x33,0x1e)); disc(fb,tx,ty-3,8,C_tree); }
    /* gravestone */
    int cx=HERDER_SCRW/2, top=180;
    uint16_t stone=rgb565(0x9a,0x98,0x92), shade=rgb565(0x7a,0x78,0x72), base=rgb565(0x6a,0x66,0x60);
    herder_fb_fill(fb,cx-96,top+18,192,150,shade);
    herder_fb_fill(fb,cx-92,top+14,184,150,stone);
    disc(fb,cx,top+22,92,stone);
    herder_fb_fill(fb,cx-110,top+168,220,16,base);
    /* a little mound and the crook resting against it */
    disc(fb,cx,top+184,120,rgb565(0x5a,0x6a,0x36));
    herder_fb_fill(fb,cx+96,top+70,3,110,rgb565(0x8a,0x6a,0x3a));
}


/* dev helper: draw one herder / sheep for the sprite sheet. */
void herder_fb_test_herder(uint16_t *fb,int x,int y,int facing,int carrying,int moving,int level,int mode){ draw_herder(fb,x,y,facing,carrying,moving,level,0,mode); }
void herder_fb_test_sheep(uint16_t *fb,int x,int y,int black,int facing,int moving,int named,int crowned,int pose){ draw_sheep(fb,x,y,black,facing,moving,named,crowned,pose); }
void herder_fb_test_dog(uint16_t *fb,int x,int y,int facing,int moving){ draw_dog(fb,x,y,facing,moving); }

static void draw_rival(uint16_t *fb, const HerderWorld *w){
    if(!w->has_rival) return;
    int cx=(int)(g_camx<0?w->h.x:g_camx), cy=(int)(g_camy<0?w->h.y:g_camy);
    int ox=cx-VIEWW/2, oy=cy-VIEWH/2;
    int rx=(int)((w->rival_x-ox)*HERDER_TS)+HERDER_TS/2;
    int ry=HERDER_TOP+(int)((w->rival_y-oy)*HERDER_TS)+HERDER_TS/2;
    if(rx<-60||rx>HERDER_SCRW+60) return;
    int dir = w->rival_dx<0 ? 2 : 0;                 /* facing */
    int back = w->rival_dx<0 ? 1 : -1;               /* his flock trails behind */
    /* his tidy flock: a neat line, all facing the same way, none fleeing */
    for(int i=1;i<=4;i++){ draw_sheep(fb, rx+back*i*(HERDER_TS+2), ry+2, 0, dir, 1, 0, 0, 0); }
    /* the rival: a herder in a different coat */
    /* reuse draw_herder via test wrapper colour? draw a compact figure */
    herder_fb_fill(fb,rx-4,ry-2,8,12,HEX(0x4a6a8a));   /* blue coat, unlike ours */
    herder_fb_fill(fb,rx-4,ry+5,8,2,HEX(0x2a2f3a));
    disc(fb,rx,ry-8,4,HEX(0xe8b98a));
    herder_fb_fill(fb,rx-5,ry-11,10,2,HERDER_C_ink);
    herder_fb_fill(fb,rx-3,ry-14,6,3,HERDER_C_ink);
    int px=dir==2?-6:5; herder_fb_fill(fb,rx+px,ry-10,2,18,HEX(0x8a6a3a));
}


void herder_fb_hud(uint16_t *fb, const HerderWorld *w, int fast){
    const int px=16, py=16, pw=250, ph=98;
    herder_fb_fill(fb, px-2, py-2, pw+4, ph+4, HERDER_C_ink);
    herder_fb_fill(fb, px, py, pw, ph, HERDER_C_panel);
    double hours=w->tick/14400.0;
    int hh=9+(int)hours, mm=(int)((hours-(int)hours)*60);
    int level=herder_level_for(herder_erudition(w->booksRead,w->sheepPenned,hours));
    double fr=w->frustration;
    const char *mood = fr<20?"Muttering": fr<40?"Grumbling": fr<60?"Cursing": fr<80?"Swearing":"Unhinged";
    char nm[64]; snprintf(nm,sizeof(nm),"%s%s %s", herder_name_old(w->seed)?"Old ":"", HERDER_FIRST[herder_name_first(w->seed)], HERDER_EPITHET[herder_name_epithet(w->seed)]);
    char row[80];
    herder_fb_text(fb, px+8, py+6, nm, HERDER_C_ink, 1);
    if(fast>1){ char sp[16]; snprintf(sp,sizeof(sp),"x%d",fast); herder_fb_text(fb, px+pw-herder_fb_text_w(sp,1)-8, py+6, sp, HERDER_C_ink, 1); }
    snprintf(row,sizeof(row),"Day    %02d:%02d", hh,mm);                        herder_fb_text(fb, px+8, py+22, row, HERDER_C_ink, 1);
    snprintf(row,sizeof(row),"Flock  %d / %d", w->sheepPenned, w->sheep_count);  herder_fb_text(fb, px+8, py+35, row, HERDER_C_ink, 1);
    snprintf(row,sizeof(row),"Level  %d \xC2\xB7 %s", level, HERDER_LEVEL_NAMES[level]); herder_fb_text(fb, px+8, py+48, row, HERDER_C_ink, 1);
    snprintf(row,sizeof(row),"Mood   %s %d", mood, (int)(fr+0.5));              herder_fb_text(fb, px+8, py+61, row, HERDER_C_ink, 1);
    herder_fb_bar(fb, px+8, py+78, pw-16, 10, fr/100.0);
}


/* the Curse's dry meta-commentary banner, top-centre (below the HUD) */
void herder_fb_curse_banner(uint16_t *fb, const char *line){
    if(!line||!line[0]) return;
    const char *lbl="THE CURSE:";
    int lw=herder_fb_text_w(lbl,1), tw=herder_fb_text_w(line,1);
    int bw=lw+8+tw+16; if(bw>HERDER_SCRW-40) bw=HERDER_SCRW-40;
    int bx=(HERDER_SCRW-bw)/2, by=120;
    herder_fb_fill(fb,bx-2,by-2,bw+4,26,HERDER_C_ink);
    herder_fb_fill(fb,bx,by,bw,22,HERDER_C_hud);
    herder_fb_text(fb,bx+8,by+7,lbl,rgb565(0xd8,0xb0,0x50),1);   /* dim gold label */
    herder_fb_text(fb,bx+8+lw+8,by+7,line,0xffff,1);
}


/* a light toast at the top-centre (e.g. a book found), like the web */
void herder_fb_toast(uint16_t *fb, const char *text){
    if(!text||!text[0]) return;
    int tw=herder_fb_text_w(text,1); int bw=tw+20; if(bw>HERDER_SCRW-40)bw=HERDER_SCRW-40;
    int bx=(HERDER_SCRW-bw)/2, by=96;
    herder_fb_fill(fb,bx-2,by-2,bw+4,24,HERDER_C_ink);
    herder_fb_fill(fb,bx,by,bw,20,HERDER_C_panel);
    herder_fb_text(fb,bx+10,by+6,text,HERDER_C_ink,1);
}


/* the persistent "Last heard" strip at the foot of the screen, as on the web */
void herder_fb_lastheard(uint16_t *fb, const char *text){
    if(!text||!text[0]) return;
    int py=HERDER_SCRH-40;
    herder_fb_fill(fb,0,py,HERDER_SCRW,40,HERDER_C_panel);
    herder_fb_fill(fb,0,py,HERDER_SCRW,2,HERDER_C_ink);
    herder_fb_text(fb,16,py+6,"Last heard",rgb565(0x90,0x88,0x74),1);
    /* clip the line to the width */
    char buf[100]; int per=(HERDER_SCRW-40)/HERDER_FONT_W; if(per>99)per=99;
    int i=0; for(;text[i]&&i<per;i++) buf[i]=text[i]; buf[i]=0;
    if(text[i]){ if(i>2){ buf[i-1]='.'; buf[i-2]='.'; } }
    herder_fb_text(fb,16,py+20,buf,HERDER_C_ink,1);
}


/* the day-start forecast, a wrapped cream card centred under the HUD */
void herder_fb_forecast(uint16_t *fb, const char *text){
    if(!text||!text[0]) return;
    int bw=HERDER_SCRW-160, bx=(HERDER_SCRW-bw)/2, by=132, bh=58;
    herder_fb_fill(fb,bx-2,by-2,bw+4,bh+4,HERDER_C_ink);
    herder_fb_fill(fb,bx,by,bw,bh,HERDER_C_panel);
    herder_fb_text_wrap(fb,bx+8,by+8,text,HERDER_C_ink,1,bw-16,4);
}


/* a cow standing in a field, drawn at a world tile (camera-transformed) */
void herder_fb_cow(uint16_t *fb, const HerderWorld *w, int tx, int ty){
    if(tx<0) return;
    int cx=(int)(g_camx<0?w->h.x:g_camx), cy=(int)(g_camy<0?w->h.y:g_camy);
    int ox=cx-VIEWW/2, oy=cy-VIEWH/2;
    int sx=(tx-ox)*HERDER_TS+HERDER_TS/2, sy=HERDER_TOP+(ty-oy)*HERDER_TS+HERDER_TS/2;
    if(sx<-20||sy<HERDER_TOP-20||sx>HERDER_SCRW+20||sy>HERDER_SCRH+20) return;
    uint16_t body=HEX(0x6b5a4a), white=HEX(0xf0ead8), dark=HEX(0x2a2018);
    shadow(fb,sx,sy+8,12);
    herder_fb_fill(fb,sx-7,sy+6,3,5,dark); herder_fb_fill(fb,sx+4,sy+6,3,5,dark); /* legs */
    disc(fb,sx,sy,9,body);                       /* body */
    herder_fb_fill(fb,sx-6,sy-4,6,7,white); herder_fb_fill(fb,sx+2,sy+1,5,5,white); /* patches */
    disc(fb,sx+9,sy-2,4,body);                    /* head */
    herder_fb_fill(fb,sx+12,sy-1,2,2,HEX(0xd8b0a0)); /* muzzle */
    herder_fb_fill(fb,sx+8,sy-6,1,2,dark); herder_fb_fill(fb,sx+11,sy-6,1,2,dark); /* horns/ears */
}
