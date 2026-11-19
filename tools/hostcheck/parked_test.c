#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-46s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
int main(void){
  char b[160];
  wr_seed(6); audio_init(); game_init();
  // Find a garage, stand in it, and see whether real cars appear in the bays.
  int found=-1;
  for(int a=0;a<400 && found<0;a++){
    g_p.x=(int32_t)(wr_rand_n(60)+30)*TILE; g_p.y=(int32_t)(wr_rand_n(60)+30)*TILE;
    if(cell_at_point(g_p.x,g_p.y)==CELL_WATER) continue;
    city_refresh(g_p.x,g_p.y);
    for(int li=0;li<g_nlot;li++) if(g_lot[li].kind==LOT_GARAGE){ found=li; break; }
  }
  if(found<0){ printf("no garage found\n"); return 1; }
  g_p.x=(g_lot[found].x0+g_lot[found].x1)/2;
  g_p.y=(g_lot[found].y0+g_lot[found].y1)/2;
  city_refresh(g_p.x,g_p.y);
  for(int k=0;k<200;k++) game_step();

  int parked=0, moving=0;
  for(int i=0;i<MAX_ENT;i++){
    if(g_ent[i].type!=ENT_CAR) continue;
    if(g_ent[i].state==ST_PARK) parked++; else moving++;
  }
  snprintf(b,sizeof b,"%d parked, %d moving cars alive",parked,moving);
  ck("real cars appear in the garage bays", parked>0, b);
  ck("parked cars do not starve the traffic", moving>0, b);

  // A parked car must stay parked...
  int pi=-1; for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_CAR&&g_ent[i].state==ST_PARK){pi=i;break;}
  int32_t px=g_ent[pi].x, py=g_ent[pi].y;
  for(int k=0;k<300;k++) game_step();
  snprintf(b,sizeof b,"drifted %ld units over 300 frames",
    (long)wr_dist(g_ent[pi].x-px,g_ent[pi].y-py));
  ck("a parked car stays where it is parked",
     g_ent[pi].type==ENT_CAR && wr_dist(g_ent[pi].x-px,g_ent[pi].y-py)<8, b);

  // ...and it must be stealable.
  g_p.x=g_ent[pi].x+40; g_p.y=g_ent[pi].y; g_p.in_car=0;
  game_input_action();
  snprintf(b,sizeof b,"in_car=%d (slot %d)",g_p.in_car,pi+1);
  ck("you can steal a parked car", g_p.in_car==pi+1, b);

  // And the emptied bay must not be an invisible wall.
  g_p.in_car=0; g_ent[pi].type=ENT_NONE;
  snprintf(b,sizeof b,"blocked=%d at the empty bay",city_blocked(px,py,true));
  ck("an empty bay is not an invisible wall", !city_blocked(px,py,true), b);

  // Seafront bays: cars nosed in at the sand line, with a meter beside each.
  wr_seed(6); game_init();
  // (seafront bays and meters were retired to make room for the club)

  // A car you get out of must stay where you left it.
  wr_seed(6); game_init();
  for(int i=0;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
  { int32_t tx=g_p.x, ty=g_p.y; uint16_t ta=0; road_snap(&tx,&ty,&ta);
    Ent*c=&g_ent[0]; memset(c,0,sizeof(Ent));
    c->type=ENT_CAR; c->sub=CAR_SEDAN; c->hp=100; c->state=ST_IDLE;
    c->x=tx+40; c->y=ty; c->ang=ta;
    g_p.x=tx; g_p.y=ty; g_p.in_car=0;
    game_input_action();                       // get in
    int got_in = g_p.in_car ? 1 : 0;
    for(int k=0;k<40;k++){ g_p.stick_f=256; game_step(); }
    game_input_action();                       // and out again
    int32_t px=c->x, py=c->y;
    for(int k=0;k<200;k++){ game_step();
      for(int i=1;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE; }
    snprintf(b,sizeof b,"got in=%d, drifted %ld units after stepping out",
             got_in,(long)wr_dist(c->x-px,c->y-py));
    // Under a third of a tile. The anchor is stored at 1/64 resolution, so a
    // parked car settles within about 96 subunits of where it was left - the
    // point is that it does not DRIVE OFF, which is what it used to do.
    ck("a car you step out of stays put",
       got_in && !g_p.in_car && wr_dist(c->x-px,c->y-py) < 110, b);

    // ...and works again when you get back in.
    g_p.x=c->x+40; g_p.y=c->y;
    game_input_action();
    snprintf(b,sizeof b,"back in car=%d, state=%d",g_p.in_car?1:0,c->state);
    ck("and it drives again when you take it back", g_p.in_car!=0, b);
  }

  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
