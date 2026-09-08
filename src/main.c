/* Curse of the Herder, Dreamcast port.
 * The byte-exact ported simulation and language pipeline, drawn to the
 * framebuffer via the portable renderer in render/fb.c, with the herder's
 * generated curses shown through the BIOS font. Start exits; D-pad reseeds.
 */
#include <kos.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "core/map/generate.h"
#include "core/map/terrain.h"
#include "core/sim/world.h"
#include "core/lang/speech.h"
#include "core/lang/grammar.h"
#include "core/rng.h"
#include "data/lang_data.h"
#include "core/progression.h"
#include "core/names.h"
#include "render/fb.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

static uint16 *fb;
static double mku(const char *seed,const char *dom,int tick){ char b[128]; snprintf(b,sizeof(b),"%s|%s|%d",seed,dom,tick); uint32_t st=herder_fnv1a(b); return herder_unit_from_u32(herder_mulberry32_u32(&st)); }
static int g_fast=1;



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
    { time_t t=time(NULL); struct tm *lt=localtime(&t); int m=lt?lt->tm_mon:0;
      const char *sn = (m>=2&&m<=4)?"spring":(m>=5&&m<=7)?"summer":(m>=8&&m<=10)?"autumn":"winter";
      g_world.season=sn; herder_fb_set_season(sn); }
    herder_fb_reset_camera();
}



typedef struct { int used; char name[48]; char clock[8]; int sheep, books, level; char epitaph[120]; char seed[32]; } HallRec;
static HallRec g_hall[8];
static int g_hall_n=0;

/* VMU persistence of the Hall, wrapped in a proper vmu_pkg (header + icon). */
#include <dc/vmu_pkg.h>
static void herder_vmu_save(void){
    if(!maple_enum_type(0, MAPLE_FUNC_MEMCARD)) return;
    uint8 data[sizeof(int)+sizeof(g_hall)];
    memcpy(data,&g_hall_n,sizeof(int));
    memcpy(data+sizeof(int),g_hall,sizeof(g_hall));
    vmu_pkg_t pkg; memset(&pkg,0,sizeof(pkg));
    strncpy(pkg.desc_short,"Herder Hall",sizeof(pkg.desc_short)-1);
    strncpy(pkg.desc_long,"Curse of the Herder",sizeof(pkg.desc_long)-1);
    strncpy(pkg.app_id,"HERDER",sizeof(pkg.app_id)-1);
    pkg.icon_cnt=1; pkg.icon_anim_speed=0; pkg.eyecatch_type=VMUPKG_EC_NONE;
    static uint8 icon[512]; memset(icon,0x11,sizeof(icon)); /* a plain filled icon */
    pkg.icon_data=icon;
    pkg.data_len=sizeof(data); pkg.data=data;
    uint8 *out=NULL; int outsz=0;
    if(vmu_pkg_build(&pkg,&out,&outsz)<0) return;
    fs_unlink("/vmu/a1/HERDERHALL");
    file_t f=fs_open("/vmu/a1/HERDERHALL", O_WRONLY);
    if(f>=0){ fs_write(f,out,outsz); fs_close(f); }
    free(out);
}
static void herder_vmu_load(void){
    file_t f=fs_open("/vmu/a1/HERDERHALL", O_RDONLY);
    if(f<0) return;
    int sz=(int)fs_total(f);
    if(sz<=0){ fs_close(f); return; }
    uint8 *raw=malloc(sz); if(!raw){ fs_close(f); return; }
    fs_read(f,raw,sz); fs_close(f);
    vmu_pkg_t pkg;
    if(vmu_pkg_parse(raw,&pkg)==0 && pkg.data_len>=(int)sizeof(int)){
        int n; memcpy(&n,pkg.data,sizeof(int));
        if(n>=0 && n<=8 && pkg.data_len>=(int)(sizeof(int)+sizeof(g_hall))){ g_hall_n=n; memcpy(g_hall,pkg.data+sizeof(int),sizeof(g_hall)); }
    }
    free(raw);
}


static void herder_full_name(const char *seed, char *out, int cap){
    snprintf(out,cap,"%s%s %s", herder_name_old(seed)?"Old ":"", HERDER_FIRST[herder_name_first(seed)], HERDER_EPITHET[herder_name_epithet(seed)]);
}
static void record_day(const HerderWorld *w){
    HallRec r; r.used=1;
    herder_full_name(w->seed, r.name, sizeof(r.name));
    double hours = w->finishedTick/14400.0;
    int hh=9+(int)hours, mm=(int)((hours-(int)hours)*60);
    snprintf(r.clock,sizeof(r.clock),"%02d:%02d",hh,mm);
    r.sheep=w->sheepPenned; r.books=w->booksRead;
    r.level=herder_level_for(herder_erudition(w->booksRead,w->sheepPenned,hours));
    HerderUtterance ep=herder_speak_epitaph(w,4); snprintf(r.epitaph,sizeof(r.epitaph),"%s",ep.text);
    snprintf(r.seed,sizeof(r.seed),"%s",w->seed);
    /* newest first, keep 8, dedupe by seed */
    for(int i=0;i<g_hall_n;i++) if(strcmp(g_hall[i].seed,r.seed)==0){ for(int j=i;j<g_hall_n-1;j++) g_hall[j]=g_hall[j+1]; g_hall_n--; break; }
    for(int i=(g_hall_n<8?g_hall_n:7);i>0;i--) g_hall[i]=g_hall[i-1];
    g_hall[0]=r; if(g_hall_n<8) g_hall_n++;
    herder_vmu_save();
}

static void hall_screen(int *prevBtns){
    for(;;){
        maple_device_t *cont=maple_enum_type(0,MAPLE_FUNC_CONTROLLER);
        int btns=0; if(cont){ cont_state_t *st=(cont_state_t*)maple_dev_status(cont); if(st) btns=st->buttons; }
        int pressed=btns & ~*prevBtns; *prevBtns=btns;
        if(pressed & (CONT_START|CONT_A|CONT_B)) return;
        herder_fb_fill(fb,0,0,HERDER_SCRW,HERDER_SCRH,HERDER_C_hud);
        herder_fb_text(fb,(HERDER_SCRW-herder_fb_text_w("THE HALL OF HERDERS",2))/2,16,"THE HALL OF HERDERS",0xffff,2);
        if(g_hall_n==0) herder_fb_text(fb,120,120,"No days yet. Go and suffer one.",0xffff,2);
        for(int i=0;i<g_hall_n;i++){ char l[96]; HallRec *r=&g_hall[i];
            snprintf(l,sizeof(l),"%-22s %s  %2d sheep  Lv%d", r->name, r->clock, r->sheep, r->level);
            herder_fb_text(fb,24,58+i*46,l,0xffff,1);
            char e[80]; snprintf(e,sizeof(e),"  \"%.60s\"", r->epitaph);
            herder_fb_text(fb,24,58+i*46+16,e,0xce59,1);
        }
        herder_fb_text(fb,(HERDER_SCRW-herder_fb_text_w("B: back",2))/2,452,"B: back",0xffff,2);
        vid_waitvbl();
    }
}


static int induction_screen(const HallRec *r, int *prevBtns){
    for(int f=0;;f++){
        maple_device_t *cont=maple_enum_type(0,MAPLE_FUNC_CONTROLLER);
        int btns=0; if(cont){ cont_state_t *st=(cont_state_t*)maple_dev_status(cont); if(st) btns=st->buttons; }
        int pressed=btns & ~*prevBtns; *prevBtns=btns;
        if(pressed & CONT_START) return 1;          /* to the title */
        if((pressed & CONT_A) || f>15*60) return 0; /* a new herder wakes at dawn */
        herder_fb_gravestone(fb);
        herder_fb_text(fb,(HERDER_SCRW-herder_fb_text_w(r->name,2))/2,116,r->name,0xffff,2);
        herder_fb_text_wrap(fb,HERDER_SCRW/2-84,230,r->epitaph,HERDER_C_ink,1,168,4);
        char st[80]; snprintf(st,sizeof(st),"%d sheep  %d books  Lv%d  %s", r->sheep, r->books, r->level, r->clock);
        herder_fb_text(fb,(HERDER_SCRW-herder_fb_text_w(st,1))/2,392,st,0xffff,1);
        herder_fb_text(fb,(HERDER_SCRW-herder_fb_text_w("A new herder wakes at dawn  -  START for the title",1))/2,414,"A new herder wakes at dawn  -  START for the title",0xffff,1);
        vid_waitvbl();
    }
}

static void title_screen(int *prevBtns){
    int frame=0;
    for(;;){
        maple_device_t *cont=maple_enum_type(0,MAPLE_FUNC_CONTROLLER);
        int btns=0; if(cont){ cont_state_t *st=(cont_state_t*)maple_dev_status(cont); if(st) btns=st->buttons; }
        int pressed=btns & ~*prevBtns; *prevBtns=btns;
        if(pressed & CONT_DPAD_RIGHT) seed_idx=(seed_idx+1)%5;
        if(pressed & CONT_DPAD_LEFT) seed_idx=(seed_idx+4)%5;
        if(pressed & CONT_Y){ hall_screen(prevBtns); continue; }
        if(pressed & (CONT_START|CONT_A)) return;
        herder_fb_title(fb);
        herder_fb_text(fb,(HERDER_SCRW-herder_fb_text_w("CURSE OF THE HERDER",3))/2, 40, "CURSE OF THE HERDER", HERDER_C_ink, 3);
        char sl[64]; snprintf(sl,sizeof(sl),"< pasture: %s >", SEEDS[seed_idx]);
        herder_fb_text(fb,(HERDER_SCRW-herder_fb_text_w(sl,2))/2, 418, sl, 0xffff, 2);
        if((frame/30)%2==0) herder_fb_text(fb,(HERDER_SCRW-herder_fb_text_w("Press START",2))/2, 448, "Press START", HERDER_C_ink, 2);
        herder_fb_text(fb,16,452,"Y: Hall",HERDER_C_ink,2);
        vid_waitvbl(); frame++;
    }
}

int main(int argc, char **argv){
    (void)argc;(void)argv;
    vid_set_mode(DM_640x480, PM_RGB565);
    fb=vram_s;
    herder_fb_palette_init();
    herder_grammar_init();
    herder_vmu_load();

    int prevBtns=0;
  restart:
    title_screen(&prevBtns);
  newday:
    new_day();

    char curline[512]="The Curse of the Herder.";
    int lineUntil=240;
    int nextIdle=g_world.tick + herder_next_idle_curse_ticks(&g_world);
    int lastSeq=-1, frame=0, recorded=0, lastSignpost=-100000, lastHat=-100000, lastInn=-100000, lastHens=-100000;
    int wasRaining=0, rainbowUntil=-1;
    char curseLine[160]=""; int curseUntil=-1, lastCurse=-100000;
    char toastLine[96]=""; int toastUntil=-1;
    char lastHeard[512]="";
    char qtext[8][512]; int qat[8], qsecs[8]; int qn=0, qhead=0;
    int lastDiaryHour=9; char diaryLine[160]=""; int diaryUntil=-1; int prevLevel=0; char forecast[256]=""; int forecastUntil=frame+8*60; herder_forecast(g_world.seed,g_world.season,forecast,sizeof(forecast));
    static const int SPEEDS[6]={1,2,5,20,60,300}; int spi=0; g_fast=SPEEDS[spi];
    uint64 lastms=timer_ms_gettime64(); double tickAccum=0;

    for(;;){
        maple_device_t *cont=maple_enum_type(0,MAPLE_FUNC_CONTROLLER);
        int btns=0; if(cont){ cont_state_t *st=(cont_state_t*)maple_dev_status(cont); if(st) btns=st->buttons; }
        if(btns & CONT_START){ prevBtns=btns; goto restart; }
        int pressed=btns & ~prevBtns; prevBtns=btns;
        if(pressed & CONT_DPAD_RIGHT){ seed_idx=(seed_idx+1)%5; new_day(); lastSeq=-1; nextIdle=herder_next_idle_curse_ticks(&g_world); snprintf(curline,sizeof(curline),"A new day: %s.",SEEDS[seed_idx]); lineUntil=frame+180; lastms=timer_ms_gettime64(); tickAccum=0; herder_forecast(g_world.seed,g_world.season,forecast,sizeof(forecast)); forecastUntil=frame+8*60; }
        if(pressed & CONT_A){ spi=(spi+1)%6; g_fast=SPEEDS[spi]; }
        if(pressed & CONT_DPAD_LEFT){ seed_idx=(seed_idx+4)%5; new_day(); lastSeq=-1; nextIdle=herder_next_idle_curse_ticks(&g_world); snprintf(curline,sizeof(curline),"A new day: %s.",SEEDS[seed_idx]); lineUntil=frame+180; lastms=timer_ms_gettime64(); tickAccum=0; herder_forecast(g_world.seed,g_world.season,forecast,sizeof(forecast)); forecastUntil=frame+8*60; }

        { uint64 nowms=timer_ms_gettime64(); tickAccum += (double)(nowms-lastms)*g_fast/250.0; lastms=nowms; }
        int steps=(int)tickAccum; tickAccum-=steps; if(steps>240) steps=240;
        if(!g_world.finished){
            for(int k=0;k<steps && !g_world.finished;k++){
                herder_step(&g_world);
                while(g_world.event_count > lastSeq+1){
                    int seq=++lastSeq;
                    HerderEvent *ev=&g_world.events[seq];
                    HerderUtterance u=herder_speak_for_event(&g_world,ev,4);
                    if(u.ok){ snprintf(curline,sizeof(curline),"%s",u.text); lineUntil=frame+(int)(u.seconds*60); snprintf(lastHeard,sizeof(lastHeard),"%s",u.text); }
                    if(strcmp(ev->kind,"caught")==0 && ev->sheep_id>=0 && ev->sheep_id<g_world.sheep_count && g_world.sheep[ev->sheep_id].named && u.ok){
                        int lv=herder_level_for(herder_erudition(g_world.booksRead,g_world.sheepPenned,g_world.tick/14400.0));
                        if(lv>=2){ int at=frame+(int)(u.seconds*60)+24; static const char*bk[3]={"repeatEscape","flytingReply","repeatEscape"};
                            for(int bi=0;bi<3 && qn<8;bi++){ HerderEvent be=*ev; snprintf(be.kind,sizeof(be.kind),"%s",bk[bi]); HerderUtterance bu=herder_speak_for_event(&g_world,&be,4);
                                if(bu.ok){ snprintf(qtext[(qhead+qn)%8],512,"%s",bu.text); qat[(qhead+qn)%8]=at; qsecs[(qhead+qn)%8]=(int)(bu.seconds*60); qn++; at+=(int)(bu.seconds*60)+36; } } }
                    }
                    if(strcmp(ev->kind,"bookFound")==0 && ev->book[0]){ const char *ti=ev->book; for(int bi=0;bi<HERDER_BOOKS_N;bi++) if(strcmp(HERDER_BOOKS[bi].id,ev->book)==0){ ti=HERDER_BOOKS[bi].title; break; } snprintf(toastLine,sizeof(toastLine),"Found: %s",ti); toastUntil=frame+5*60; }
                    { const char *k=ev->kind; char *T=toastLine; int show=0;
                      if(strcmp(k,"repeatEscape")==0 && ev->sheep_id>=0 && ev->sheep_id<g_world.sheep_count && g_world.sheep[ev->sheep_id].flees==2){ snprintf(T,96,"That one has earned a name. It is %s now.", HERDER_SHEEP[herder_sheep_name(g_world.seed,ev->sheep_id)]); show=1; }
                      else if(strcmp(k,"dogHelps")==0){ snprintf(T,96,"%s herded a sheep. Once. It will not be repeated.", HERDER_DOGS[herder_dog_name(g_world.seed)]); show=1; }
                      else if(strcmp(k,"jailbreak")==0 && ev->sheep_id>=0){ snprintf(T,96,"%s has jumped the fence. It was in. It was in.", HERDER_SHEEP[herder_sheep_name(g_world.seed,ev->sheep_id)]); show=1; }
                      else if(strcmp(k,"nemesis")==0 && ev->sheep_id>=0){ snprintf(T,96,"Nemesis declared: %s, three flights and counting.", HERDER_SHEEP[herder_sheep_name(g_world.seed,ev->sheep_id)]); show=1; }
                      else if(strcmp(k,"nemesisCaught")==0 && ev->sheep_id>=0){ snprintf(T,96,"Nemesis caught: %s, after %s flights.", HERDER_SHEEP[herder_sheep_name(g_world.seed,ev->sheep_id)], ev->detail[0]?ev->detail:"several"); show=1; }
                      else if(strcmp(k,"lunchStolen")==0 && ev->sheep_id>=0){ snprintf(T,96,"Lunch: eaten by %s, while he watched. The cheese too.", HERDER_SHEEP[herder_sheep_name(g_world.seed,ev->sheep_id)]); show=1; }
                      else if(strcmp(k,"rival")==0){ snprintf(T,96,"%s walks past with three sheep that follow him.", HERDER_RIVALS[herder_rival_name(g_world.seed)]); show=1; }
                      else if(strcmp(k,"rivalBolt")==0){ snprintf(T,96,"A sheep has bolted from %s's tidy line. He is thrilled.", HERDER_RIVALS[herder_rival_name(g_world.seed)]); show=1; }
                      else if(strcmp(k,"book")==0){ const char *ti=ev->book; for(int bi=0;bi<HERDER_BOOKS_N;bi++) if(strcmp(HERDER_BOOKS[bi].id,ev->book)==0){ ti=HERDER_BOOKS[bi].title; break; } HerderContext kc; herder_build_context(&kc,&g_world,NULL,4); snprintf(T,96,"Read %s. Vocabulary: %d words.", ti, herder_known_words(&kc)); show=1; }
                      if(show) toastUntil=frame+5*60;
                    }
                    if(frame-lastCurse>1200){ int lv=herder_level_for(herder_erudition(g_world.booksRead,g_world.sheepPenned,g_world.tick/14400.0));
                        const char *ck = (strcmp(ev->kind,"mishap")==0||strcmp(ev->kind,"milestone")==0)&&ev->detail[0]?ev->detail:ev->kind;
                        const char *cr=herder_curse_remark(g_world.seed,g_world.tick,ck,lv);
                        if(cr){ snprintf(curseLine,sizeof(curseLine),"%s",cr); curseUntil=frame+7*60; lastCurse=frame; } }
                }
                if(g_world.tick>=nextIdle){
                    HerderUtterance u=herder_speak_idle(&g_world,4);
                    nextIdle=g_world.tick+herder_next_idle_curse_ticks(&g_world);
                    if(u.ok){ snprintf(curline,sizeof(curline),"%s",u.text); lineUntil=frame+(int)(u.seconds*60); snprintf(lastHeard,sizeof(lastHeard),"%s",u.text); }
                }
            }
        } else if(!recorded){
            record_day(&g_world); recorded=1;
            if(induction_screen(&g_hall[0], &prevBtns)) goto restart; /* START -> title */
            seed_idx=(seed_idx+1)%5; goto newday;                    /* else a new herder wakes */
        }

        { int nowRain = g_world.rainUntilTick > g_world.tick; if(wasRaining && !nowRain) rainbowUntil=frame+8*60; wasRaining=nowRain; }
        /* render-layer delighters: a signpost he can read, his hat in the wind */
        if(!g_world.finished){
            int hx=(int)(g_world.h.x+0.5), hy=(int)(g_world.h.y+0.5), N=g_world.map->size;
            if(g_world.tick-lastSignpost > 3600){
                int found=0; for(int dy=-2;dy<=2&&!found;dy++) for(int dx=-2;dx<=2;dx++){ int x=hx+dx,y=hy+dy; if(x<0||y<0||x>=N||y>=N)continue; if(g_world.map->deco[y*N+x]==D_Signpost){ found=1; break; } }
                if(found){ HerderUtterance u=herder_speak_kind(&g_world,EV_signpost,4,0.25); lastSignpost=g_world.tick; if(u.ok){ snprintf(curline,sizeof(curline),"%s",u.text); lineUntil=frame+(int)(u.seconds*60); snprintf(lastHeard,sizeof(lastHeard),"%s",u.text); } }
            }
            if(g_world.windUntilTick>g_world.tick && g_world.tick-lastHat>14400){
                HerderUtterance u=herder_speak_kind(&g_world,EV_hat,4,0.35); lastHat=g_world.tick; if(u.ok){ snprintf(curline,sizeof(curline),"%s",u.text); lineUntil=frame+(int)(u.seconds*60); snprintf(lastHeard,sizeof(lastHeard),"%s",u.text); }
            }
            if(g_world.tick-lastInn>90*60*4){ for(int v=0;v<g_world.map->village_count;v++){ int dx=g_world.map->villages[v].x-hx, dy=g_world.map->villages[v].y-hy; if(dx*dx+dy*dy<25){ lastInn=g_world.tick; HerderUtterance u=herder_speak_kind(&g_world,EV_inn,4,0.3); if(u.ok){ snprintf(curline,sizeof(curline),"%s",u.text); lineUntil=frame+(int)(u.seconds*60); snprintf(lastHeard,sizeof(lastHeard),"%s",u.text); } break; } } }
            if(g_world.tick-lastHens>45*60*4 && mku(g_world.seed,"hens-say",g_world.tick)<0.5){ int found=0; for(int dy=-3;dy<=3&&!found;dy++) for(int dx=-3;dx<=3;dx++){ int x=hx+dx,y=hy+dy; if(x<0||y<0||x>=N||y>=N)continue; int dd=g_world.map->deco[y*N+x]; if(dd==D_House||dd==D_HouseRed){ found=1; break; } } if(found){ lastHens=g_world.tick; HerderUtterance u=herder_speak_kind(&g_world,EV_hens,4,0.3); if(u.ok){ snprintf(curline,sizeof(curline),"%s",u.text); lineUntil=frame+(int)(u.seconds*60); snprintf(lastHeard,sizeof(lastHeard),"%s",u.text); } } }
        }
        /* deliver queued flyting beats when the bubble is free */
        if(qn>0 && frame>=lineUntil && frame>=qat[qhead]){ snprintf(curline,sizeof(curline),"%s",qtext[qhead]); lineUntil=frame+qsecs[qhead]; snprintf(lastHeard,sizeof(lastHeard),"%s",qtext[qhead]); qhead=(qhead+1)%8; qn--; }
        /* hourly diary */
        { int lvl=herder_level_for(herder_erudition(g_world.booksRead,g_world.sheepPenned,g_world.tick/14400.0));
          if(lvl>prevLevel && prevLevel>0){ snprintf(toastLine,sizeof(toastLine),"Level %d - %s. New words have arrived.", lvl, HERDER_LEVEL_NAMES[lvl]); toastUntil=frame+5*60; }
          prevLevel=lvl; }
        { double dh=9.0+g_world.tick/14400.0; int hr=(int)dh;
          if(hr!=lastDiaryHour && g_world.tick>=40 && hr!=9 && !g_world.finished){ lastDiaryHour=hr;
            int lv=herder_level_for(herder_erudition(g_world.booksRead,g_world.sheepPenned,g_world.tick/14400.0));
            double fr=g_world.frustration; const char*md=fr<22?"grumbling":fr<42?"cursing":fr<66?"swearing":"unhinged";
            snprintf(diaryLine,sizeof(diaryLine),"%02d:00 - %d/%d in - %d books - level %d - %s", hr, g_world.sheepPenned, g_world.sheep_count, g_world.booksRead, lv, md);
            diaryUntil=frame+5*60; } }
        herder_fb_set_anim(frame);
        herder_fb_set_tint(9.0 + g_world.tick/14400.0);
        herder_fb_draw_world(fb,&g_world);
        herder_fb_weather(fb,&g_world);
        if(frame<rainbowUntil){ int left=rainbowUntil-frame; herder_fb_rainbow(fb, left>120?200:left*200/120); }
        herder_fb_minimap(fb,&g_world);
        herder_fb_hud(fb,&g_world,g_fast);
        if(frame<forecastUntil) herder_fb_forecast(fb,forecast);
        if(frame<curseUntil) herder_fb_curse_banner(fb,curseLine);
        if(frame<toastUntil) herder_fb_toast(fb,toastLine);
        if(frame<diaryUntil) herder_fb_toast(fb,diaryLine);
        if(frame<lineUntil) herder_fb_bubble(fb,&g_world,curline);
        herder_fb_lastheard(fb,lastHeard);
        vid_waitvbl();
        frame++;
    }
    return 0;
}
