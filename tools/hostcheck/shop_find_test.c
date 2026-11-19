#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-46s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
int main(void){
  char b[160];
  wr_seed(6); audio_init(); game_init();
  // Stand somewhere with a shop in the cache and confirm one is findable
  // within a couple of blocks - which is what the pointer is for.
  int spots=0, withshop=0; int32_t nearest_worst=0;
  for(int a=0;a<200 && spots<40;a++){
    g_p.x=(int32_t)(wr_rand_n(60)+30)*TILE; g_p.y=(int32_t)(wr_rand_n(60)+30)*TILE;
    if(cell_at_point(g_p.x,g_p.y)!=CELL_ROAD) continue;
    city_refresh(g_p.x,g_p.y);
    spots++;
    int32_t bd=0x7FFFFFFF;
    for(int i=0;i<g_nlot;i++){
      if(g_lot[i].kind!=LOT_GUNSHOP) continue;
      int32_t cx=(g_lot[i].x0+g_lot[i].x1)/2, cy=(g_lot[i].y0+g_lot[i].y1)/2;
      int32_t d=wr_dist(cx-g_p.x,cy-g_p.y);
      if(d<bd) bd=d;
    }
    if(bd<0x7FFFFFFF){ withshop++; if(bd>nearest_worst) nearest_worst=bd; }
  }
  snprintf(b,sizeof b,"%d street spots, %d had a gun shop in the cache (furthest %ld tiles)",
    spots,withshop,(long)(nearest_worst/TILE));
  ck("a gun shop is usually within a block or two", spots>20 && withshop*4>=spots*3, b);
  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
