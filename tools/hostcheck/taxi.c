// Crazy taxi: a fare waves, gets in, pays at the marker.
#include "wr.h"
#include <stdio.h>
#include <string.h>
int main(void){
  char b[160]; int fails=0;
  wr_seed(6); audio_init(); game_init();
  int32_t x=g_p.x,y=g_p.y; uint16_t a; road_snap(&x,&y,&a); g_p.x=x; g_p.y=y; g_p.ang=a;
  Ent*c=&g_ent[0]; memset(c,0,sizeof(Ent)); c->type=ENT_CAR; c->hp=100; c->state=ST_PARK; c->sub=CAR_TAXI; c->x=g_p.x+40; c->y=g_p.y; c->ang=a;
  game_input_action();
  for(int f=0;f<200 && !g_taxi.fare;f++) game_step();
  snprintf(b,sizeof b,"in_car=%d fare=%d",g_p.in_car,g_taxi.fare);
  printf("  %-40s %s   %s\n","a fare waves down the cab", (g_p.in_car && g_taxi.fare)?"ok  ":"FAIL", b); if(!(g_p.in_car&&g_taxi.fare)) fails++;
  Ent*f=&g_ent[g_taxi.fare-1]; g_p.x=f->x+60; g_p.y=f->y; g_p.spd=0; c->x=g_p.x; c->y=g_p.y;
  for(int k=0;k<5;k++) game_step();
  snprintf(b,sizeof b,"aboard=%d dest %d,%d (%d tiles away)",g_taxi.aboard,(int)g_taxi.dx,(int)g_taxi.dy,(int)(wr_dist(g_taxi.dx-g_p.x,g_taxi.dy-g_p.y)/TILE));
  printf("  %-40s %s   %s\n","stop beside them and they get in", g_taxi.aboard?"ok  ":"FAIL", b); if(!g_taxi.aboard) fails++;
  uint16_t cash=g_p.cash; g_p.x=g_taxi.dx; g_p.y=g_taxi.dy; c->x=g_p.x; c->y=g_p.y; g_p.spd=0;
  for(int k=0;k<5;k++) game_step();
  snprintf(b,sizeof b,"cash %d -> %d, caption=%d",cash,g_p.cash,g_p.caption);
  printf("  %-40s %s   %s\n","the marker pays", (g_p.cash>cash && !g_taxi.aboard)?"ok  ":"FAIL", b); if(!(g_p.cash>cash)) fails++;
  printf("\n%d failure(s)\n",fails); return fails?1:0;
}
