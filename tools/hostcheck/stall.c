#include "wr.h"
#include <stdio.h>
int main(void){
  wr_seed(6); audio_init(); game_init();
  for(int f=0;f<300;f++) game_step();
  int32_t px[MAX_ENT],py[MAX_ENT]; uint16_t pa[MAX_ENT];
  for(int i=0;i<MAX_ENT;i++){ px[i]=g_ent[i].x; py[i]=g_ent[i].y; pa[i]=g_ent[i].ang; }
  int spins[MAX_ENT]; memset(spins,0,sizeof(spins));
  for(int f=0;f<200;f++){
    uint16_t a0[MAX_ENT];
    for(int i=0;i<MAX_ENT;i++) a0[i]=g_ent[i].ang;
    ent_update();
    for(int i=0;i<MAX_ENT;i++) if(g_ent[i].ang!=a0[i]) spins[i]++;
  }
  for(int i=0;i<MAX_ENT;i++){
    if(g_ent[i].type!=ENT_CAR) continue;
    int32_t d=wr_dist(g_ent[i].x-px[i],g_ent[i].y-py[i]);
    int32_t du=g_ent[i].x+g_ent[i].y, dv=g_ent[i].x-g_ent[i].y;
    printf("  car %2d: moved %5d  turned on %3d/200 frames  surf=%d  spd=%d\n",
      i,(int)d,spins[i],cell_at_point(g_ent[i].x,g_ent[i].y),g_ent[i].spd);
    (void)du;(void)dv;
  }
  return 0;
}
