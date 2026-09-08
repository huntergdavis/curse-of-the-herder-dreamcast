#include "core/lang/banned.h"
#include <string.h>
#include <stdio.h>

/* The banned patterns, verbatim from banned.ts (the /.../i bodies).  Matched
 * case-insensitively against text already lowercased+leet-normalised. The
 * mini-matcher below supports exactly the constructs these use: \b, literal
 * chars, [class], (?:alt|alt), and a trailing ? on any atom. */
static const char *PATTERNS[] = {
  "\\bn[i1]gg?(?:e|a|u)r?s?\\b",
  "\\bch[i1]nks?\\b",
  "\\bg[o0]{2}ks?\\b",
  "\\bk[i1]kes?\\b",
  "\\bsp[i1]cs?\\b",
  "\\bwetbacks?\\b",
  "\\btowel ?heads?\\b",
  "\\bragheads?\\b",
  "\\bgyps?(?:y|ies)\\b",
  "\\bpak[i1]s?\\b",
  "\\bredskins?\\b",
  "\\bsquaws?\\b",
  "\\bcoons?\\b",
  "\\bdarkies?\\b",
  "\\bhalf[- ]?breeds?\\b",
  "\\bjap(?:s)?\\b",
  "\\bpolacks?\\b",
  "\\bkrauts?\\b",
  "\\bwops?\\b",
  "\\bdagos?\\b",
  "\\bmicks?\\b",
  "\\byids?\\b",
  "\\bhebes?\\b",
  "\\bzipperheads?\\b",
  "\\bfagg?(?:ot|it)s?\\b",
  "\\bfags?\\b",
  "\\bdykes?\\b",
  "\\btrann(?:y|ies)\\b",
  "\\bshemales?\\b",
  "\\bhomos?\\b",
  "\\bqueers?\\b",
  "\\bpoof(?:ter)?s?\\b",
  "\\bso gay\\b",
  "\\bthat'?s gay\\b",
  "\\bretard(?:ed|s)?\\b",
  "\\bspaz(?:z|tic|zes|es)?\\b",
  "\\bspastics?\\b",
  "\\bcretins?\\b",
  "\\bimbeciles?\\b",
  "\\bmorons?\\b",
  "\\bmongoloids?\\b",
  "\\bmongs?\\b",
  "\\bpsychos?\\b",
  "\\bschizos?\\b",
  "\\bidiots?\\b",
  "\\bidiotic\\b",
  "\\bdumb\\b",
  "\\blame\\b",
  "\\bcrazy\\b",
  "\\binsane\\b",
  "\\blunatics?\\b",
  "\\bcripples?\\b",
  "\\bcrippled\\b",
  "\\bmidgets?\\b",
  "\\bdwarf\\b",
  "\\bfat(?:ty|so|ass)?\\b",
  "\\bugly\\b",
  "\\bc[u*]nts?\\b",
  "\\bb[i1]tch(?:es|y)?\\b",
  "\\bwhores?\\b",
  "\\bsluts?\\b",
  "\\bskanks?\\b",
  "\\bhysterical\\b",
  "\\btw[a@]ts?\\b",
  "\\bp[u*]ss(?:y|ies)\\b",
  "\\bc[o0]cks?(?:sucker)?s?\\b",
  "\\bmotherfucker\\b",
  "\\brape[sd]?\\b",
  "\\brapist\\b",
  "\\bmolest",
  "\\bpedo",
  "\\bincest",
  "\\bkill (?:your|my)self\\b",
  "\\bsuicide\\b",
  "\\bhang (?:your|my)self\\b",
  "\\bcrackhead\\b",
  "\\bjunkie\\b",
  "\\bnazi\\b",
  "\\bhitler\\b",
  "\\bjihad",
  "\\bterrorist",
  NULL
};

static int lc(int c){ return (c>='A'&&c<='Z')?c+32:c; }
static int is_word_ch(int c){ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'; }

/* Fold a UTF-8 Latin-1 Supplement / Latin Extended-A precomposed letter (the
 * NFD-strip path) to its base ASCII letter. Advances *i past the sequence and
 * returns the base char, or -1 if not a foldable sequence. */
static int fold_utf8(const char *s, int *i){
  unsigned char c0=(unsigned char)s[*i];
  if(c0<0x80) return -1;
  if(c0==0xC3){
    unsigned char c1=(unsigned char)s[*i+1]; *i+=2;
    /* U+00C0..U+00FF */
    unsigned cp=0xC0+(c1-0x80);
    static const char *map=
      "AAAAAAECEEEEIIIIDNOOOOOxOUUUUYPs"  /* C0..DF (x=multiply, P=thorn, s=eszett) */
      "aaaaaaeceeeeiiiidnooooo/ouuuuypy"; /* E0..FF (/=divide) */
    if(cp>=0xC0&&cp<=0xFF){ char b=map[cp-0xC0]; return (b=='x'||b=='/')? '?' : b; }
    return '?';
  }
  /* other multibyte: skip one continuation-led sequence, return sentinel */
  if((c0&0xE0)==0xC0){ *i+=2; return '?'; }
  if((c0&0xF0)==0xE0){ *i+=3; return '?'; }
  if((c0&0xF8)==0xF0){ *i+=4; return '?'; }
  *i+=1; return '?';
}

void herder_normalise_for_ban(char *out, int cap, const char *text){
  char tmp[4096]; int o=0;
  for(int i=0; text[i] && o<4090; ){
    unsigned char c=(unsigned char)text[i];
    if(c<0x80){ tmp[o++]=(char)lc(c); i++; }
    else { int base=fold_utf8(text,&i); if(base>0) tmp[o++]=(char)lc(base); }
  }
  tmp[o]=0;
  /* leet: [1!|]->i, 0->o, @->a, $->s, 3->e */
  for(int i=0;tmp[i];i++){ char c=tmp[i];
    if(c=='1'||c=='!'||c=='|') tmp[i]='i';
    else if(c=='0') tmp[i]='o';
    else if(c=='@') tmp[i]='a';
    else if(c=='$') tmp[i]='s';
    else if(c=='3') tmp[i]='e';
  }
  /* collapse (.)\1{2,} -> two */
  int p=0;
  for(int i=0;tmp[i]&&p<cap-1;i++){
    if(p>=2 && tmp[i]==out[p-1] && tmp[i]==out[p-2]) continue;
    out[p++]=tmp[i];
  }
  out[p]=0;
}

/* Match one alternative-free atom sequence of `pat` against `t` starting at ti.
 * Returns 1 and sets *end to the text index after the match, or 0.
 * Supports: \b, literal, [class], (?:a|b), trailing ? on any atom. */
static int match_here(const char *pat, int pi, int pend, const char *t, int tlen, int ti, int *end);

/* Parse one atom starting at pi; returns atom end (exclusive) in *ai. Kind:
 * fills matcher via match_atom. */
static int atom_end(const char *pat, int pi, int pend){
  char c=pat[pi];
  if(c=='\\') return pi+2;
  if(c=='['){ int j=pi+1; while(j<pend && pat[j]!=']') j++; return j+1; }
  if(c=='('){ int depth=0,j=pi; for(;j<pend;j++){ if(pat[j]=='(') depth++; else if(pat[j]==')'){ depth--; if(depth==0) return j+1; } } return pend; }
  return pi+1;
}

/* Try to match a single atom [pi,ae) against text at ti; return matched length
 * (>=0) or -1 on fail. Zero-width atoms (\b) return 0 on success. */
static int match_atom_once(const char *pat, int pi, int ae, const char *t, int tlen, int ti){
  (void)ae;
  char c=pat[pi];
  if(c=='\\'){
    char e=pat[pi+1];
    if(e=='b'){ int prev=(ti>0)?is_word_ch((unsigned char)t[ti-1]):0; int cur=(ti<tlen)?is_word_ch((unsigned char)t[ti]):0; return (prev!=cur)?0:-1; }
    /* escaped literal */
    if(ti<tlen && lc((unsigned char)t[ti])==lc((unsigned char)e)) return 1;
    return -1;
  }
  if(c=='['){
    if(ti>=tlen) return -1;
    int j=pi+1; int neg=0; if(pat[j]=='^'){ neg=1; j++; }
    int hit=0; char tc=(char)lc((unsigned char)t[ti]);
    while(pat[j]!=']'){
      if(pat[j]=='\\'){ if(lc((unsigned char)pat[j+1])==tc) hit=1; j+=2; continue; }
      if(pat[j+1]=='-'&&pat[j+2]!=']'){ char a=(char)lc((unsigned char)pat[j]),b=(char)lc((unsigned char)pat[j+2]); if(tc>=a&&tc<=b) hit=1; j+=3; continue; }
      if(lc((unsigned char)pat[j])==tc) hit=1;
      j++;
    }
    if(neg) hit=!hit;
    return hit?1:-1;
  }
  if(c=='('){ return -2; /* group: handled in match_here */ }
  /* literal */
  if(ti<tlen && lc((unsigned char)t[ti])==lc((unsigned char)c)) return 1;
  return -1;
}

/* Match the group (?:...) body's alternatives; try each, recursing into the
 * rest of the pattern after the group. */
static int match_group(const char *pat, int gi, int ge, const char *rest_pat, int rest_pi, int pend, const char *t, int tlen, int ti, int *end){
  /* body is pat[gi+3 .. ge-1) split by top-level | */
  int bstart=gi+3, bend=ge-1;
  int seg=bstart, depth=0;
  for(int j=bstart;j<=bend;j++){
    if(j<bend && pat[j]=='(') depth++;
    else if(j<bend && pat[j]==')') depth--;
    if(j==bend || (depth==0 && pat[j]=='|')){
      /* alternative [seg,j) : build a temp match: match alt then rest */
      /* We match alt against t at ti, then rest_pat from rest_pi */
      int altpi=seg, altpend=j;
      /* recursively: match alt fully, for each way, try rest. Since our matcher
         is greedy-deterministic enough, do: match_here on a concatenation. We
         emulate by matching alt then rest via a two-stage recursive call. */
      /* stage 1: find all end positions of alt — but our match_here returns one
         (greedy). Good enough for these patterns (no ambiguity across | + rest). */
      int mid;
      if(match_here(pat, altpi, altpend, t, tlen, ti, &mid)){
        int fin;
        if(match_here(rest_pat, rest_pi, pend, t, tlen, mid, &fin)){ *end=fin; return 1; }
      }
      seg=j+1;
    }
  }
  return 0;
}

static int match_here(const char *pat, int pi, int pend, const char *t, int tlen, int ti, int *end){
  if(pi>=pend){ *end=ti; return 1; }
  int ae=atom_end(pat, pi, pend);
  int optional = (ae<pend && pat[ae]=='?');
  int after = optional ? ae+1 : ae;
  char c=pat[pi];
  if(c=='('){
    /* try group present */
    if(match_group(pat, pi, ae, pat, after, pend, t, tlen, ti, end)) return 1;
    if(optional){ if(match_here(pat, after, pend, t, tlen, ti, end)) return 1; }
    return 0;
  }
  int r=match_atom_once(pat, pi, ae, t, tlen, ti);
  if(r>=0){ if(match_here(pat, after, pend, t, tlen, ti+r, end)) return 1; }
  if(optional){ if(match_here(pat, after, pend, t, tlen, ti, end)) return 1; }
  return 0;
}

static int pattern_search(const char *pat, const char *t){
  int pend=(int)strlen(pat), tlen=(int)strlen(t);
  for(int ti=0; ti<=tlen; ti++){ int end; if(match_here(pat,0,pend,t,tlen,ti,&end)) return 1; }
  return 0;
}

int herder_find_banned(const char *text){
  char norm[4096]; herder_normalise_for_ban(norm, sizeof(norm), text);
  for(int p=0; PATTERNS[p]; p++) if(pattern_search(PATTERNS[p], norm)) return 1;
  return 0;
}
