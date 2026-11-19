#include "wr.h"
#include <stdio.h>
int main(void){
  wr_seed(6); audio_init(); game_init();
  // Find a garage, stand under its deck at five stars, and see whether the
  // heat can come down at all.
  int found=-1;
  for(int a=0;a<400 && found<0;a++){
    g_p.x=(int32_t)(wr_rand_n(60)+30)*TILE; g_p.y=(int32_t)(wr_rand_n(60)+30)*TILE;
    if(cell_at_point(g_p.x,g_p.y)==CELL_WATER) continue;
    city_refresh(g_p.x,g_p.y);
    for(int li=0;li<g_nlot;li++)
      if(g_lot[li].kind==LOT_GARAGE){ found=li; break; }
  }
  if(found<0){ printf("no garage found\n"); return 1; }
  Lot*L=&g_lot[found];
  g_p.x=(L->x0+L->x1)/2; g_p.y=(L->y0+L->y1)/2;
  city_refresh(g_p.x,g_p.y);
  printf("parked in a garage at (%ld,%ld)\n",(long)(g_p.x/TILE),(long)(g_p.y/TILE));
  g_p.wanted=5; g_p.heat=479; g_p.evade=0;
  int cleared=-1;
  for(int k=0;k<4000;k++){
    g_p.hp=100; g_p.arrest=0;
    // stay put under the deck
    int32_t hx=g_p.x, hy=g_p.y;
    game_step();
    g_p.x=hx; g_p.y=hy;
    if((k%500)==0)
      printf("  t=%5.1f stars=%d heat=%3d evade=%4d\n",k/25.0,g_p.wanted,g_p.heat,g_p.evade);
    if(!g_p.wanted){ cleared=k; break; }
  }
  if(cleared>=0) printf("CLEAN after %.1fs hiding in the garage\n",cleared/25.0);
  else printf("still wanted after 160s under cover\n");
  return cleared>=0?0:1;
}
