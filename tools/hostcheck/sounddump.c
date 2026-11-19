// Render every sound effect and each continuous generator to a WAV file,
// so the design can be heard on a PC before it is flashed.
// Usage: sounddump <outdir>
#include "wr.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
extern signed char g_pcm[4096]; extern unsigned g_pcm_n; extern unsigned long g_pcm_total;
const int8_t *audio_peek_buf(int *len);
static void wav_header(FILE *f, unsigned long nsamp){
  unsigned long data=nsamp, rate=8000;
  unsigned char h[44]={'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,1,0,8,0,'d','a','t','a',0,0,0,0};
  unsigned long cs=36+data; h[4]=cs;h[5]=cs>>8;h[6]=cs>>16;h[7]=cs>>24;
  h[24]=rate;h[25]=rate>>8;h[26]=rate>>16;h[27]=rate>>24; h[28]=rate;h[29]=rate>>8;h[30]=rate>>16;h[31]=rate>>24;
  h[40]=data;h[41]=data>>8;h[42]=data>>16;h[43]=data>>24;
  fseek(f,0,SEEK_SET); fwrite(h,1,44,f);
}
static FILE *wav_open(const char *path, unsigned long nsamp){ FILE *f=fopen(path,"wb"); if(f) wav_header(f,nsamp); return f; }
static void wav_close(FILE *f){ long n=ftell(f)-44; wav_header(f,(unsigned long)n); fclose(f); }
static void tick_to(FILE *f, int frames){
  for(int i=0;i<frames;i++){ audio_tick(); int len; const int8_t *b=audio_peek_buf(&len);
    for(int k=0;k<len;k++){ unsigned char u=(unsigned char)(b[k]+128); fputc(u,f);} }
}
static const char *NAME[SFX_COUNT]={"punch","bat","pistol","shotgun","scream_m","scream_w","car_start","crash","pickup","busted","wanted_up","escape","die_m","die_w","explode","door","smash","arrest","footstep","flame","rocket","glass","runover"};
int main(int argc,char**argv){
  const char *dir=argc>1?argv[1]:"/tmp/sounds"; char path[256];
  wr_seed(6); audio_init(); game_init(); audio_set_volume(90);
  memset(g_ent,0,sizeof g_ent); g_p.in_car=0; g_p.wanted=0;
  for(int s=0;s<SFX_COUNT;s++){
    snprintf(path,sizeof path,"%s/%02d_%s.wav",dir,s,NAME[s]);
    FILE *f=wav_open(path,0); if(!f){printf("cannot write %s\n",path);return 1;}
    for(int q=0;q<5;q++) audio_tick();                // settle
    if(s==SFX_FLAME){ for(int fr=0;fr<40;fr++){ audio_sfx(SFX_FLAME); tick_to(f,1);} tick_to(f,20); }
    else { audio_sfx((uint8_t)s); tick_to(f,45); }
    wav_close(f);
    for(int q=0;q<60;q++) audio_tick();
  }
  // engine: idle 1s, accelerate over 2s, cruise 1s, brake 1s
  snprintf(path,sizeof path,"%s/30_engine.wav",dir); FILE *f=wav_open(path,0);
  { Ent *c=&g_ent[1]; c->type=ENT_CAR; c->state=ST_DRIVE; c->hp=100; c->x=g_p.x; c->y=g_p.y; g_p.in_car=2;
    g_p.spd=0; tick_to(f,25); for(int fr=0;fr<50;fr++){ g_p.spd=(int16_t)(fr*30/50); tick_to(f,1);} g_p.spd=30; tick_to(f,25);
    for(int fr=0;fr<25;fr++){ g_p.spd=(int16_t)(30-fr*30/25); tick_to(f,1);} g_p.spd=0; tick_to(f,15); g_p.in_car=0; tick_to(f,10);
    wav_close(f); }
  // siren: patrol car approaching from 10 tiles to 1 over 5 s, then away
  snprintf(path,sizeof path,"%s/31_siren.wav",dir); f=wav_open(path,0);
  { Ent *c=&g_ent[2]; c->type=ENT_COPCAR; c->state=ST_CHASE; c->hp=100; c->y=g_p.y; g_p.wanted=2;
    for(int fr=0;fr<125;fr++){ c->x=g_p.x+(10*TILE)-(fr*9*TILE)/125; tick_to(f,1);} for(int fr=0;fr<75;fr++){ c->x=g_p.x+TILE+(fr*9*TILE)/75; tick_to(f,1);}
    c->type=ENT_NONE; g_p.wanted=0; tick_to(f,15);
    wav_close(f); }
  // rotor: helicopter arriving overhead and leaving
  snprintf(path,sizeof path,"%s/32_helicopter.wav",dir); f=wav_open(path,0);
  { Ent *h=&g_ent[3]; h->type=ENT_HELI; h->state=ST_CHASE; h->hp=200; h->z=HELI_ALT; h->y=g_p.y;
    for(int fr=0;fr<100;fr++){ h->x=g_p.x+(12*TILE)-(fr*11*TILE)/100; tick_to(f,1);} tick_to(f,50); for(int fr=0;fr<75;fr++){ h->x=g_p.x+TILE+(fr*11*TILE)/75; tick_to(f,1);}
    h->type=ENT_NONE; tick_to(f,15);
    wav_close(f); }
  // a firefight: pistol, shotgun, scream, crash, explosion in sequence
  snprintf(path,sizeof path,"%s/33_firefight.wav",dir); f=wav_open(path,0);
  { audio_sfx(SFX_PISTOL); tick_to(f,8); audio_sfx(SFX_PISTOL); tick_to(f,8); audio_sfx(SFX_PISTOL); tick_to(f,14);
    audio_sfx(SFX_SCREAM_M); tick_to(f,12); audio_sfx(SFX_SHOTGUN); tick_to(f,18); audio_sfx(SFX_DIE_W); tick_to(f,20);
    audio_sfx(SFX_CRASH); tick_to(f,20); audio_sfx(SFX_EXPLODE); tick_to(f,50);
    wav_close(f); }
  // the club, from the door
  snprintf(path,sizeof path,"%s/34_club_music.wav",dir); f=wav_open(path,0);
  { inside_enter(IN_CLUB, g_p.x, g_p.y, 0, 1); tick_to(f,25*24); inside_exit(); wav_close(f); }
  printf("wrote %s\n",dir);
  return 0;
}
