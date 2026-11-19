#include "wr.h"
#include "wr_gfx.h"
#include <stdio.h>
extern uint32_t wr_prof_polys, wr_prof_pixels;
// Sample the colour of a fixed world point on the road across many headings.
// If the street rendering is stable, a spot on the tarmac stays tarmac.
int main(void){
  wr_seed(6); audio_init(); game_init();
  // stand on a road centreline
  int32_t x=g_p.x,y=g_p.y; uint16_t a; road_snap(&x,&y,&a);
  g_p.x=x; g_p.y=y; g_p.zoom=40;
  for(int i=0;i<200;i++) render_frame(NULL,GRect(0,0,200,228));

  puts("--- polygon count across a full 360 turn (stability) ---");
  int minp=1<<30,maxp=0; long sum=0;
  for(int t=0;t<128;t++){
    g_p.ang=(uint16_t)(t*512);
    render_frame(NULL,GRect(0,0,200,228));
    wr_prof_polys=0; render_frame(NULL,GRect(0,0,200,228));
    int p=(int)wr_prof_polys; sum+=p;
    if(p<minp)minp=p; if(p>maxp)maxp=p;
  }
  printf("  min=%d avg=%ld max=%d   spread=%d%%\n",minp,sum/128,maxp,(maxp-minp)*100/(maxp?maxp:1));

  puts("\n--- frame-to-frame jitter while driving straight ---");
  g_p.ang=8192; g_p.zoom=40;
  memset(g_ent,0,sizeof(g_ent));
  g_ent[0].type=ENT_CAR; g_ent[0].hp=100; g_ent[0].state=ST_DRIVE;
  g_ent[0].x=g_p.x+40; g_ent[0].y=g_p.y; game_input_action();
  game_touch_down(STICK_HOME_X,STICK_HOME_Y);
  game_touch_move(STICK_HOME_X,(int16_t)(STICK_HOME_Y-STICK_RANGE));
  int prev=-1,jumps=0,maxjump=0;
  for(int f=0;f<400;f++){
    game_step(); wr_prof_polys=0; render_frame(NULL,GRect(0,0,200,228));
    int p=(int)wr_prof_polys;
    if(prev>=0){ int d=p-prev; if(d<0)d=-d; if(d>maxjump)maxjump=d; if(d>25) jumps++; }
    prev=p;
  }
  printf("  frames with a >25 polygon jump: %d/400, worst single jump %d\n",jumps,maxjump);
  printf("  final surface under the car: %d (0=road)  speed=%d\n",
     cell_at_point(g_p.x,g_p.y), g_p.spd);
  return 0;
}
