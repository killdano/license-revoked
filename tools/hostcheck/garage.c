// The garage trick: with heat on, take a different car under a parking
// deck and the police lose you.
#include "wr.h"
#include <stdio.h>
#include <string.h>
int main(void){
  wr_seed(6); audio_init(); game_init();
  // find a garage lot near the player by walking the world
  int found=-1; Lot L;
  for(int t=0;t<400 && found<0;t++){
    g_p.x=(int32_t)(wr_rand_n(60)+15)*TILE; g_p.y=(int32_t)(wr_rand_n(60)+15)*TILE; city_refresh(g_p.x,g_p.y);
    for(int i=0;i<g_nlot;i++) if(g_lot[i].kind==LOT_GARAGE){ found=i; L=g_lot[i]; break; }
  }
  if(found<0){ printf("no garage found\n"); return 1; }
  int32_t cx=(L.x0+L.x1)/2, cy=(L.y0+L.y1)/2;
  g_p.x=cx-60; g_p.y=cy; city_refresh(g_p.x,g_p.y);
  memset(g_ent,0,sizeof g_ent);
  Ent*c=&g_ent[2]; c->type=ENT_CAR; c->state=ST_PARK; c->hp=100; c->sub=CAR_SEDAN; c->x=cx+40; c->y=cy;
  g_p.wanted=3; g_p.heat=250; g_p.evade=0; g_p.in_car=0;
  int cap=0; for(int f=0;f<70;f++){ game_step(); if(g_p.caption==CAP_GARAGE && g_p.caption_t) cap=1; }
  printf("  %-40s %s   under cover=%d caption shown=%d\n","garage hint appears", cap?"ok  ":"FAIL", player_under_cover(), cap);
  game_input_action();
  bool ok = g_p.in_car && g_p.wanted==0 && g_p.heat==0 && g_p.caption==CAP_LOST;
  printf("  %-40s %s   in_car=%d wanted=%d heat=%d caption=%d\n","swapping cars in a garage clears the heat", ok?"ok  ":"FAIL", g_p.in_car, g_p.wanted, g_p.heat, g_p.caption);
  return (cap && ok) ? 0 : 1;
}
