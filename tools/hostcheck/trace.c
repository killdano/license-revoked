#include "wr.h"
#include <stdio.h>
int main(void){
  wr_seed(6); audio_init(); game_init();
  int32_t x=g_p.x,y=g_p.y; uint16_t a; road_snap(&x,&y,&a);
  g_p.x=x; g_p.y=y; g_p.ang=a;
  printf("snapped to (%d,%d) heading %u; surface=%d\n",(int)x,(int)y,a,cell_at_point(x,y));
  printf("  fcos=%d fsin=%d  (equal magnitudes keep us on the diagonal)\n",
     (int)fcos(a),(int)fsin(a));
  memset(g_ent,0,sizeof(g_ent));
  g_ent[0].type=ENT_CAR; g_ent[0].hp=100; g_ent[0].state=ST_DRIVE;
  g_ent[0].x=g_p.x+40; g_ent[0].y=g_p.y; g_ent[0].ang=a; game_input_action();
  printf("  in_car=%d, after boarding pos=(%d,%d) surface=%d\n",
     g_p.in_car,(int)g_p.x,(int)g_p.y,cell_at_point(g_p.x,g_p.y));
  game_touch_down(STICK_HOME_X,STICK_HOME_Y);
  game_touch_move(STICK_HOME_X,(int16_t)(STICK_HOME_Y-STICK_RANGE));
  int32_t inv0 = (a==8192||a==40960) ? (g_p.x-g_p.y) : (g_p.x+g_p.y);
  for(int f=0;f<40;f++){
    game_step();
    int32_t inv = (a==8192||a==40960) ? (g_p.x-g_p.y) : (g_p.x+g_p.y);
    if(f<12 || f%8==0)
      printf("  f=%2d spd=%2d pos=(%5d,%5d) surf=%d  drift off centreline=%d\n",
        f,g_p.spd,(int)g_p.x,(int)g_p.y,cell_at_point(g_p.x,g_p.y),(int)(inv-inv0));
  }
  return 0;
}
