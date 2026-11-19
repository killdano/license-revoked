#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-46s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
const uint8_t *audio_peek_buf(int *len);
int main(void){
  char b[170];
  setvbuf(stdout,NULL,_IONBF,0);
  wr_seed(6); audio_init(); game_init();
  g_menu.sfx_on=1; audio_set_enabled(true); audio_set_volume(100);

  // Fire everything at once - the worst case the mixer ever sees - and count
  // how many samples sit at the rails. Hard clipping is what crackles.
  int clipped=0, total=0, peak=0;
  for(int k=0;k<120;k++){
    if((k%7)==0){ audio_sfx(SFX_SHOTGUN); audio_sfx(SFX_PISTOL); }
    if((k%23)==0) audio_sfx(SFX_EXPLODE);
    if((k%5)==0)  audio_sfx(SFX_SMASH);
    audio_tick();
    int len=0; const uint8_t*buf=audio_peek_buf(&len);
    for(int i=0;i<len;i++){
      int v=(int8_t)buf[i];
      if(v<0)v=-v;
      if(v>peak)peak=v;
      if(v>=119) clipped++;
      total++;
    }
  }
  snprintf(b,sizeof b,"%d of %d samples at the rails (peak %d of 127)",clipped,total,peak);
  ck("the mixer does not clip under heavy fire", total>0 && clipped*200 < total, b);

  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
