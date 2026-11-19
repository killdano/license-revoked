#include "wr.h"
#include <stdio.h>
extern signed char g_pcm[4096]; extern unsigned g_pcm_n; extern unsigned long g_pcm_total;
static int fails=0;
static void ck(const char*n,int ok,const char*d){ printf("  %-34s %s   %s\n",n,ok?"ok  ":"FAIL",d); if(!ok)fails++; }
int main(void){
  char b[90];
  wr_seed(6); audio_init(); game_init();
  g_pcm_n=0;
  for(int f=0;f<40;f++) audio_tick();
  int nz=0; for(unsigned i=0;i<g_pcm_n;i++) if(g_pcm[i]) nz++;
  snprintf(b,sizeof b,"%lu samples, %d non-silent at rest",g_pcm_total,nz);
  ck("stream is fed every frame", g_pcm_total>=40*320, b);

  // fire every weapon and make sure the mixer produces real signal in range
  int worst=0; long energy=0; unsigned n0=0;
  for(int w=0;w<W_COUNT;w++){
    g_p.weapon=(uint8_t)w; g_p.ammo=90; g_p.attack_cd=0;
    game_input_action();
    g_pcm_n=0; n0=0;
    for(int f=0;f<12;f++) audio_tick();
    for(unsigned i=0;i<g_pcm_n;i++){
      int v=g_pcm[i]; if(v<0)v=-v; if(v>worst)worst=v; energy+=v; n0++;
    }
  }
  snprintf(b,sizeof b,"peak |sample| = %d (limit 127), mean %ld",worst,n0?energy/n0:0);
  ck("mixer never clips out of range", worst<=127 && worst>20, b);

  // voices must actually finish, or the mixer silts up and nothing new is heard
  for(int f=0;f<400;f++) audio_tick();
  g_pcm_n=0;
  for(int f=0;f<20;f++) audio_tick();
  long tail=0; for(unsigned i=0;i<g_pcm_n;i++){ int v=g_pcm[i]; tail+=v<0?-v:v; }
  snprintf(b,sizeof b,"residual level after 16s of silence: %ld",g_pcm_n?tail/g_pcm_n:0);
  ck("voices decay and free up", (g_pcm_n? tail/(long)g_pcm_n : 0) < 12, b);

  // muting really mutes
  audio_set_enabled(false);
  g_pcm_n=0;
  g_p.weapon=W_SHOTGUN; g_p.ammo=9; g_p.attack_cd=0; game_input_action();
  for(int f=0;f<10;f++) audio_tick();
  long m=0; for(unsigned i=0;i<g_pcm_n;i++){ int v=g_pcm[i]; m+=v<0?-v:v; }
  snprintf(b,sizeof b,"total energy while off: %ld",m);
  ck("sound toggle silences the mixer", m==0, b);
  audio_set_enabled(true);

  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
