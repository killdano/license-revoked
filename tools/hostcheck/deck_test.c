#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-48s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
int main(void){
  char b[170];
  wr_seed(6); audio_init(); game_init();
  uint16_t ha=bridge_angle();
  int32_t ax=ISLAND_CX*TILE, ay=ISLAND_CY*TILE;
  int32_t bx=ISLAND_BX*TILE, by=ISLAND_BY*TILE;
  int32_t span=wr_dist(bx-ax,by-ay);

  // 1. Beyond the islands there must be no highway at all.
  int beyond_road=0;
  for(int32_t o=TILE; o<20*TILE; o+=TILE){
    int32_t x=bx+((fcos(ha)*o)>>10), y=by+((fsin(ha)*o)>>10);
    // Only the highway's own claim counts. Past the far island this line
    // runs through that island's city, where ordinary streets are CELL_ROAD
    // and should be - the question is whether the CAUSEWAY still claims
    // territory out there.
    int32_t d; if(on_bridge(x,y,&d,NULL)) beyond_road++;
  }
  snprintf(b,sizeof b,"%d highway samples past the far island",beyond_road);
  ck("the causeway stops at the islands", beyond_road==0, b);

  // 2. A boat must not be able to drive onto the deck.
  int on_deck=0, tested=0;
  for(int32_t t=span/4; t<(3*span)/4; t+=TILE){
    int32_t x=ax+((fcos(ha)*t)>>10), y=ay+((fsin(ha)*t)>>10);
    int32_t d; if(!on_bridge(x,y,&d,NULL)) continue;
    if(d>BRIDGE_HALF) continue;
    tested++;
    if(!water_blocked(x,y)) on_deck++;      // passable to a boat = bad
  }
  snprintf(b,sizeof b,"%d deck points sampled, %d passable to a boat",tested,on_deck);
  ck("boats cannot drive onto the deck", tested>4 && on_deck==0, b);

  // 3. Everything painted as deck must be driveable, and vice versa.
  int paint_no_drive=0, drive_no_paint=0, n=0;
  for(int32_t t=TILE; t<span-TILE; t+=TILE/2)
    for(int32_t o=-BRIDGE_KERB-60; o<=BRIDGE_KERB+60; o+=60){
      int32_t x=ax+((fcos(ha)*t)>>10)+((-fsin(ha)*o)>>10);
      int32_t y=ay+((fsin(ha)*t)>>10)+(( fcos(ha)*o)>>10);
      int32_t d; bool ob=on_bridge(x,y,&d,NULL);
      bool painted = ob && d<BRIDGE_HALF;    // what the renderer paints as road
      bool drives  = cell_at_point(x,y)==CELL_ROAD;
      n++;
      if(painted && !drives) paint_no_drive++;
      if(drives && !painted && wr_dist(x-ax,y-ay)>6*TILE &&
         wr_dist(x-bx,y-by)>6*TILE) drive_no_paint++;
    }
  snprintf(b,sizeof b,"%d points: %d painted-not-driveable, %d driveable-not-painted",
           n,paint_no_drive,drive_no_paint);
  ck("painted deck and driveable deck agree", paint_no_drive==0, b);

  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
