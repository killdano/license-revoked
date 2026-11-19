#include "wr.h"
#include "wr_gfx.h"
#include <stdio.h>
extern uint32_t wr_prof_polys;
int main(void){
  wr_seed(6); audio_init(); game_init();
  puts("--- do the street bands survive a full turn? ---");
  int minp=99999,maxp=0;
  for(int a=0;a<64;a++){
    g_p.ang=(uint16_t)(a*1024);
    render_frame(NULL,GRect(0,0,200,228));
    wr_prof_polys=0; render_frame(NULL,GRect(0,0,200,228));
    int p=(int)wr_prof_polys;
    if(p<minp)minp=p; if(p>maxp)maxp=p;
  }
  printf("  polys over 64 headings: min=%d max=%d  (a collapse to near zero means bands dropped)\n",minp,maxp);

  puts("\n--- can a car follow a diagonal street? ---");
  // put a car on a road band and let it drive
  memset(g_ent,0,sizeof(g_ent));
  int32_t sx=0,sy=0;
  for(int32_t t=0;t<4000;t+=32)
    if(cell_at_point(g_p.x+t,g_p.y)==CELL_ROAD){ sx=g_p.x+t; sy=g_p.y; break; }
  g_ent[0].type=ENT_CAR; g_ent[0].sub=CAR_SEDAN; g_ent[0].hp=100;
  g_ent[0].state=ST_DRIVE; g_ent[0].x=sx; g_ent[0].y=sy; g_ent[0].spd=14;
  g_ent[0].ang=8192;
  int32_t x0=g_ent[0].x,y0=g_ent[0].y; int onroad=0;
  for(int f=0;f<300;f++){ ent_update();
    if(cell_at_point(g_ent[0].x,g_ent[0].y)==CELL_ROAD) onroad++; }
  int32_t dist=wr_sqrt((g_ent[0].x-x0)*(g_ent[0].x-x0)+(g_ent[0].y-y0)*(g_ent[0].y-y0));
  printf("  after 300 frames: travelled %d subunits (%.1f tiles), on-road %d%% of the time\n",
     (int)dist, dist/256.0, onroad*100/300);
  return 0;
}
