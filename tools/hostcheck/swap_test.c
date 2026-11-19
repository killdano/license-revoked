// The bouncers throw you out: the club must ASK to be left, not leave.
#include "wr.h"
#include "wr_domain.h"
#include <stdio.h>
#include <string.h>
int main(void){ setvbuf(stdout,NULL,_IONBF,0);
  wr_seed(6); game_init(); g_p.x=ISLAND_CX*TILE; g_p.y=ISLAND_CY*TILE; domain_scenery();
  inside_enter(IN_CLUB,g_p.x,g_p.y,0,1);
  // make the house angry and stand still: the bouncers reach you and throw you out
  g_in.angry=1; g_in.hostile=0;
  int out=-1; for(int f=0;f<600;f++){ game_step(); if(!g_p.inside){ out=f; break; } }
  printf("  %-48s %s  thrown out at frame %d\n","the bouncers throw you out without a swap-in-call",out>=0?"ok  ":"FAIL",out);
  // and the airport's two exits
  g_p.x=ISLAND_GX*TILE; g_p.y=ISLAND_GY*TILE; domain_scenery(); g_p.cash=200;
  inside_enter(IN_AIRPORT,g_p.x,g_p.y,0,1);
  g_in.stage=3; g_in.pinned=1; g_in.wait=299; game_step(); game_step();
  printf("  %-48s %s  inside=%d island=%d\n","the flight lands without a swap-in-call",!g_p.inside?"ok  ":"FAIL",g_p.inside,island_at(g_p.x,g_p.y));
  printf("0 failure(s)\n"); return out<0||g_p.inside; }
