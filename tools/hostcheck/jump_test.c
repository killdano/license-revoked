// The collapsed bridge. Fast at the lip clears it; slow goes in the water.
#include "wr.h"
#include "wr_domain.h"
#include <stdio.h>
#include <string.h>
static int fails;
static void ck(const char *w, bool ok, const char *d){ printf("  %-44s %s  %s\n",w,ok?"ok  ":"FAIL",d); if(!ok) fails++; }
static void approach(int32_t spd, int push){
  wr_seed(6); game_init(); memset(g_ent,0,sizeof g_ent);
  // stand on the ghetto bridge deck just before the gap, heading for it
  const Island *A=&ISLANDS[BRIDGES[BR_GHETTO].a], *B=&ISLANDS[BRIDGES[BR_GHETTO].b];
  int32_t ax=A->cx*TILE, ay=A->cy*TILE, dx=(B->cx-A->cx)*TILE, dy=(B->cy-A->cy)*TILE;
  int32_t t=BRIDGES[BR_GHETTO].gap0-(push?40:3);   // a run-up, or right at the lip
  g_p.x=ax+wr_muldiv(dx,t,1024); g_p.y=ay+wr_muldiv(dy,t,1024); g_p.ang=bridge_angle_of(BR_GHETTO);
  Ent*c=&g_ent[0]; c->type=ENT_CAR; c->hp=100; c->state=ST_PARK; c->sub=CAR_SPORTS; c->x=g_p.x; c->y=g_p.y; c->ang=g_p.ang;
  g_p.in_car=1; c->occupied=1; g_p.spd=(int16_t)spd;
  domain_scenery();
  if(push){ game_touch_down(STICK_HOME_X,STICK_HOME_Y); game_touch_move(STICK_HOME_X,(int16_t)(STICK_HOME_Y-STICK_RANGE)); }
}
int main(void){ char b[160];
  int32_t lat,t; int bi=bridge_find(ISLANDS[0].cx*TILE, (ISLANDS[0].cy+30)*TILE, &lat,&t);
  snprintf(b,sizeof b,"bridge=%d t=%d",bi,(int)t);
  ck("the bridge to the ghetto exists", bi==BR_GHETTO, b);
  // fast
  approach(30,1); int peak=0, frames=0, landed=0;
  for(int f=0;f<160;f++){ game_step(); if(g_p.in_car){ int z=g_ent[0].z; if(z>peak)peak=z; } if(g_p.jump) frames++;
    if(!g_p.jump && f>10 && g_p.in_car && cell_at_point(g_p.x,g_p.y)==CELL_ROAD && frames>0){ landed=1; break; } }
  snprintf(b,sizeof b,"peak z=%d, %d frames airborne, in_car=%d, cell=%d",peak,frames,g_p.in_car,cell_at_point(g_p.x,g_p.y));
  ck("at speed the car clears the gap and lands on the far stub", landed && peak>100, b);
  // slow
  approach(12,0); int drowned=0;                  // coasting at the lip, foot off
  for(int f=0;f<200;f++){ game_step(); if(!g_p.in_car && g_ent[0].state==ST_DEAD && cell_at_point(g_ent[0].x,g_ent[0].y)==CELL_WATER){ drowned=1; break; } }
  snprintf(b,sizeof b,"in_car=%d wreck state=%d swim=%d",g_p.in_car,g_ent[0].state,g_p.swim);
  ck("too slow, the car goes in and is a wreck in the water", drowned && g_p.swim, b);
  printf("%d failure(s)\n",fails); return fails?1:0; }
