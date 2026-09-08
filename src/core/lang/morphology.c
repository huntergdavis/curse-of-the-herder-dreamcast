#include "core/lang/morphology.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define ELL "\xE2\x80\xA6"   /* … */
#define LQ  "\xE2\x80\x9C"   /* “ */
#define RQ  "\xE2\x80\x9D"   /* ” */

static int lc(int c){ return (c>='A'&&c<='Z')?c+32:c; }
static int is_word_ch(int c){ return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'; }
static int is_vowel_ci(int c){ c=lc(c); return c=='a'||c=='e'||c=='i'||c=='o'||c=='u'; }
static int is_upper(int c){ return c>='A'&&c<='Z'; }
static int is_lower(int c){ return c>='a'&&c<='z'; }
static int is_alpha(int c){ return is_upper(c)||is_lower(c); }
static int is_space(int c){ return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f'||c=='\v'; }
static int ends_ci(const char*s,const char*suf){ size_t ls=strlen(s),lf=strlen(suf); if(lf>ls) return 0; for(size_t i=0;i<lf;i++) if(lc((unsigned char)s[ls-lf+i])!=lc((unsigned char)suf[i])) return 0; return 1; }
static int starts_ci(const char*w,const char*p){ while(*p){ if(lc((unsigned char)*w)!=lc((unsigned char)*p)) return 0; w++;p++; } return 1; }
static char *cpy(char*out,int cap,const char*s){ snprintf(out,(size_t)cap,"%s",s); return out; }

/* ---- article ---- */
const char *herder_article(const char *word){
  while(is_space((unsigned char)*word)) word++;
  if(!*word) return "a";
  static const char* A[]={"ewe","europe","euro","eulog","unicorn","unifor","union","unique","unit","univers","use","user","usual","utensil","utopia","one","once","ouija",0};
  for(int i=0;A[i];i++) if(starts_ci(word,A[i])) return "a";
  if(starts_ci(word,"hour")||starts_ci(word,"honest")||starts_ci(word,"honor")||starts_ci(word,"honour")||starts_ci(word,"heir")) return "an";
  if(starts_ci(word,"herb")){ if(!is_word_ch((unsigned char)word[4])) return "an"; }
  return is_vowel_ci((unsigned char)*word)?"an":"a";
}
char *herder_with_article(char*out,int cap,const char*word){ snprintf(out,(size_t)cap,"%s %s",herder_article(word),word); return out; }

char *herder_capitalize(char*out,int cap,const char*s){
  if(!*s){ out[0]=0; return out; }
  snprintf(out,(size_t)cap,"%s",s);
  if(is_lower((unsigned char)out[0])) out[0]=(char)(out[0]-32);
  return out;
}

/* matchCase: uppercase target's first letter iff source[0] is an uppercase letter */
static char *match_case(char*out,int cap,const char*source,const char*target){
  if(is_upper((unsigned char)source[0])) return herder_capitalize(out,cap,target);
  return cpy(out,cap,target);
}

/* ---- pluralize (single word) ---- */
static const char *IRR_PL[][2]={
 {"sheep","sheep"},{"ewe","ewes"},{"ox","oxen"},{"hoof","hooves"},{"foot","feet"},{"tooth","teeth"},
 {"goose","geese"},{"mouse","mice"},{"louse","lice"},{"man","men"},{"woman","women"},{"child","children"},
 {"knife","knives"},{"leaf","leaves"},{"loaf","loaves"},{"calf","calves"},{"half","halves"},{"wolf","wolves"},
 {"life","lives"},{"potato","potatoes"},{"tomato","tomatoes"},{"hero","heroes"},{"cactus","cacti"},{"fungus","fungi"},
 {"radius","radii"},{"crisis","crises"},{"mud","mud"},{"rain","rain"},{"regret","regret"},{"wool","wool"},
 {"grass","grass"},{"despair","despair"},{"misery","misery"},{"woe","woes"},{0,0}};

static char *plural_word(char*out,int cap,const char*word){
  char low[128]; int i=0; for(;word[i]&&i<127;i++) low[i]=(char)lc((unsigned char)word[i]); low[i]=0;
  for(int k=0;IRR_PL[k][0];k++) if(strcmp(low,IRR_PL[k][0])==0) return match_case(out,cap,word,IRR_PL[k][1]);
  size_t n=strlen(word);
  /* already plural: /[^sui]s$/i && !/(?:ss|us|is)$/i */
  if(n>=2 && lc((unsigned char)word[n-1])=='s'){
    int before=lc((unsigned char)word[n-2]);
    if(before!='s'&&before!='u'&&before!='i' && !(ends_ci(word,"ss")||ends_ci(word,"us")||ends_ci(word,"is")))
      return cpy(out,cap,word);
  }
  if(ends_ci(word,"s")||ends_ci(word,"x")||ends_ci(word,"z")||ends_ci(word,"ch")||ends_ci(word,"sh")){ snprintf(out,(size_t)cap,"%ses",word); return out; }
  if(n>=2 && !is_vowel_ci((unsigned char)word[n-2]) && lc((unsigned char)word[n-1])=='y'){ snprintf(out,(size_t)cap,"%.*sies",(int)(n-1),word); return out; }
  if(ends_ci(word,"fe")){ snprintf(out,(size_t)cap,"%.*sves",(int)(n-2),word); return out; }
  snprintf(out,(size_t)cap,"%ss",word); return out;
}

static int in_stop(const char*w){
  static const char* S[]={"of","with","that","who","which","nobody","in","on","at","for","from","without","under","over","by","to",0};
  char low[64]; int i=0; for(;w[i]&&i<63;i++) low[i]=(char)lc((unsigned char)w[i]); low[i]=0;
  for(int k=0;S[k];k++){ if(strcmp(low,S[k])==0) return 1; }
  return 0;
}

char *herder_pluralize(char*out,int cap,const char*word,const char*override){
  if(override && strcmp(override,"-")==0) return cpy(out,cap,word);
  if(override) return cpy(out,cap,override);
  /* split on spaces */
  char buf[512]; cpy(buf,sizeof(buf),word);
  char *parts[64]; int np=0;
  for(char *p=strtok(buf," ");p&&np<64;p=strtok(NULL," ")) parts[np++]=p;
  if(np>1){
    int idx=-1;
    for(int i=1;i<np;i++) if(in_stop(parts[i])){ idx=i; break; }
    int target = (idx>0)? idx-1 : np-1;
    char pl[256]; plural_word(pl,sizeof(pl),parts[target]);
    int o=0;
    for(int i=0;i<np;i++){ const char*w=(i==target)?pl:parts[i]; o+=snprintf(out+o,(size_t)cap-(size_t)o,"%s%s",i?" ":"",w); }
    return out;
  }
  return plural_word(out,cap,word);
}

/* ---- verb forms ---- */
static const char *IRR_V[][4]={
 {"be","is","was","been"},{"have","has","had","had"},{"do","does","did","done"},{"go","goes","went","gone"},
 {"carry","carries","carried","carried"},{"run","runs","ran","run"},{"eat","eats","ate","eaten"},{"bite","bites","bit","bitten"},
 {"fall","falls","fell","fallen"},{"find","finds","found","found"},{"lose","loses","lost","lost"},{"sit","sits","sat","sat"},
 {"stand","stands","stood","stood"},{"hide","hides","hid","hidden"},{"flee","flees","fled","fled"},{"know","knows","knew","known"},
 {"see","sees","saw","seen"},{"get","gets","got","got"},{"take","takes","took","taken"},{"give","gives","gave","given"},
 {"come","comes","came","come"},{"make","makes","made","made"},{"say","says","said","said"},{"think","thinks","thought","thought"},
 {"bring","brings","brought","brought"},{"leave","leaves","left","left"},{"sink","sinks","sank","sunk"},{"climb","climbs","climbed","climbed"},
 {"swim","swims","swam","swum"},{"wet","wets","wet","wet"},{"forget","forgets","forgot","forgotten"},{"rain","rains","rained","rained"},
 {"smite","smites","smote","smitten"},{0,0,0,0}};

/* CVC test: /[^aeiou][aeiou][^aeiouwxy]$/ && len<=5 */
static int cvc(const char*b){
  size_t n=strlen(b); if(n<3||n>5) return 0;
  int c1=lc((unsigned char)b[n-3]), v=lc((unsigned char)b[n-2]), c2=lc((unsigned char)b[n-1]);
  int c1cons = !(c1=='a'||c1=='e'||c1=='i'||c1=='o'||c1=='u');
  int vv = (v=='a'||v=='e'||v=='i'||v=='o'||v=='u');
  int c2ok = !(c2=='a'||c2=='e'||c2=='i'||c2=='o'||c2=='u'||c2=='w'||c2=='x'||c2=='y');
  return c1cons&&vv&&c2ok;
}
char *herder_verb_form(char*out,int cap,const char*base,const char*form,const char*ov_s,const char*ov_ed,const char*ov_en){
  const char *fs=ov_s,*fe=ov_ed,*fn=ov_en;
  if(!fs){ char low[64]; int i=0; for(;base[i]&&i<63;i++) low[i]=(char)lc((unsigned char)base[i]); low[i]=0;
    for(int k=0;IRR_V[k][0];k++) if(strcmp(low,IRR_V[k][0])==0){ fs=IRR_V[k][1]; fe=IRR_V[k][2]; fn=IRR_V[k][3]; break; } }
  if(fs){
    if(strcmp(form,"s")==0) return cpy(out,cap,fs);
    if(strcmp(form,"ed")==0) return cpy(out,cap,fe);
    if(strcmp(form,"en")==0) return cpy(out,cap,fn);
  }
  size_t n=strlen(base);
  if(strcmp(form,"ing")==0){
    if(n>=2 && ends_ci(base,"ie")) { snprintf(out,(size_t)cap,"%.*sying",(int)(n-2),base); return out; }
    if(n>=2 && lc((unsigned char)base[n-1])=='e' && lc((unsigned char)base[n-2])!='e'){ snprintf(out,(size_t)cap,"%.*sing",(int)(n-1),base); return out; }
    if(cvc(base)){ snprintf(out,(size_t)cap,"%s%cing",base,base[n-1]); return out; }
    snprintf(out,(size_t)cap,"%sing",base); return out;
  }
  if(strcmp(form,"s")==0){
    if(ends_ci(base,"s")||ends_ci(base,"x")||ends_ci(base,"z")||ends_ci(base,"ch")||ends_ci(base,"sh")){ snprintf(out,(size_t)cap,"%ses",base); return out; }
    if(n>=2 && !is_vowel_ci((unsigned char)base[n-2]) && lc((unsigned char)base[n-1])=='y'){ snprintf(out,(size_t)cap,"%.*sies",(int)(n-1),base); return out; }
    snprintf(out,(size_t)cap,"%ss",base); return out;
  }
  /* ed / en (past) */
  if(n>=1 && lc((unsigned char)base[n-1])=='e'){ snprintf(out,(size_t)cap,"%sd",base); return out; }
  if(n>=2 && !is_vowel_ci((unsigned char)base[n-2]) && lc((unsigned char)base[n-1])=='y'){ snprintf(out,(size_t)cap,"%.*sied",(int)(n-1),base); return out; }
  if(cvc(base)){ snprintf(out,(size_t)cap,"%s%ced",base,base[n-1]); return out; }
  snprintf(out,(size_t)cap,"%sed",base); return out;
}

/* ---- number words ---- */
static const char *ONES[]={"zero","one","two","three","four","five","six","seven","eight","nine","ten","eleven","twelve","thirteen","fourteen","fifteen","sixteen","seventeen","eighteen","nineteen"};
static const char *TENS[]={"","","twenty","thirty","forty","fifty","sixty","seventy","eighty","ninety"};
char *herder_number_word(char*out,int cap,long n){
  if(n<0){ char t[128]; herder_number_word(t,sizeof(t),-n); snprintf(out,(size_t)cap,"minus %s",t); return out; }
  if(n<20) return cpy(out,cap,ONES[n]);
  if(n<100){ if(n%10) snprintf(out,(size_t)cap,"%s-%s",TENS[n/10],ONES[n%10]); else cpy(out,cap,TENS[n/10]); return out; }
  if(n<1000){ char rest[160]; if(n%100){ char r2[128]; herder_number_word(r2,sizeof(r2),n%100); snprintf(rest,sizeof(rest)," and %s",r2);} else rest[0]=0; snprintf(out,(size_t)cap,"%s hundred%s",ONES[n/100],rest); return out; }
  snprintf(out,(size_t)cap,"%ld",n); return out;
}
char *herder_ordinal_word(char*out,int cap,long n){
  switch(n){ case 1:return cpy(out,cap,"first"); case 2:return cpy(out,cap,"second"); case 3:return cpy(out,cap,"third");
             case 5:return cpy(out,cap,"fifth"); case 8:return cpy(out,cap,"eighth"); case 9:return cpy(out,cap,"ninth"); case 12:return cpy(out,cap,"twelfth"); }
  if(n>20&&n<100&&n%10!=0){ char t[64]; herder_ordinal_word(t,sizeof(t),n%10); snprintf(out,(size_t)cap,"%s-%s",TENS[n/10],t); return out; }
  char w[128]; herder_number_word(w,sizeof(w),n);
  size_t l=strlen(w);
  if(l>=1 && w[l-1]=='y'){ snprintf(out,(size_t)cap,"%.*sieth",(int)(l-1),w); return out; }
  snprintf(out,(size_t)cap,"%sth",w); return out;
}

/* ---- syllables ---- */
int herder_count_syllables(const char*word){
  /* lower, non-a-z -> space, trim, split */
  char clean[256]; int ci=0;
  for(int i=0;word[i]&&ci<255;i++){ int c=lc((unsigned char)word[i]); clean[ci++]= (c>='a'&&c<='z')? (char)c : ' '; }
  clean[ci]=0;
  int total=0; char *save=NULL; (void)save;
  char tmp[256]; cpy(tmp,sizeof(tmp),clean);
  for(char *part=strtok(tmp," ");part;part=strtok(NULL," ")){
    char p[128]; cpy(p,sizeof(p),part);
    size_t pl=strlen(p);
    if(pl>=1 && p[pl-1]=='e'){ p[pl-1]=0; pl--; }               /* replace /e$/ */
    if(pl>=2 && (p[pl-1]=='s'||p[pl-1]=='d') && p[pl-2]=='e'){ p[pl-2]=0; pl-=2; } /* /(es|ed)$/ */
    const char *use = pl? p : part;
    int n=0,in=0;
    for(const char*q=use;*q;q++){ int v=(*q=='a'||*q=='e'||*q=='i'||*q=='o'||*q=='u'||*q=='y'); if(v&&!in){ n++; in=1; } else if(!v) in=0; }
    if(n==0) n=1;
    /* /[^aeiou]le$/.test(part) */
    size_t partl=strlen(part);
    if(partl>=3 && part[partl-1]=='e'&&part[partl-2]=='l'){ int b=part[partl-3]; if(!(b=='a'||b=='e'||b=='i'||b=='o'||b=='u')) n++; }
    total += (n<1?1:n);
  }
  return total;
}

/* ---- tidySentence ---- */
static int is_open(int c){ return c=='('||c=='['||c=='"'||c=='\''; }
static int match3(const char*s,const char*m){ return (unsigned char)s[0]==(unsigned char)m[0]&&(unsigned char)s[1]==(unsigned char)m[1]&&(unsigned char)s[2]==(unsigned char)m[2]; }

char *herder_tidy_sentence(char*out,int cap,const char*in){
  static char a[8192],b[8192];
  /* Pass A: "..." -> … */
  { int o=0; for(int i=0;in[i];){ if(in[i]=='.'&&in[i+1]=='.'&&in[i+2]=='.'){ a[o++]=(char)0xE2;a[o++]=(char)0x80;a[o++]=(char)0xA6; i+=3; } else a[o++]=in[i++]; } a[o]=0; }
  /* Pass B: \s+ -> " " */
  { int o=0,i=0; while(a[i]){ if(is_space((unsigned char)a[i])){ b[o++]=' '; while(is_space((unsigned char)a[i])) i++; } else b[o++]=a[i++]; } b[o]=0; }
  /* Pass C: \s+([,.!?;:]) -> $1  (drop the single spaces before punctuation) */
  { int o=0,i=0; while(b[i]){ if(b[i]==' '){ int j=i; while(b[j]==' ') j++; char nx=b[j]; if(nx==','||nx=='.'||nx=='!'||nx=='?'||nx==';'||nx==':'){ i=j; continue; } a[o++]=b[i++]; } else a[o++]=b[i++]; } a[o]=0; }
  /* Pass D: ([,;:])(?=\S) -> "$1 " */
  { int o=0,i=0; while(a[i]){ char c=a[i]; b[o++]=c; if((c==','||c==';'||c==':') && a[i+1] && !is_space((unsigned char)a[i+1])) b[o++]=' '; i++; } b[o]=0; }
  /* Pass E: trim -> a */
  { int s=0; while(b[s]&&is_space((unsigned char)b[s])) s++; int e=(int)strlen(b); while(e>s&&is_space((unsigned char)b[e-1])) e--; int o=0; for(int i=s;i<e;i++) a[o++]=b[i]; a[o]=0; }
  /* Pass F: article fix -> b */
  { int o=0,i=0; int len=(int)strlen(a);
    while(i<len){
      int boundary = (i==0)|| !is_word_ch((unsigned char)a[i-1]);
      int matched=0;
      if(boundary && (a[i]=='a'||a[i]=='A')){
        const char *art=NULL; int tl=0;
        if(is_space((unsigned char)a[i+1])){ art=(a[i]=='A')?"A":"a"; tl=1; }
        else if(a[i+1]=='n' && is_space((unsigned char)a[i+2])){ art=(a[i]=='A')?"An":"an"; tl=2; }
        if(art){
          int j=i+tl; int sp=j; while(is_space((unsigned char)a[j])) j++;  /* \s+ */
          if(j>sp && is_alpha((unsigned char)a[j])){
            int ws=j; while(a[j] && (is_word_ch((unsigned char)a[j])||a[j]=='-')) j++;  /* word [A-Za-z][\w-]* */
            char word[256]; int wl=j-ws; if(wl>255) wl=255; memcpy(word,a+ws,(size_t)wl); word[wl]=0;
            const char *na=herder_article(word);
            char fixed[8]; if(art[0]=='A'){ herder_capitalize(fixed,sizeof(fixed),na); } else cpy(fixed,sizeof(fixed),na);
            o+=snprintf(b+o,sizeof(b)-(size_t)o,"%s",fixed);
            for(int k=sp;k<ws;k++) b[o++]=a[k];   /* the whitespace run */
            for(int k=ws;k<j;k++) b[o++]=a[k];     /* the word */
            i=j; matched=1;
          }
        }
      }
      if(!matched) b[o++]=a[i++];
    }
    b[o]=0;
  }
  /* Pass G: sentence-start capitalisation (in place on b) */
  { int len=(int)strlen(b); int i=0;
    while(i<len){
      int g1end=-1;
      if(i==0) g1end=0;
      else if((b[i]=='.'||b[i]=='!'||b[i]=='?') && is_space((unsigned char)b[i+1])){ int j=i+1; while(is_space((unsigned char)b[j])) j++; g1end=j; }
      else if(b[i]=='"'){ int j=i+1; while(is_space((unsigned char)b[j])) j++; g1end=j; }
      else if(match3(b+i,LQ)){ int j=i+3; while(is_space((unsigned char)b[j])) j++; g1end=j; }
      if(g1end>=0){
        int j=g1end; while(b[j] && (is_open((unsigned char)b[j])||match3(b+j,LQ))){ j+= match3(b+j,LQ)?3:1; }  /* group2 opens */
        if(is_lower((unsigned char)b[j])){ b[j]=(char)(b[j]-32); i=j+1; continue; }
      }
      i++;
    }
  }
  /* Pass H: \bi\b -> I (in place on b) */
  { int len=(int)strlen(b); for(int i=0;i<len;i++){ if(b[i]=='i'){ int pb=(i==0)||!is_word_ch((unsigned char)b[i-1]); int nb=(i+1>=len)||!is_word_ch((unsigned char)b[i+1]); if(pb&&nb) b[i]='I'; } } }
  /* Pass I: terminal punctuation */
  { int len=(int)strlen(b);
    int hasTerm=0;
    if(len>=1){ char c=b[len-1]; if(c=='.'||c=='!'||c=='?'||c=='"') hasTerm=1; }
    if(!hasTerm && len>=3 && (match3(b+len-3,ELL)||match3(b+len-3,RQ))) hasTerm=1;
    int hasParen=0;
    if(len>=2 && b[len-1]==')'){ char c=b[len-2]; if(c=='.'||c=='!'||c=='?') hasParen=1; if(len>=4 && match3(b+len-4,ELL)) hasParen=1; }
    if(!hasTerm && !hasParen){ b[len]='.'; b[len+1]=0; }
  }
  return cpy(out,cap,b);
}
