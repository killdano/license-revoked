#include "wr.h"
#include <stdio.h>
int main(void){
  wr_seed(6); audio_init(); game_init();

  puts("--- player driving down a street ---");
  int32_t x=g_p.x,y=g_p.y; uint16_t a; road_snap(&x,&y,&a);
  g_p.x=x; g_p.y=y; g_p.ang=a;
  memset(g_ent,0,sizeof(g_ent));
  g_ent[0].type=ENT_CAR; g_ent[0].hp=100; g_ent[0].state=ST_DRIVE;
  g_ent[0].x=g_p.x+40; g_ent[0].y=g_p.y; g_ent[0].ang=a; game_input_action();
  game_touch_down(STICK_HOME_X,STICK_HOME_Y);
  game_touch_move(STICK_HOME_X,(int16_t)(STICK_HOME_Y-STICK_RANGE));
  int onroad=0,n=0; int32_t sx=g_p.x,sy=g_p.y; int minspd=999,maxspd=0;
  for(int f=0;f<90;f++){
    game_step(); n++;
    if(cell_at_point(g_p.x,g_p.y)==CELL_ROAD) onroad++;
    if(g_p.spd<minspd)minspd=g_p.spd; if(g_p.spd>maxspd)maxspd=g_p.spd;
  }
  printf("  on-road %d%% of %d frames, speed range %d..%d, travelled %.1f tiles\n",
     onroad*100/n,n,minspd,maxspd,
     wr_sqrt((g_p.x-sx)*(g_p.x-sx)+(g_p.y-sy)*(g_p.y-sy))/256.0);

  puts("\n--- AI traffic ---");
  wr_seed(6); game_init();
  for(int f=0;f<300;f++) game_step();      // let traffic populate
  int cars=0; long tot=0, road=0; int stalled=0;
  int32_t px[MAX_ENT],py[MAX_ENT];
  for(int i=0;i<MAX_ENT;i++){ px[i]=g_ent[i].x; py[i]=g_ent[i].y; }
  for(int f=0;f<300;f++){
    ent_update();
    for(int i=0;i<MAX_ENT;i++){
      if(g_ent[i].type!=ENT_CAR||g_ent[i].state==ST_DEAD) continue;
      tot++;
      if(cell_at_point(g_ent[i].x,g_ent[i].y)==CELL_ROAD) road++;
    }
  }
  for(int i=0;i<MAX_ENT;i++){
    if(g_ent[i].type!=ENT_CAR) continue; cars++;
    int32_t d=wr_dist(g_ent[i].x-px[i],g_ent[i].y-py[i]);
    if(d<300) stalled++;
  }
  printf("  %d cars alive; on-road %ld%% of the time; %d barely moved in 300 frames\n",
     cars, tot? road*100/tot : 0, stalled);
  return 0;
}
