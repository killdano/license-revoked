#include "wr.h"
#include <stdio.h>
int main(void){
  wr_seed(6); audio_init(); game_init();
  int worst=0;
  for(int trial=0;trial<4;trial++){
    game_init();
    int32_t x=g_p.x,y=g_p.y; uint16_t a; road_snap(&x,&y,&a);
    // force each of the four street headings in turn
    const uint16_t H[4]={8192,24576,40960,57344};
    a=H[trial]; road_snap(&x,&y,NULL);
    g_p.x=x; g_p.y=y; g_p.ang=a;
    memset(g_ent,0,sizeof(g_ent));
    g_ent[0].type=ENT_CAR; g_ent[0].hp=100; g_ent[0].state=ST_DRIVE;
    g_ent[0].x=g_p.x+40; g_ent[0].y=g_p.y; g_ent[0].ang=a; game_input_action();
    game_touch_down(STICK_HOME_X,STICK_HOME_Y);
    game_touch_move(STICK_HOME_X,(int16_t)(STICK_HOME_Y-STICK_RANGE));
    int32_t inv0=(a==8192||a==40960)?(g_p.x-g_p.y):(g_p.x+g_p.y);
    int maxd=0;
    for(int f=0;f<400;f++){
      game_step();
      if(cell_at_point(g_p.x,g_p.y)!=CELL_ROAD) break;
      int32_t inv=(a==8192||a==40960)?(g_p.x-g_p.y):(g_p.x+g_p.y);
      int d=(int)(inv-inv0); if(d<0)d=-d; if(d>maxd)maxd=d;
    }
    printf("  heading %5u: max drift off centreline = %d subunits (road half-width %d)\n",
       a,maxd,ROAD_HALF);
    if(maxd>worst)worst=maxd;
  }
  printf("  worst %d -> %s\n",worst, worst<ROAD_HALF/2?"stays in lane":"DRIFTS");
  return 0;
}
