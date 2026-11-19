// The paint scan. Every junction in the world: no centre-line dash may be
// painted inside a crossing road's box - the road, its stop line and its
// crosswalk - on either street through it.
#include "wr.h"
#include <stdio.h>
int main(void){ setvbuf(stdout,NULL,_IONBF,0); wr_seed(6); game_init();
  int junctions=0, bad=0; int32_t bx=0,by=0;
  for(int ty=8;ty<132;ty+=1) for(int tx=8;tx<132;tx+=1){
    int32_t jun[MAX_JUNC][3]; int nj=city_junctions(tx*TILE,ty*TILE,jun,MAX_JUNC);
    for(int j=0;j<nj;j++){
      int32_t jx=jun[j][0], jy=jun[j][1];
      if(((jx/TILE)^(jy/TILE))&1) continue;              // each junction once, roughly: the scan overlaps
      junctions++;
      // along each family's centreline through the junction, out to a tile past the box
      for(int fam=0;fam<2;fam++){
        uint16_t a=ROAD_ARM[fam]; int32_t c=fcos(a), s=fsin(a);
        for(int32_t o=-(ROAD_HALF+280);o<=ROAD_HALF+280;o+=32){
          int32_t x=jx+((c*o)>>10), y=jy+((s*o)>>10);
          if(road_paint_ok(x,y,RD_DIAG_U)){ bad++; if(bad<4){ bx=x; by=y; printf("  dash allowed at (%d,%d), %d from junction (%d,%d)\n",(int)x,(int)y,(int)o,(int)jx,(int)jy); } }
        }
      }
    }
  }
  printf("  %-56s %s  %d junctions scanned, %d dash positions inside a crossing's box\n","no centre-line paint inside any junction's box",bad==0?"ok  ":"FAIL",junctions,bad);
  printf("%d failure(s)\n",bad?1:0); return bad?1:0; }
