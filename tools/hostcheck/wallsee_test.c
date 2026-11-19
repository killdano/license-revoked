#include "wr.h"
#include "wr_gfx.h"
#include <stdio.h>
extern uint32_t wr_prof_culled, wr_prof_lots_seen, wr_prof_occ_depth, wr_prof_occ_bbox;
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-48s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
int main(void){
  char b[170];
  setvbuf(stdout,NULL,_IONBF,0);
  wr_seed(6); audio_init(); game_init();

  // Indoors: stand against each wall in turn and face it, so the camera
  // behind you is on the far side of a wall every time.
  int li=-1;
  for(int a=0;a<400 && li<0;a++){
    g_p.x=(int32_t)(wr_rand_n(60)+30)*TILE; g_p.y=(int32_t)(wr_rand_n(60)+30)*TILE;
    if(cell_at_point(g_p.x,g_p.y)==CELL_WATER) continue;
    city_refresh(g_p.x,g_p.y);
    for(int i=0;i<g_nlot;i++) if(g_lot[i].kind==LOT_GUNSHOP){ li=i; break; }
  }
  int32_t dx,dy,nx,ny; city_shop_door(&g_lot[li],&dx,&dy,&nx,&ny);
  shop_enter(dx,dy,nx,ny);

  int views=0, occluded=0;
  for(int corner=0;corner<4;corner++){
    g_p.x = (corner&1) ? ROOM_OX+ROOM_W-40 : ROOM_OX+40;
    g_p.y = (corner&2) ? ROOM_OY+ROOM_H-40 : ROOM_OY+40;
    for(int t=0;t<8;t++){
      g_p.ang=(uint16_t)(t*8192);
      uint32_t before=wr_prof_culled;
      for(int f=0;f<3;f++) render_frame(NULL,GRect(0,0,200,228));
      views++;
      if(wr_prof_culled>before) occluded++;
    }
  }
  printf("  lots drawn %u | passed depth %u | inside bbox %u | stippled %u\n",
         wr_prof_lots_seen, wr_prof_occ_depth, wr_prof_occ_bbox, wr_prof_culled);
  snprintf(b,sizeof b,"%d indoor views, wall stippled in %d of them",views,occluded);
  ck("walls between camera and player are seen through", occluded>0, b);
  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
