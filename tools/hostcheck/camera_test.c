#include "wr.h"
#include "wr_gfx.h"
#include <stdio.h>
int main(void){
  wr_seed(6); audio_init(); game_init();
  printf("lots near player: %d\n", g_nlot);
  puts("\n zoom  player(x,y)  pitch  horizon  char px tall");
  for(int z=0;z<=256;z+=128){
    g_p.zoom=(int16_t)z;
    for(int i=0;i<140;i++) render_frame(NULL,GRect(0,0,200,228));
    Proj a=gfx_project(g_p.x,g_p.y,0), b=gfx_project(g_p.x,g_p.y,108);
    printf("  %3d   (%3d,%3d)   %2d    %4d     %3d\n",z,a.sx,a.sy,
      (int)(PITCH_NEAR+(((PITCH_FAR-PITCH_NEAR)*z)>>8)),(int)g_horizon,a.sy-b.sy);
  }
  g_p.zoom=110;
  puts("\n run speed on foot:");
  game_touch_down(STICK_HOME_X,STICK_HOME_Y);
  game_touch_move(STICK_HOME_X,(int16_t)(STICK_HOME_Y-STICK_RANGE));
  for(int i=0;i<40;i++) game_step();
  printf("  full forward -> spd=%d (was 9)\n",g_p.spd);
  return 0;
}
