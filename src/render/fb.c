#include "render/fb.h"
#include "core/map/terrain.h"

static uint16_t rgb565(int r, int g, int b){ return (uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3)); }
#define HEX(h) rgb565(((h)>>16)&0xff, ((h)>>8)&0xff, (h)&0xff)

static uint16_t TERRAIN_COL[16];
static uint16_t C_pen, C_penground, C_tree, C_boulder, C_house, C_houseR, C_lib, C_well, C_scare, C_stump;
static uint16_t C_sheep, C_black, C_herder;
uint16_t HERDER_C_hud, HERDER_C_panel, HERDER_C_ink, HERDER_C_bar_bg, HERDER_C_bar;
static int g_anim = 0;
void herder_fb_set_anim(int frame){ g_anim = frame; }

/* base (untinted) world colours, 0xRRGGBB */
static uint32_t bT[16];
static uint32_t bpen,bpeng,btree,bboul,bhouse,bhouseR,blib,bwell,bscare,bstump,bsheep,bblack,bherder;

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
}
void herder_fb_palette_init(void){
    bT[T_Water]=0x4f8fc9; bT[T_Sand]=0xe3d29a; bT[T_Grass]=0x7cb548; bT[T_Meadow]=0x8fbf50;
    bT[T_Farm]=0xc9a35a; bT[T_Forest]=0x5a9a44; bT[T_Mud]=0x8d6f4e; bT[T_Rock]=0x9a958c;
    bT[T_Snow]=0xf2f4f7; bT[T_Road]=0xd8c398; bT[T_Bridge]=0xa8804f;
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

static void draw_sheep(uint16_t *fb,int cx,int cy,int black,int facing,int moving){
    uint16_t wool=black?C_black:C_sheep; uint16_t face=black?HEX(0xe8e2d4):HEX(0x3a2f2a);
    int sw = moving ? ((g_anim/4)%2 ? 1 : -1) : 0;   /* leg swing */
    herder_fb_fill(fb,cx-5,cy+5, 2, 4+sw, face); herder_fb_fill(fb,cx+3,cy+5, 2, 4-sw, face);
    disc(fb,cx,cy+1,7,wool); disc(fb,cx-4,cy,5,wool); disc(fb,cx+4,cy,5,wool);
    int hx=facing==2?-8:8; disc(fb,cx+hx,cy-1,3,face);
    herder_fb_fill(fb,cx+hx+(facing==2?-1:1),cy-2,1,1,HERDER_C_ink); /* eye */
}
static void draw_herder(uint16_t *fb,int cx,int cy,int facing,int carrying,int moving){
    uint16_t coat=C_herder, skin=HEX(0xe8b98a), hat=HERDER_C_ink, boot=HEX(0x3a2a1a);
    int sw = moving ? ((g_anim/4)%2 ? 2 : -2) : 0;  /* stride */
    herder_fb_fill(fb,cx-4,cy+8, 3, 5, boot); herder_fb_fill(fb,cx+1,cy+8, 3, 5, boot); /* legs */
    if(moving){ herder_fb_fill(fb,cx-4+sw,cy+11,3,2,boot); herder_fb_fill(fb,cx+1-sw,cy+11,3,2,boot); }
    herder_fb_fill(fb,cx-4,cy-2,8,12,coat);         /* body */
    disc(fb,cx,cy-8,4,skin);                        /* head */
    herder_fb_fill(fb,cx-5,cy-11,10,3,hat);         /* hat brim */
    herder_fb_fill(fb,cx-3,cy-14,6,3,hat);          /* hat crown */
    int px=facing==0?5:facing==2?-6:-1;             /* crook */
    herder_fb_fill(fb,cx+px,cy-8,2,16,HEX(0x8a6a3a));
    if(carrying){ disc(fb,cx,cy-4,5,C_sheep); herder_fb_fill(fb,cx+(facing==2?-5:4),cy-5,1,1,HERDER_C_ink); }
}
static void draw_dog(uint16_t *fb,int cx,int cy,int facing,int moving){
    uint16_t body=HEX(0x7a5a3a), dark=HEX(0x2a2018), white=HEX(0xf0ead8);
    int sw = moving ? ((g_anim/3)%2 ? 1 : -1) : 0;
    herder_fb_fill(fb,cx-4,cy+2,2,3+sw,dark); herder_fb_fill(fb,cx+2,cy+2,2,3-sw,dark); /* legs */
    disc(fb,cx,cy,4,body);                          /* body */
    int hx=facing==2?-5:5; disc(fb,cx+hx,cy-2,3,body); /* head */
    herder_fb_fill(fb,cx+hx+(facing==2?-1:0),cy-4,1,2,dark); /* ear */
    herder_fb_fill(fb,cx,cy,2,2,white);             /* a patch */
    herder_fb_fill(fb,cx-(facing==2?-6:6),cy+1,3,1,body); /* tail */
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
    for(int i=0;i<w->sheep_count;i++){ const HerderSheep*s=&w->sheep[i]; if(s->mode==2) continue; int sx=(int)((s->x-ox)*HERDER_TS)+HERDER_TS/2, sy=HERDER_TOP+(int)((s->y-oy)*HERDER_TS)+HERDER_TS/2; if(sx<-20||sy<HERDER_TOP-20||sx>HERDER_SCRW+20||sy>HERDER_SCRH-HERDER_BOT+20) continue; int smv=(s->tx!=s->x)||(s->ty!=s->y); draw_sheep(fb,sx,sy,s->black,s->x<w->h.x?2:0,smv); }
    /* herder */
    { int sx=(int)((w->h.x-ox)*HERDER_TS)+HERDER_TS/2, sy=HERDER_TOP+(int)((w->h.y-oy)*HERDER_TS)+HERDER_TS/2;
      int hmv=(w->h.mode==HM_TOSHEEP||w->h.mode==HM_TOPEN||w->h.mode==HM_TOLIBRARY);
      /* the sheepdog trots a step behind, on the side away from his facing */
      int dogoff = w->h.facing==2?HERDER_TS:-HERDER_TS;
      draw_dog(fb,sx+dogoff,sy+HERDER_TS/2, w->h.facing, hmv);
      draw_herder(fb,sx,sy,w->h.facing,w->h.carrying>=0,hmv); }
}


/* small overview map, top-right, echoing the web's minimap */
#define MM_SZ 110
#define MM_X (HERDER_SCRW - MM_SZ - 12)
#define MM_Y (HERDER_TOP + 8)
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
    int vy0=HERDER_TOP, vy1=HERDER_SCRH-HERDER_BOT;
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
