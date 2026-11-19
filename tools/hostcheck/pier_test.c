#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-46s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
int main(void){
  char b[160];
  wr_seed(6); audio_init(); game_init();
  int marinas=0, piered=0, longest=0, over_land=0;
  // Visit each marina in turn.
  for(int m=0;m<4;m++){
    int32_t md; int mi;
    // Walk the world to find distinct marinas by standing near each island.
    int32_t px = (m<2?ISLAND_CX:ISLAND_BX)*TILE, py=(m<2?ISLAND_CY:ISLAND_BY)*TILE;
    int32_t r = (m&1)? (CITY_R-2)*TILE : -(CITY_R-2)*TILE;
    g_p.x = px + r; g_p.y = py;
    mi = nearest_marina(g_p.x,g_p.y,&md);
    g_p.x = MARINA_TX[mi]*TILE; g_p.y = MARINA_TY[mi]*TILE;
    city_refresh(g_p.x,g_p.y); props_refresh();
    marinas++;
    int planks=0;
    for(int i=0;i<MAX_PROP;i++){
      Prop*p=&g_prop[i];
      if(!p->alive||p->kind!=PROP_DOCK) continue;
      if(wr_dist(p->x-g_p.x,p->y-g_p.y)>12*TILE) continue;
      planks++;
      // A plank should be over water or right at the shore, never inland.
      if(cell_at_point(p->x,p->y)!=CELL_WATER &&
         cell_at_point(p->x,p->y)!=CELL_SAND) over_land++;
    }
    if(planks>longest) longest=planks;
    if(planks>=3) piered++;
  }
  snprintf(b,sizeof b,"%d marinas visited, %d have a pier of 3+ sections (longest %d)",
     marinas,piered,longest);
  ck("marinas have a pier running into the water", piered>=2 && longest>=3, b);
  snprintf(b,sizeof b,"%d pier sections sitting inland",over_land);
  ck("no pier section is built on dry land", over_land==0, b);
  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
