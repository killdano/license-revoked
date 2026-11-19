#include "wr.h"
#include <stdio.h>
int main(void){
  wr_seed(6); audio_init(); game_init();
  int32_t ptx=g_p.x>>TILE_SH, pty=g_p.y>>TILE_SH;
  printf("player tile %d,%d   DIAG_EVERY=%d ROAD_HALF=%d\n\n",
     (int)ptx,(int)pty,DIAG_EVERY,ROAD_HALF);
  const char *sym=".pB#s~";   // road, pave, bld, park, sand, water
  for(int32_t ty=pty-16;ty<=pty+16;ty++){
    for(int32_t tx=ptx-24;tx<=ptx+24;tx++){
      uint8_t c=cell_at(tx,ty);
      putchar(c<6?sym[c]:'?');
    }
    putchar('\n');
  }
  int counts[6]={0};
  for(int32_t ty=pty-20;ty<=pty+20;ty++) for(int32_t tx=ptx-20;tx<=ptx+20;tx++)
    counts[cell_at(tx,ty)%6]++;
  printf("\nlocal mix: road=%d pave=%d bld=%d park=%d sand=%d water=%d\n",
     counts[0],counts[1],counts[2],counts[3],counts[4],counts[5]);
  int pk=0; for(int i=0;i<MAX_PICKUP;i++) pk+=g_pickup[i].alive;
  int solid=0,total=0;
  for(int i=0;i<MAX_PROP;i++) if(g_prop[i].alive){total++; if(prop_solid(g_prop[i].kind)) solid++;}
  printf("pickups visible=%d  props=%d (of which solid obstacles=%d)\n",pk,total,solid);
  return 0;
}
