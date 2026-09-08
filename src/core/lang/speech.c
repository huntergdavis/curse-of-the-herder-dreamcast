#include "core/lang/speech.h"
#include "core/lang/grammar.h"
#include "core/map/terrain.h"
#include "core/map/generate.h"
#include "core/names.h"
#include "core/progression.h"
#include "core/rng.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#define TICKS_PER_HOUR 14400.0

static double ku(const char *seed, const char *dom){ char b[128]; snprintf(b,sizeof(b),"%s|%s",seed,dom); uint32_t s=herder_fnv1a(b); return herder_unit_from_u32(herder_mulberry32_u32(&s)); }
static double kui(const char *seed, const char *dom, long i){ char b[160]; snprintf(b,sizeof(b),"%s|%s|%ld",seed,dom,i); uint32_t s=herder_fnv1a(b); return herder_unit_from_u32(herder_mulberry32_u32(&s)); }

static const char *SIGNATURE_WORDS[]={"turnip","bucket","parsnip","cabbage","sock","thistle","puddle","trough","wheelbarrow","stile","haystack","pebble"};
const char *herder_signature_word(const char *seed){ return SIGNATURE_WORDS[(int)floor(ku(seed,"signature")*12)]; }

static const char *terrain_noun(const HerderMap *m, double x, double y, double rnd){
  int t=m->terrain[(int)lround(y)*m->size+(int)lround(x)];
  switch(t){
    case T_Mud: return rnd<0.5?"mud":"bog";
    case T_Rock: return rnd<0.5?"rock":"scree";
    case T_Forest: return rnd<0.5?"bramble":"wood";
    case T_Farm: return "furrow";
    case T_Sand: return "sand";
    case T_Water: case T_Bridge: return "river";
    case T_Road: return "road";
    default: return rnd<0.5?"hill":rnd<0.8?"field":"slope";
  }
}

static double hours_elapsed(const HerderWorld *w){ return w->tick / TICKS_PER_HOUR; }

/* target kinds by index: sheep0 terrain1 weather2 self3 curse4 pantheon5 book6 pen7 sky8 day9 */
static void sheep_target(const HerderWorld *w, int id, HerderTarget *t){
  const HerderSheep *s = (id>=0 && id<w->sheep_count)?&w->sheep[id]:NULL;
  t->kind=0; t->noun="sheep"; t->name = (s && s->named)? HERDER_SHEEP[herder_sheep_name(w->seed,id)] : NULL; t->plural=0;
}
static void pick_target(const HerderWorld *w, const HerderEvent *e, HerderTarget *t){
  if(e && e->sheep_id>=0){ sheep_target(w,e->sheep_id,t); return; }
  double r=kui(w->seed,"target",w->tick);
  if(r<0.45 && w->h.targetSheep>=0){ sheep_target(w,w->h.targetSheep,t); return; }
  if(r<0.8){ t->kind=1; t->noun=terrain_noun(w->map,w->h.x,w->h.y,kui(w->seed,"terrain-noun",w->tick)); t->name=NULL; t->plural=0; return; }
  if(r<0.9){ t->kind=9; t->noun="day"; t->name=NULL; t->plural=0; return; }
  t->kind=4; t->noun="Curse"; t->name=NULL; t->plural=0;
}

static const char *g_regs[16];
void herder_build_context(HerderContext *c, const HerderWorld *w, const HerderEvent *e, int bandCap){
  memset(c,0,sizeof(*c));
  double hours=hours_elapsed(w);
  c->seed=w->seed; c->tick=w->tick;
  c->level=herder_level_for(herder_erudition(w->booksRead, w->sheepPenned, hours));
  int fc=herder_filth_ceiling(w->frustration); c->band = bandCap<fc?bandCap:fc;
  c->heat=w->frustration/100.0;
  c->hour=9+hours;
  pick_target(w,e,&c->target);
  int rn=0; for(int i=0;i<w->registers_n;i++) if(w->registers[i].untilTick>w->tick) g_regs[rn++]=w->registers[i].reg;
  c->registers=g_regs; c->registers_n=rn;
  c->signatureWord=herder_signature_word(w->seed);
  c->sheepRemaining=w->sheep_count - w->sheepPenned;
  c->sheepPenned=w->sheepPenned; c->booksRead=w->booksRead;
  c->recent=NULL; c->recent_n=0; c->recentRules=NULL; c->recentRules_n=0; c->rulesToday=NULL; c->rulesToday_n=0;
  c->knownPacks=w->knownPacks; c->knownPacks_n=w->knownPacks_n;
  /* nearest village */
  double bd=INFINITY; const char *bn="the village";
  for(int v=0;v<w->map->village_count;v++){ double d=hypot((double)w->map->villages[v].x-w->h.x,(double)w->map->villages[v].y-w->h.y); if(d<bd){ bd=d; bn=HERDER_VILLAGE_NAMES[w->map->villages[v].name]; } }
  c->villageName=bn;
  c->dogName=HERDER_DOGS[herder_dog_name(w->seed)];
  c->rivalName=HERDER_RIVALS[herder_rival_name(w->seed)];
  c->season=w->season;
  c->st_flees=w->st_flees; c->st_absurds=w->st_absurds; c->st_shames=w->st_shames; c->st_rains=w->st_rains; c->st_breathers=w->st_breathers; c->st_books=w->st_books;
}

/* kind string -> RuleEvent name (or NULL) */
static const char *event_map(const char *kind){
  static const char *M[][2]={
    {"flee","flee"},{"caught","caught"},{"penned","penned"},{"absurd","absurd"},{"repeatEscape","repeatEscape"},
    {"finished","finished"},{"book","book"},{"walkOfShame","walkOfShame"},{"breather","breather"},{"rain","rain"},
    {"bookPassed","bookPassed"},{"rant","rant"},{"fog","fog"},{"gaze","gaze"},{"mishap","idle"},{"started","dawn"},
    {"curious","curious"},{"dozy","dozy"},{"wind","wind"},{"lunch","lunch"},{"black","black"},{"milestone","idle"},
    {"scarecrow","scarecrow"},{"jailbreak","jailbreak"},{"streak","streak"},{"reread","reread"},{"rainStops","rainStops"},
    {"fogLifts","fogLifts"},{"windDrops","windDrops"},{"streakBroken","streakBroken"},{"recaptured","recaptured"},
    {"nemesis","nemesis"},{"nemesisCaught","nemesisCaught"},{"rival","rival"},{"rivalGone","rivalGone"},
    {"lunchStolen","lunchStolen"},{"thiefCaught","thiefCaught"},{"dogHelps","dogHelps"},{"rivalBolt","rivalBolt"},
    {"drink","drink"},{"flytingReply","flytingReply"},{NULL,NULL}};
  for(int i=0;M[i][0];i++) if(strcmp(M[i][0],kind)==0) return M[i][1];
  return NULL;
}
static int evid(const char *name){ for(int i=0;i<HERDER_EVENT_COUNT;i++) if(strcmp(HERDER_EVENT_NAME[i],name)==0) return i; return -1; }

static double heat_bump(int ev){
  switch(ev){
    case EV_flee:return 0.25; case EV_repeatEscape:return 0.4; case EV_absurd:return 0.3; case EV_penned:return -0.2;
    case EV_finished:return 0.5; case EV_rant:return 0.3; case EV_gaze:return -0.5; case EV_bog:return 0.35;
    case EV_nettles:return 0.35; case EV_stub:return 0.4; case EV_cowpat:return 0.3; case EV_wasp:return 0.45;
    case EV_bite:return 0.4; case EV_gate:return 0.35; case EV_molehill:return 0.3; case EV_heave:return 0.3;
    case EV_curious:return -0.4; case EV_dozy:return -0.1; case EV_crook:return 0.5; case EV_lunch:return -0.6;
    case EV_scarecrow:return -0.2; case EV_jailbreak:return 0.5; case EV_streak:return -0.3; case EV_reread:return 0.1;
    case EV_rainStops:return -0.2; case EV_fogLifts:return -0.2; case EV_windDrops:return -0.2; case EV_streakBroken:return 0.4;
    case EV_recaptured:return 0.3; case EV_nemesis:return 0.5; case EV_nemesisCaught:return -0.1; case EV_rival:return 0.5;
    case EV_rivalGone:return 0.3; case EV_lunchStolen:return 0.6; case EV_thiefCaught:return 0.2; case EV_dogHelps:return -0.4;
    case EV_rivalBolt:return -0.2; case EV_drink:return -0.3; case EV_flytingReply:return 0.35; default:return 0;
  }
}

static int count_words(const char *t){ int n=0,in=0; for(const char*p=t;*p;p++){ int sp=(*p==' '||*p=='\t'||*p=='\n'); if(!sp&&!in){ n++; in=1; } else if(sp) in=0; } return n?n:1; }
static double hold_seconds(const char *text, double heat){ double w=count_words(text); double s=1.6+w*0.42+heat*0.5; if(s<2.2)s=2.2; if(s>14)s=14; return s; }
static void target_label(char *out,int cap,const HerderTarget *t){
  if(t->name){ snprintf(out,cap,"%s",t->name); return; }
  if(t->kind==0) snprintf(out,cap,"the sheep");
  else if(t->kind==1) snprintf(out,cap,"the %s",t->noun);
  else if(t->kind==9) snprintf(out,cap,"the day");
  else if(t->kind==4) snprintf(out,cap,"the Curse");
  else snprintf(out,cap,"%s",t->noun);
}

static int was_on_roof(const HerderWorld *w, int id){ if(id<0||id>=w->sheep_count) return 0; const HerderSheep*s=&w->sheep[id]; int d=w->map->deco[(int)lround(s->home_y)*w->map->size+(int)lround(s->home_x)]; return d==D_House||d==D_HouseRed; }
static int was_in_river(const HerderWorld *w, int id){ if(id<0||id>=w->sheep_count) return 0; const HerderSheep*s=&w->sheep[id]; return w->map->terrain[(int)lround(s->home_y)*w->map->size+(int)lround(s->home_x)]==T_Water; }

static HerderUtterance make_utt(const HerderGen *g, const HerderContext *ctx){
  HerderUtterance u; u.ok=1; snprintf(u.text,sizeof(u.text),"%s",g->text); u.heat=ctx->heat; u.seconds=hold_seconds(g->text,ctx->heat); u.ruleId=g->ruleId; snprintf(u.used,sizeof(u.used),"%s",g->used); u.sheepId=-1; target_label(u.targetLabel,sizeof(u.targetLabel),&ctx->target); return u;
}

HerderUtterance herder_speak_for_event(const HerderWorld *w, const HerderEvent *e, int bandCap){
  HerderUtterance nul; memset(&nul,0,sizeof(nul)); nul.ok=0; nul.sheepId=-1;
  const char *evn=event_map(e->kind); if(!evn) return nul;
  int ev=evid(evn);
  if((strcmp(e->kind,"mishap")==0||strcmp(e->kind,"milestone")==0) && e->detail[0]) ev=evid(e->detail);
  if(strcmp(e->kind,"absurd")==0){ if(was_on_roof(w,e->sheep_id)) ev=EV_roof; else if(was_in_river(w,e->sheep_id)) ev=EV_river; else if(e->sheep_id>=0 && e->sheep_id<w->sheep_count){ const HerderSheep*s=&w->sheep[e->sheep_id]; if(w->map->deco[(int)lround(s->home_y)*w->map->size+(int)lround(s->home_x)]==D_Boulder) ev=EV_boulder; } }
  if(strcmp(e->kind,"penned")==0 && w->sheepPenned>=6 && kui(w->seed,"miscount",w->sheepPenned)<0.18) ev=EV_miscount;
  HerderContext ctx; herder_build_context(&ctx,w,e,bandCap);
  double b=ctx.heat+heat_bump(ev); if(b<0)b=0; if(b>1)b=1; ctx.heat=b;
  HerderGen g=herder_generate(ev,&ctx,e->sheep_id,-1);
  if(!g.ok && ev==EV_rant) g=herder_generate(EV_idle,&ctx,e->sheep_id,-1);
  if(!g.ok) return nul;
  HerderUtterance u=make_utt(&g,&ctx); return u;
}


HerderUtterance herder_speak_kind(const HerderWorld *w, int ev, int bandCap, double heatBump){
    HerderUtterance nul; memset(&nul,0,sizeof(nul)); nul.ok=0; nul.sheepId=-1;
    HerderContext ctx; herder_build_context(&ctx,w,NULL,bandCap);
    double b=ctx.heat+heatBump; if(b<0)b=0; if(b>1)b=1; ctx.heat=b;
    HerderGen g=herder_generate(ev,&ctx,w->tick,-1);
    if(!g.ok) return nul;
    return make_utt(&g,&ctx);
}

HerderUtterance herder_speak_idle(const HerderWorld *w, int bandCap){
  HerderUtterance nul; memset(&nul,0,sizeof(nul)); nul.ok=0; nul.sheepId=-1;
  HerderContext ctx; herder_build_context(&ctx,w,NULL,bandCap);
  int ev=EV_idle;
  double u=kui(w->seed,"idle-kind",w->tick);
  if(ctx.hour>=16.5 && u<0.3) ev=EV_dusk;
  else if(u>0.92 && w->st_flees>=2 && w->st_rains>=1 && w->st_shames>=1) ev=EV_callback;
  HerderGen g=herder_generate(ev,&ctx,0,-1);
  if(!g.ok) g=herder_generate(EV_idle,&ctx,0,-1);
  if(!g.ok) return nul;
  HerderUtterance out=make_utt(&g,&ctx);
  if(ctx.target.kind==0 && w->h.targetSheep>=0) out.sheepId=w->h.targetSheep;
  return out;
}

HerderUtterance herder_speak_epitaph(const HerderWorld *w, int bandCap){
  HerderContext ctx; herder_build_context(&ctx,w,NULL,bandCap);
  ctx.heat=1; ctx.band = bandCap<4?bandCap:4;
  int mt=ctx.level-1; if(mt<0)mt=0;
  HerderGen g=herder_generate(EV_epitaph,&ctx,0,mt);
  for(int salt=1;(!g.ok||strlen(g.text)>110)&&salt<12;salt++) g=herder_generate(EV_epitaph,&ctx,salt,mt);
  HerderUtterance u; u.ok=1; u.sheepId=-1; u.heat=1; u.seconds=30; target_label(u.targetLabel,sizeof(u.targetLabel),&ctx.target); u.used[0]=0;
  if(!g.ok||strlen(g.text)>110){ snprintf(u.text,sizeof(u.text),"Sheep."); u.ruleId="fallback"; }
  else { snprintf(u.text,sizeof(u.text),"%s",g.text); u.ruleId=g.ruleId; }
  return u;
}

int herder_next_idle_curse_ticks(const HerderWorld *w){
  int level=herder_level_for(herder_erudition(w->booksRead,w->sheepPenned,hours_elapsed(w)));
  double seconds=herder_curse_interval_seconds(level,w->frustration);
  double jitter=0.7+kui(w->seed,"curse-jitter",w->tick)*0.6;
  long v=lround(seconds*jitter/0.25);
  return v<20?20:(int)v;
}
