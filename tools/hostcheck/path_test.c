#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-46s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
int main(void){
  char b[160];
  wr_seed(6); audio_init(); game_init();
  // Put the player on one side of a building and a cop on the far side, so
  // the direct line is straight through a wall.
  int trials=0, reached=0; int32_t worst=0;
  for(int a=0;a<2000 && trials<12;a++){   // pavement beside a building is a rare draw
    g_p.x=(int32_t)(wr_rand_n(60)+30)*TILE; g_p.y=(int32_t)(wr_rand_n(60)+30)*TILE;
    if(cell_at_point(g_p.x,g_p.y)!=CELL_PAVE) continue;
    city_refresh(g_p.x,g_p.y);
    int li=-1;
    for(int i=0;i<g_nlot;i++){
      int32_t cx=(g_lot[i].x0+g_lot[i].x1)/2, cy=(g_lot[i].y0+g_lot[i].y1)/2;
      if(g_lot[i].kind==LOT_GARAGE) continue;
      if(wr_dist(cx-g_p.x,cy-g_p.y)<3*TILE){ li=i; break; }
    }
    if(li<0) continue;
    // Cop directly opposite the player across that building.
    int32_t cx=(g_lot[li].x0+g_lot[li].x1)/2, cy=(g_lot[li].y0+g_lot[li].y1)/2;
    int32_t ox=cx-(g_p.x-cx), oy=cy-(g_p.y-cy);
    if(city_blocked(ox,oy,false)) continue;
    for(int i=0;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
    Ent*e=&g_ent[0]; memset(e,0,sizeof(Ent));
    e->type=ENT_COP; e->hp=100; e->state=ST_CHASE; e->spd=7;
    e->x=ox; e->y=oy; e->wep=W_BAT;
    g_p.wanted=1; g_p.heat=100;
    trials++;
    int32_t start=wr_dist(e->x-g_p.x,e->y-g_p.y), best=start;
    for(int k=0;k<600;k++){
      int32_t hx=g_p.x, hy=g_p.y;
      g_p.hp=100; g_p.arrest=0;
      game_step();
      g_p.x=hx; g_p.y=hy;              // player stands still
      for(int i=1;i<MAX_ENT;i++) if(g_ent[i].type==ENT_COP) g_ent[i].type=ENT_NONE;
      if(e->type!=ENT_COP) break;
      int32_t d=wr_dist(e->x-g_p.x,e->y-g_p.y);
      if(d<best) best=d;
      if(d<140) break;
    }
    if(best<140) reached++;
    if(best>worst) worst=best;
  }
  snprintf(b,sizeof b,"%d cops started behind a building, %d got to the player (worst final gap %ld)",
     trials,reached,(long)worst);
  ck("foot police path around buildings", trials>=6 && reached*4>=trials*3, b);
  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
