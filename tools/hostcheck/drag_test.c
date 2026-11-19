#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-46s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
int main(void){
  char b[160];
  wr_seed(6); audio_init(); game_init();
  int32_t tx=g_p.x, ty=g_p.y; uint16_t ta=0; road_snap(&tx,&ty,&ta);
  for(int i=0;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;

  // A body at rest on the road, and a car driven over it.
  Ent*body=&g_ent[0]; memset(body,0,sizeof(Ent));
  body->type=ENT_PED; body->sub=PED_MAN; body->state=ST_DEAD; body->hp=0;
  body->x=tx; body->y=ty;
  int32_t bx0=body->x, by0=body->y;

  Ent*car=&g_ent[1]; memset(car,0,sizeof(Ent));
  car->type=ENT_CAR; car->sub=CAR_SEDAN; car->hp=100; car->state=ST_DRIVE;
  car->x=tx-((fcos(ta)*700)>>10); car->y=ty-((fsin(ta)*700)>>10);
  car->ang=ta; car->spd=22; car->timer=1;

  int32_t moved=0;
  for(int k=0;k<160;k++){
    game_step();
    for(int i=2;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
    if(body->type!=ENT_PED) break;
    int32_t d=wr_dist(body->x-bx0,body->y-by0);
    if(d>moved) moved=d;
  }
  snprintf(b,sizeof b,"body dragged %ld units down the street",(long)moved);
  ck("a car drags a body rather than passing through", moved>120, b);

  // A parked car must leave it alone.
  wr_seed(6); game_init();
  for(int i=0;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
  Ent*b2=&g_ent[0]; memset(b2,0,sizeof(Ent));
  b2->type=ENT_PED; b2->state=ST_DEAD; b2->x=tx; b2->y=ty;
  Ent*c2=&g_ent[1]; memset(c2,0,sizeof(Ent));
  c2->type=ENT_CAR; c2->hp=100; c2->state=ST_IDLE; c2->x=tx+40; c2->y=ty; c2->spd=0;
  int32_t sx=b2->x, sy=b2->y;
  for(int k=0;k<80;k++){ game_step();
    for(int i=2;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
    c2->spd=0; }
  snprintf(b,sizeof b,"body shifted %ld units beside a parked car",
    (long)wr_dist(b2->x-sx,b2->y-sy));
  ck("a stationary car leaves bodies alone", wr_dist(b2->x-sx,b2->y-sy)<20, b);

  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
