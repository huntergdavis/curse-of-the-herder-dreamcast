#include "core/lang/grammar.h"
#include "core/lang/morphology.h"
#include "core/lang/banned.h"
#include "core/rng.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#define MAX_DEPTH 12

/* ---- rng ---- */
typedef struct { uint32_t s; } Rnd;
static double rnext(Rnd *r){ return herder_unit_from_u32(herder_mulberry32_u32(&r->s)); }
static void rnd_seed(Rnd *r, const char *s){ r->s = herder_fnv1a(s); }

/* ---- init: pos and event index lists ---- */
static int *g_pos_idx[HERDER_POS_COUNT]; static int g_pos_n[HERDER_POS_COUNT];
static int *g_ev_idx[HERDER_EVENT_COUNT]; static int g_ev_n[HERDER_EVENT_COUNT];
static int g_init = 0;
void herder_grammar_init(void){
  if(g_init) return;
  g_init=1;
  for(int p=0;p<HERDER_POS_COUNT;p++){ int c=0; for(int i=0;i<HERDER_LEXICON_N;i++) if(HERDER_LEXICON[i].pos==p) c++; g_pos_idx[p]=malloc(sizeof(int)*(c?c:1)); g_pos_n[p]=0; }
  for(int i=0;i<HERDER_LEXICON_N;i++){ int p=HERDER_LEXICON[i].pos; g_pos_idx[p][g_pos_n[p]++]=i; }
  for(int e=0;e<HERDER_EVENT_COUNT;e++){ int c=0; for(int i=0;i<HERDER_RULES_N;i++) if(HERDER_RULES[i].event==e) c++; g_ev_idx[e]=malloc(sizeof(int)*(c?c:1)); g_ev_n[e]=0; }
  for(int i=0;i<HERDER_RULES_N;i++){ int e=HERDER_RULES[i].event; g_ev_idx[e][g_ev_n[e]++]=i; }
}

/* ---- per-line state ---- */
static const char *g_used[128]; static int g_used_n;
static char g_allit; /* 0 = null */
static const char *const *g_ruleReg; static int g_ruleReg_n;

static int str_in(const char *s, const char *const *list, int n){ for(int i=0;i<n;i++) if(strcmp(s,list[i])==0) return 1; return 0; }
static int reg_intersect(const char *const *reg, int rn, const HerderContext *c){
  if(!reg||rn==0||c->registers_n==0) return 0;
  for(int i=0;i<rn;i++) if(str_in(reg[i], c->registers, c->registers_n)) return 1;
  return 0;
}

/* ---- context symbols ---- */
static const char *YAN_TAN[]={"yan","tan","tethera","methera","pip","sethera","lethera","hovera","dovera","dick","yan-a-dick","tan-a-dick","tethera-dick","methera-dick","bumfit","yan-a-bumfit","tan-a-bumfit","tethera-bumfit","methera-bumfit","jiggit"};
static const char *BIG_NUMBERS[]={"ten thousand","a hundred","forty","seven","a thousand","twelve","ninety-nine","a million","eleven","several hundred"};

static const char *time_phrase(double hour, Rnd *r){
  static const double until[]={10,12,14,16.5,18,99};
  static const char *lists[6][4]={
    {"at this hour","before the dew is off","first thing","this early"},
    {"before noon","with the sun climbing","all morning","on a perfectly good morning"},
    {"at midday","with the sun straight up","at lunch, which I have not had","in the heat of the day"},
    {"all afternoon","at this stage of the afternoon","with the shadows getting long","past teatime"},
    {"at dusk","with the light going","at the tail end of the day","as the sun gives up on me"},
    {"in the dark","at night","by moonlight, apparently","after hours"},
  };
  for(int i=0;i<6;i++) if(hour<until[i]) return lists[i][(int)floor(rnext(r)*4)];
  return "today";
}

/* Returns 1 and fills out if `symbol` is a contextual symbol; else 0 (no rnd). */
static int context_symbol(char *out, int cap, const char *symbol, const HerderContext *c, Rnd *r){
  const HerderTarget *t=&c->target;
  if(strcmp(symbol,"target")==0){ snprintf(out,cap,"%s",t->noun); return 1; }
  if(strcmp(symbol,"targets")==0){ herder_pluralize(out,cap,t->noun,NULL); return 1; }
  if(strcmp(symbol,"name")==0){ snprintf(out,cap,"%s",t->name?t->name:t->noun); return 1; }
  if(strcmp(symbol,"vocative")==0){ if(t->name) snprintf(out,cap,"%s",t->name); else if(t->kind==0/*sheep*/) snprintf(out,cap,"%s", str_in("crawler",c->registers,c->registers_n)?"Donut":"sheep"); else snprintf(out,cap,"%s",t->noun); return 1; }
  if(strcmp(symbol,"you")==0){ snprintf(out,cap,"%s",t->name?t->name:"you"); return 1; }
  if(strcmp(symbol,"it")==0){ snprintf(out,cap,"%s",t->name?t->name:(t->plural?"them":"it")); return 1; }
  if(strcmp(symbol,"time")==0){ snprintf(out,cap,"%s",time_phrase(c->hour,r)); return 1; }
  if(strcmp(symbol,"remaining")==0){ herder_number_word(out,cap,c->sheepRemaining); return 1; }
  if(strcmp(symbol,"penned")==0){ herder_number_word(out,cap,c->sheepPenned); return 1; }
  if(strcmp(symbol,"yantan")==0){ int idx=(c->sheepPenned<1?1:c->sheepPenned); if(idx>20)idx=20; snprintf(out,cap,"%s",YAN_TAN[idx-1]); return 1; }
  if(strcmp(symbol,"yantancount")==0){ int n=c->sheepPenned<1?1:c->sheepPenned; int k=n<6?n:6; int o=0; for(int i=0;i<k;i++) o+=snprintf(out+o,cap-o,"%s%s",i?", ":"",YAN_TAN[i]); if(n>6) snprintf(out+o,cap-o,"\xE2\x80\xA6"); return 1; }
  if(strcmp(symbol,"nth")==0){ herder_ordinal_word(out,cap,c->sheepPenned<1?1:c->sheepPenned); return 1; }
  if(strcmp(symbol,"books")==0){ herder_number_word(out,cap,c->booksRead); return 1; }
  if(strcmp(symbol,"sig")==0){ snprintf(out,cap,"%s",c->signatureWord); return 1; }
  if(strcmp(symbol,"village")==0){ snprintf(out,cap,"%s",c->villageName); return 1; }
  if(strcmp(symbol,"dog")==0){ snprintf(out,cap,"%s",c->dogName); return 1; }
  if(strcmp(symbol,"rival")==0){ snprintf(out,cap,"%s",c->rivalName); return 1; }
  if(strcmp(symbol,"bignum")==0){ snprintf(out,cap,"%s",BIG_NUMBERS[(int)floor(rnext(r)*10)]); return 1; }
  if(strcmp(symbol,"hour")==0){ long h=(long)lround(c->hour-9); if(h<1)h=1; herder_number_word(out,cap,h); return 1; }
  if(strcmp(symbol,"flees")==0){ herder_number_word(out,cap,c->st_flees); return 1; }
  if(strcmp(symbol,"fleesNth")==0){ herder_ordinal_word(out,cap,c->st_flees<1?1:c->st_flees); return 1; }
  if(strcmp(symbol,"absurds")==0){ herder_number_word(out,cap,c->st_absurds); return 1; }
  if(strcmp(symbol,"rains")==0){ herder_number_word(out,cap,c->st_rains); return 1; }
  if(strcmp(symbol,"rainsNth")==0){ herder_ordinal_word(out,cap,c->st_rains<1?1:c->st_rains); return 1; }
  if(strcmp(symbol,"shames")==0){ herder_number_word(out,cap,c->st_shames); return 1; }
  return 0;
}

/* ---- entry predicates ---- */
static int entry_known(const LexEntry *e, const HerderContext *c){
  if(e->level<=c->level) return 1;
  if(e->pack && str_in(e->pack, c->knownPacks, c->knownPacks_n)) return 1;
  return 0;
}
static int entry_allowed(const LexEntry *e, const HerderContext *c, int cap, int ignore_targets){
  int mn = c->band<cap?c->band:cap;
  if(e->band>mn) return 0;
  if(e->pos==POS_verb && e->lang && strcmp(e->lang,"en")!=0) return 0;
  if(!entry_known(e,c)) return 0;
  if(!ignore_targets && e->targets && !(e->targets & (1<<c->target.kind))) return 0;
  return 1;
}
static int reg_has_verse_or_rhyme(const char *const *reg, int rn){
  for(int i=0;i<rn;i++){ if(strcmp(reg[i],"verse")==0) return 1; if(strncmp(reg[i],"rhyme:",6)==0) return 1; }
  return 0;
}
static double entry_weight(const LexEntry *e, const HerderContext *c){
  double w=1;
  int active = reg_intersect(e->reg, e->reg_n, c);
  if(active) w*=3;
  if(!active && e->lang && strcmp(e->lang,"en")!=0) w*=0.1;
  if(!active && reg_has_verse_or_rhyme(e->reg, e->reg_n)) w*=0.3;
  if(strcmp(e->w, c->signatureWord)==0) w*=4;
  if(e->level>=c->level-1 && e->level>0) w*=1.6;
  if(c->band>=2 && e->band>0){ int gap=c->band-e->band; w*= gap==0?3.5: gap==1?1.8:1; }
  return w;
}

static int pick_index(const double *w, int n, Rnd *r){
  double total=0; for(int i=0;i<n;i++) total+=w[i];
  double rr=rnext(r)*total;
  for(int i=0;i<n;i++){ rr-=w[i]; if(rr<=0) return i; }
  return n-1;
}

/* ---- forward ---- */
static int expand(char *out, int cap, const char *tmpl, const HerderContext *c, Rnd *r, int depth, int bandCap);

/* resolve: returns 1 and fills textbuf (+ *entry index or -1); else 0. */
static int resolve(char *textbuf, int cap, const char *symbol, const HerderContext *c, Rnd *r, int depth, int band, int allit, int syl, int own, int plural, int *entry_idx){
  *entry_idx=-1;
  if(context_symbol(textbuf, cap, symbol, c, r)) return 1;
  /* non-terminal */
  for(int i=0;i<HERDER_NONTERMS_N;i++) if(strcmp(HERDER_NONTERMS[i].symbol,symbol)==0){
    const NonTerminal *nt=&HERDER_NONTERMS[i];
    int oi[64]; double ow[64]; int on=0;
    for(int k=0;k<nt->n;k++){ const NTOption *o=&nt->options[k]; if(o->min_level<=c->level && o->min_band<=c->band){ oi[on]=k; ow[on]=(o->weight)*(reg_intersect(o->reg,o->reg_n,c)?3:1); on++; } }
    if(on==0) return 0;
    for(int tries=0;tries<3;tries++){ int pick=oi[pick_index(ow,on,r)]; char sub[512]; if(expand(sub,sizeof(sub),nt->options[pick].t,c,r,depth+1,band)){ snprintf(textbuf,cap,"%s",sub); return 1; } }
    return 0;
  }
  /* selfadj */
  if(strcmp(symbol,"selfadj")==0){
    int pool[4096]; int pn=0;
    for(int j=0;j<g_pos_n[POS_adj];j++){ int idx=g_pos_idx[POS_adj][j]; const LexEntry *e=&HERDER_LEXICON[idx];
      if(entry_allowed(e,c,band,1) && (!e->targets || (e->targets & (1<<3/*self*/)))) pool[pn++]=idx; }
    if(pn==0) return 0;
    double w[4096]; for(int j=0;j<pn;j++) w[j]=entry_weight(&HERDER_LEXICON[pool[j]],c);
    int e=pool[pick_index(w,pn,r)];
    g_used[g_used_n++]=HERDER_LEXICON[e].w; *entry_idx=e; snprintf(textbuf,cap,"%s",HERDER_LEXICON[e].w); return 1;
  }
  /* POS */
  int pos=-1; for(int p=0;p<HERDER_POS_COUNT;p++) if(strcmp(HERDER_POS_NAME[p],symbol)==0){ pos=p; break; }
  if(pos>=0){
    int pool[4096]; int pn=0;
    for(int j=0;j<g_pos_n[pos];j++){ int idx=g_pos_idx[pos][j]; if(entry_allowed(&HERDER_LEXICON[idx],c,band,0)) pool[pn++]=idx; }
    if(pn==0) return 0;
    if(plural){ int cp[4096],cn=0; for(int j=0;j<pn;j++){ const char*pl=HERDER_LEXICON[pool[j]].pl; if(!(pl&&strcmp(pl,"-")==0)) cp[cn++]=pool[j]; } if(cn){ memcpy(pool,cp,cn*sizeof(int)); pn=cn; } }
    if(own && g_ruleReg_n){
      /* own filter: entry.reg intersect currentRuleReg */
      int mp2[4096],mn2=0; for(int j=0;j<pn;j++){ const LexEntry*e=&HERDER_LEXICON[pool[j]]; int hit=0; if(e->reg) for(int a=0;a<e->reg_n && !hit;a++) for(int b=0;b<g_ruleReg_n;b++) if(strcmp(e->reg[a],g_ruleReg[b])==0){ hit=1; break; } if(hit) mp2[mn2++]=pool[j]; }
      if(mn2==0) return 0;
      memcpy(pool,mp2,mn2*sizeof(int)); pn=mn2;
    }
    if(allit && g_allit){ int ap[4096],an=0; for(int j=0;j<pn;j++){ char f=(char)tolower((unsigned char)HERDER_LEXICON[pool[j]].w[0]); if(f==g_allit) ap[an++]=pool[j]; } if(an){ memcpy(pool,ap,an*sizeof(int)); pn=an; } }
    if(syl>0){ int sp[4096],sn=0; for(int j=0;j<pn;j++){ const LexEntry*e=&HERDER_LEXICON[pool[j]]; int s=e->syl?e->syl:herder_count_syllables(e->w); if(s==syl) sp[sn++]=pool[j]; } if(sn){ memcpy(pool,sp,sn*sizeof(int)); pn=sn; } else return 0; }
    double w[4096]; for(int j=0;j<pn;j++) w[j]=entry_weight(&HERDER_LEXICON[pool[j]],c);
    int e=pool[pick_index(w,pn,r)];
    if(allit && !g_allit) g_allit=(char)tolower((unsigned char)HERDER_LEXICON[e].w[0]);
    g_used[g_used_n++]=HERDER_LEXICON[e].w; *entry_idx=e; snprintf(textbuf,cap,"%s",HERDER_LEXICON[e].w); return 1;
  }
  return 0;
}

/* ---- modifiers ---- */
static void apply_modifiers(char *io, int cap, const char *mods, const LexEntry *entry){
  if(!mods||!*mods) return;
  char buf[512]; char work[512]; snprintf(work,sizeof(work),"%s",io);
  char m[256]; snprintf(m,sizeof(m),"%s",mods);
  char *save=NULL; char *tok=strtok_r(m,".",&save);
  for(;tok;tok=strtok_r(NULL,".",&save)){
    if(strcmp(tok,"cap")==0) herder_capitalize(buf,sizeof(buf),work);
    else if(strcmp(tok,"up")==0){ int i; for(i=0;work[i];i++) buf[i]=(char)toupper((unsigned char)work[i]); buf[i]=0; }
    else if(strcmp(tok,"a")==0){ if(entry&&entry->pl&&strcmp(entry->pl,"-")==0) snprintf(buf,sizeof(buf),"%s",work); else herder_with_article(buf,sizeof(buf),work); }
    else if(strcmp(tok,"the")==0) snprintf(buf,sizeof(buf),"the %s",work);
    else if(strcmp(tok,"pl")==0) herder_pluralize(buf,sizeof(buf),work, entry?entry->pl:NULL);
    else if(strcmp(tok,"s")==0) herder_verb_form(buf,sizeof(buf),work,"s", entry&&entry->forms[0]?entry->forms[0]:NULL, entry?entry->forms[1]:NULL, entry?entry->forms[2]:NULL);
    else if(strcmp(tok,"ed")==0) herder_verb_form(buf,sizeof(buf),work,"ed", entry&&entry->forms[0]?entry->forms[0]:NULL, entry?entry->forms[1]:NULL, entry?entry->forms[2]:NULL);
    else if(strcmp(tok,"en")==0) herder_verb_form(buf,sizeof(buf),work,"en", entry&&entry->forms[0]?entry->forms[0]:NULL, entry?entry->forms[1]:NULL, entry?entry->forms[2]:NULL);
    else if(strcmp(tok,"ing")==0) herder_verb_form(buf,sizeof(buf),work,"ing", entry&&entry->forms[0]?entry->forms[0]:NULL, entry?entry->forms[1]:NULL, entry?entry->forms[2]:NULL);
    else if(strcmp(tok,"poss")==0){ size_t l=strlen(work); if(l&&work[l-1]=='s') snprintf(buf,sizeof(buf),"%s'",work); else snprintf(buf,sizeof(buf),"%s's",work); }
    else if(strcmp(tok,"quote")==0) snprintf(buf,sizeof(buf),"\"%s\"",work);
    else { continue; }
    snprintf(work,sizeof(work),"%s",buf);
  }
  snprintf(io,cap,"%s",work);
}

/* ---- singularAfterRemaining ---- */
static int has_remaining_slot(const char *t){
  const char *p=t;
  while((p=strstr(p,"#remaining"))){ const char *q=p+10; while(*q=='.'){ q++; while(*q>='a'&&*q<='z') q++; } if(*q=='#') return 1; p+=10; }
  return 0;
}
static void singular_after_remaining(char *out, int cap, const char *tmpl){
  /* find start of #remaining... */
  const char *p=tmpl, *found=NULL;
  while((p=strstr(p,"#remaining"))){ const char *q=p+10; while(*q=='.'){ q++; while(*q>='a'&&*q<='z') q++; } if(*q=='#'){ found=p; break; } p+=10; }
  if(!found){ snprintf(out,cap,"%s",tmpl); return; }
  int hi=(int)(found-tmpl);
  const char *tail=found;
  /* clauseEnd: first [.!?;] not followed by [A-Za-z]+# , or end */
  int ti=0; int clauseEnd=-1;
  for(const char *s=tail; *s; s++,ti++){ char ch=*s; if(ch=='.'||ch=='!'||ch=='?'||ch==';'){ const char *k=s+1; int alpha=0; while(*k>='A'&&*k<='Z'?1:(*k>='a'&&*k<='z')){ k++; alpha++; } if(!(alpha>0 && *k=='#')){ clauseEnd=ti; break; } } }
  if(clauseEnd<0) clauseEnd=(int)strlen(tail);
  char clause[512]; snprintf(clause,sizeof(clause),"%.*s",clauseEnd,tail);
  const char *rest=tail+clauseEnd;
  /* replacements: .pl#->#(all), and first are/have/were/remain/do not */
  char c2[512]; int o=0;
  for(int i=0;clause[i];){ if(strncmp(clause+i,".pl#",4)==0){ c2[o++]='#'; i+=4; } else c2[o++]=clause[i++]; } c2[o]=0;
  /* helper to replace first whole-word occurrence */
  static const char *froms[]={"are","have","were","remain","do not"};
  static const char *tos[]={"is","has","was","remains","does not"};
  for(int w=0;w<5;w++){ char *pos=NULL; int fl=(int)strlen(froms[w]); for(char *s=c2; *s; s++){ if(strncmp(s,froms[w],fl)==0){ int pb=(s==c2)||!(isalnum((unsigned char)s[-1])); int nb=!(isalnum((unsigned char)s[fl])); if(pb&&nb){ pos=s; break; } } } if(pos){ char tmp[512]; int pre=(int)(pos-c2); snprintf(tmp,sizeof(tmp),"%.*s%s%s",pre,c2,tos[w],pos+fl); snprintf(c2,sizeof(c2),"%s",tmp); } }
  snprintf(out,cap,"%.*s%s%s",hi,tmpl,c2,rest);
}

/* ---- expand ---- */
static int is_word(int c){ return isalnum(c)||c=='_'; }
static int expand(char *out, int cap, const char *tmpl_in, const HerderContext *c, Rnd *r, int depth, int bandCap){
  if(depth>MAX_DEPTH) return 0;
  char tmpl[1024];
  if(c->sheepRemaining==1 && has_remaining_slot(tmpl_in)) singular_after_remaining(tmpl,sizeof(tmpl),tmpl_in);
  else snprintf(tmpl,sizeof(tmpl),"%s",tmpl_in);
  int failed=0; int o=0;
  for(int i=0;tmpl[i];){
    if(tmpl[i]!='#'){ if(o<cap-1) out[o++]=tmpl[i]; i++; continue; }
    /* try parse slot */
    int j=i+1; char gateKind=0; int gateNum=-1;
    if((tmpl[j]=='F'||tmpl[j]=='L') && tmpl[j+1]>='0'&&tmpl[j+1]<='9' && tmpl[j+2]==':'){ gateKind=tmpl[j]; gateNum=tmpl[j+1]-'0'; j+=3; }
    int ss=j; if(!(isalpha((unsigned char)tmpl[j])||tmpl[j]=='_')){ if(o<cap-1) out[o++]='#'; i++; continue; }
    while(is_word((unsigned char)tmpl[j])) j++;
    char symbol[64]; int sl=j-ss; if(sl>63)sl=63; memcpy(symbol,tmpl+ss,sl); symbol[sl]=0;
    int ms=j; while(tmpl[j]=='.'){ int k=j+1; if(!((tmpl[k]>='a'&&tmpl[k]<='z')||(tmpl[k]>='0'&&tmpl[k]<='9'))) break; j++; while((tmpl[j]>='a'&&tmpl[j]<='z')||(tmpl[j]>='0'&&tmpl[j]<='9')) j++; }
    char mods[128]; int ml=j-ms; if(ml>127)ml=127; memcpy(mods,tmpl+ms,ml); mods[ml]=0;
    if(tmpl[j]!='#'){ if(o<cap-1) out[o++]='#'; i++; continue; }
    j++; /* consumed slot i..j */
    i=j;
    /* process slot */
    int band=bandCap;
    if(gateKind=='F'){ if(gateNum<band) band=gateNum; }
    if(gateKind=='L' && gateNum>c->level){ failed=1; continue; }
    int wantAllit = strstr(mods,".allit")!=NULL;
    int wantOwn = strstr(mods,".own")!=NULL;
    int wantPl = strstr(mods,".pl")!=NULL;
    int wantSyl=0; { char *sp=strstr(mods,".syl"); if(sp && sp[4]>='0'&&sp[4]<='9') wantSyl=sp[4]-'0'; }
    char val[512]; int eidx;
    if(!resolve(val,sizeof(val),symbol,c,r,depth,band,wantAllit,wantSyl,wantOwn,wantPl,&eidx)){ failed=1; continue; }
    /* mods without .allit/.own/.syl<d> */
    char m2[128]; { snprintf(m2,sizeof(m2),"%s",mods);
      char *a; if((a=strstr(m2,".allit"))) memmove(a,a+6,strlen(a+6)+1);
      if((a=strstr(m2,".own"))) memmove(a,a+4,strlen(a+4)+1);
      if((a=strstr(m2,".syl"))&&a[4]>='0'&&a[4]<='9') memmove(a,a+5,strlen(a+5)+1);
    }
    apply_modifiers(val,sizeof(val),m2, eidx>=0?&HERDER_LEXICON[eidx]:NULL);
    for(int k=0;val[k]&&o<cap-1;k++) out[o++]=val[k];
  }
  out[o]=0;
  return failed?0:1;
}

/* ---- normaliseLine ---- */
static void normalise_line(char *out, int cap, const char *t){
  int o=0,prevsp=1; /* trim leading handled by prevsp */
  for(int i=0;t[i]&&o<cap-1;i++){ int ch=tolower((unsigned char)t[i]); if((ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')){ out[o++]=(char)ch; prevsp=0; } else { if(!prevsp){ out[o++]=' '; prevsp=1; } } }
  while(o>0 && out[o-1]==' ') o--;
  out[o]=0;
}

/* ---- applyHeat ---- */
static int isw2(int c){ return isalpha(c)||c=='\''||c=='-'; }
static void apply_heat(char *t, int cap, const HerderContext *c, Rnd *r){
  size_t n=strlen(t);
  if(n && t[n-1]==')') return;
  if(c->heat>0.55 && n && t[n-1]=='.' && rnext(r)<0.7){ t[n-1]='!'; }
  n=strlen(t);
  if(c->heat>0.8 && n && t[n-1]=='!' && rnext(r)<0.5){ if(n<(size_t)cap-1){ t[n]='!'; t[n+1]=0; n++; } }
  if(c->heat>0.9 && rnext(r)<0.5){
    n=strlen(t); int e=(int)n; while(e>0 && (t[e-1]=='.'||t[e-1]=='!'||t[e-1]=='?')) e--;
    int punctStart=e;
    if(punctStart<(int)n && punctStart>0){
      int w2s=punctStart; while(w2s>0 && isw2((unsigned char)t[w2s-1])) w2s--;
      if(w2s<punctStart){
        int start=w2s;
        if(w2s>0 && t[w2s-1]==' '){ int w1s=w2s-1; while(w1s>0 && isw2((unsigned char)t[w1s-1])) w1s--; if(w1s<w2s-1) start=w1s; }
        if(start>0){ for(int k=start;k<punctStart;k++) t[k]=(char)toupper((unsigned char)t[k]); }
      }
    }
  }
}

/* ---- rule selection ---- */
static int rule_allowed(const Rule *r, const HerderContext *c){
  int minLevel = r->min_level>=0? r->min_level : r->tier;
  if(minLevel>c->level) return 0;
  if(r->min_band>c->band) return 0;
  if(r->targets && !(r->targets & (1<<c->target.kind))) return 0;
  if(r->season && (!c->season || strcmp(r->season,c->season)!=0)) return 0;
  return 1;
}
static int pick_rule(const int *idx, int n, const HerderContext *c, Rnd *r){
  double w[2048];
  for(int i=0;i<n;i++){ const Rule *ru=&HERDER_RULES[idx[i]]; double x=ru->weight; int gap=c->level-ru->tier;
    x*= gap<=0?3: gap==1?2: gap==2?1: gap==3?0.5:0.15;
    if(reg_intersect(ru->reg,ru->reg_n,c)) x*=3;
    if(c->recentRules_n && str_in(ru->id,c->recentRules,c->recentRules_n)) x*=0.12;
    else if(c->rulesToday_n && str_in(ru->id,c->rulesToday,c->rulesToday_n)) x*= strchr(ru->tmpl,'#')?0.7:0.04;
    w[i]=x;
  }
  return idx[pick_index(w,n,r)];
}

/* ---- generate ---- */
HerderGen herder_generate(int event, const HerderContext *c, int salt, int minTier){
  herder_grammar_init();
  HerderGen res; res.ok=0; res.text[0]=0; res.ruleId=NULL; res.tier=0; res.used[0]=0;
  char seedbuf[256]; snprintf(seedbuf,sizeof(seedbuf),"%s|%s|%d|%d",c->seed,HERDER_EVENT_NAME[event],c->tick,salt);
  Rnd r; rnd_seed(&r,seedbuf);
  int cand[2048]; int cn=0;
  for(int i=0;i<g_ev_n[event];i++){ int ri=g_ev_idx[event][i]; if(rule_allowed(&HERDER_RULES[ri],c)) cand[cn++]=ri; }
  if(minTier>=0){ int top[2048],tn=0; for(int i=0;i<cn;i++) if(HERDER_RULES[cand[i]].tier>=minTier) top[tn++]=cand[i]; if(tn){ memcpy(cand,top,tn*sizeof(int)); cn=tn; } }
  if(cn==0) return res;
  for(int attempt=0;attempt<6;attempt++){
    int ri=pick_rule(cand,cn,c,&r);
    const Rule *rule=&HERDER_RULES[ri];
    g_used_n=0; g_allit=0; g_ruleReg=rule->reg; g_ruleReg_n=rule->reg_n;
    char raw[1024];
    if(!expand(raw,sizeof(raw),rule->tmpl,c,&r,0,4)) continue;
    char text[512]; herder_tidy_sentence(text,sizeof(text),raw);
    int isEpitaph = (event==EV_epitaph);
    int skipHeat = isEpitaph || (rule->reg && (str_in("hemingway",(const char*const*)rule->reg,rule->reg_n)||str_in("verse",(const char*const*)rule->reg,rule->reg_n)));
    if(!skipHeat) apply_heat(text,sizeof(text),c,&r);
    int tl=(int)strlen(text);
    if(rule->max_chars && tl>rule->max_chars) continue;
    if(tl>240) continue;
    { char nt[512]; normalise_line(nt,sizeof(nt),text); int dup=0; for(int k=0;k<c->recent_n;k++){ char nr[512]; normalise_line(nr,sizeof(nr),c->recent[k]); if(strcmp(nt,nr)==0){ dup=1; break; } } if(dup) continue; }
    if(herder_find_banned(text)) continue;
    /* success */
    res.ok=1; snprintf(res.text,sizeof(res.text),"%s",text); res.ruleId=rule->id; res.tier=rule->tier;
    int o=0; for(int k=0;k<g_used_n;k++){ int seen=0; for(int m2=0;m2<k;m2++) if(strcmp(g_used[k],g_used[m2])==0){ seen=1; break; } if(!seen) o+=snprintf(res.used+o,sizeof(res.used)-o,"%s%s",o?" ":"",g_used[k]); }
    return res;
  }
  return res;
}

int herder_expand_template(char *out, int cap, const char *tmpl, const HerderContext *c, int salt){
  herder_grammar_init();
  char seedbuf[256]; snprintf(seedbuf,sizeof(seedbuf),"%s|frame|%d|%d",c->seed,c->tick,salt);
  Rnd r; rnd_seed(&r,seedbuf);
  g_used_n=0; g_allit=0; g_ruleReg=NULL; g_ruleReg_n=0;
  char raw[1024];
  if(!expand(raw,sizeof(raw),tmpl,c,&r,0,c->band)) return 0;
  char text[512]; herder_tidy_sentence(text,sizeof(text),raw);
  if(herder_find_banned(text)) return 0;
  snprintf(out,cap,"%s",text); return 1;
}

int herder_known_words(const HerderContext *c){
  herder_grammar_init(); int n=0;
  for(int i=0;i<HERDER_LEXICON_N;i++) if(entry_known(&HERDER_LEXICON[i],c)) n++;
  return n;
}
