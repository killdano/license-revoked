#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-44s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
int main(void){
  char b[160];
  wr_seed(6); audio_init(); game_init();

  // Walk into the sea from the beach.
  int32_t sx=0,sy=0; uint16_t seaward=0;
  for(int a=0;a<400 && !sx;a++){
    int32_t x=(int32_t)(wr_rand_n(70)+20)*TILE, y=(int32_t)(wr_rand_n(70)+20)*TILE;
    if(cell_at_point(x,y)!=CELL_SAND) continue;
    for(int t=0;t<16;t++){
      uint16_t ang=(uint16_t)(t*4096);
      int32_t px=x+((fcos(ang)*3*TILE)>>10), py=y+((fsin(ang)*3*TILE)>>10);
      if(cell_at_point(px,py)==CELL_WATER){ sx=x; sy=y; seaward=ang; break; }
    }
  }
  if(!sx){ printf("no beach found\n"); return 1; }
  g_p.x=sx; g_p.y=sy; g_p.ang=seaward; g_p.in_car=0;
  int32_t startd=0;
  for(int k=0;k<300;k++){ g_p.stick_f=256; game_step(); }
  startd=wr_dist(g_p.x-sx,g_p.y-sy);
  snprintf(b,sizeof b,"travelled %ld units, cell=%d, swim=%d",
     (long)startd,cell_at_point(g_p.x,g_p.y),g_p.swim);
  ck("you can walk into the sea and swim",
     cell_at_point(g_p.x,g_p.y)==CELL_WATER && g_p.swim==1 && startd>2*TILE, b);

  // Slower than walking.
  int32_t swim_spd=wr_abs(g_p.spd);
  snprintf(b,sizeof b,"swim speed %ld vs walk 9",(long)swim_spd);
  ck("swimming is slower than walking", swim_spd>0 && swim_spd<=5, b);

  // No shooting mid-stroke.
  g_p.weapon=W_PISTOL; g_p.ammo=10; g_p.attack_cd=0;
  for(int i=0;i<MAX_BULLET;i++) g_bullet[i].life=0;
  game_input_action();
  int fired=0; for(int i=0;i<MAX_BULLET;i++) if(g_bullet[i].life) fired=1;
  snprintf(b,sizeof b,"fired=%d ammo=%d",fired,g_p.ammo);
  ck("you cannot shoot while swimming", !fired, b);

  // Swim out to a boat and take it.
  wr_seed(6); game_init();
  // Moor a boat in open water near the beach we found, rather than waiting
  // for the marina spawner to put one inside the spawn radius.
  int bi=-1;
  {
    int32_t wx=0,wy=0;
    for(int t=0;t<64 && !wx;t++){
      int32_t cx=sx+((fcos(seaward)*(2*TILE+t*40))>>10);
      int32_t cy=sy+((fsin(seaward)*(2*TILE+t*40))>>10);
      if(cell_at_point(cx,cy)==CELL_WATER){ wx=cx; wy=cy; }
    }
    for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_NONE){ bi=i; break; }
    if(bi>=0 && wx){
      Ent*e=&g_ent[bi]; memset(e,0,sizeof(Ent));
      e->type=ENT_BOAT; e->sub=BOAT_MOTOR; e->hp=100; e->state=ST_IDLE;
      e->x=wx; e->y=wy;
    } else bi=-1;
  }
  if(bi>=0){
    Ent*bt=&g_ent[bi];
    // Drop the player in the water a little way off the hull.
    int32_t px=bt->x, py=bt->y; int placed=0;
    for(int t=0;t<32 && !placed;t++){
      uint16_t a2=(uint16_t)(t*2048);
      int32_t cx=bt->x+((fcos(a2)*260)>>10), cy=bt->y+((fsin(a2)*260)>>10);
      if(cell_at_point(cx,cy)==CELL_WATER){ px=cx; py=cy; placed=1; }
    }
    g_p.x=px; g_p.y=py; g_p.in_car=0;
    for(int k=0;k<4;k++) game_step();
    int swam=g_p.swim;
    game_input_action();
    snprintf(b,sizeof b,"swimming=%d, boarded=%d",swam,g_p.in_car?1:0);
    ck("you can board a boat from the water", swam && g_p.in_car, b);
  }
  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
