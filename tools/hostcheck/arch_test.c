#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-48s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
int main(void){
  char b[180];
  setvbuf(stdout,NULL,_IONBF,0);
  wr_seed(6); audio_init(); game_init();
  int32_t ax=ISLAND_CX*TILE, ay=ISLAND_CY*TILE;
  int32_t bx=ISLAND_BX*TILE, by=ISLAND_BY*TILE;
  int32_t total=wr_dist(bx-ax,by-ay);
  uint16_t ha=bridge_angle();

  // The deck must cover the water and little else.
  int deck_tiles=0, deck_over_land=0;
  for(int32_t t=0;t<=total;t+=TILE/2){
    int32_t x=ax+((fcos(ha)*t)>>10), y=ay+((fsin(ha)*t)>>10);
    int32_t d,al;
    if(!on_bridge(x,y,&d,&al)) continue;
    if(d>BRIDGE_HALF) continue;
    deck_tiles++;
    int32_t tx=x>>TILE_SH, ty=y>>TILE_SH;
    int32_t r2a=(tx-ISLAND_CX)*(tx-ISLAND_CX)+(ty-ISLAND_CY)*(ty-ISLAND_CY);
    int32_t r2b=(tx-ISLAND_BX)*(tx-ISLAND_BX)+(ty-ISLAND_BY)*(ty-ISLAND_BY);
    if(r2a<(CITY_R-4)*(CITY_R-4) || r2b<(CITY_BR-4)*(CITY_BR-4)) deck_over_land++;
  }
  snprintf(b,sizeof b,"%d deck samples, %d of them deep inside a city",
           deck_tiles,deck_over_land);
  ck("the bridge no longer ploughs through the cities", deck_over_land==0, b);

  // Mid-span must be sailable, and the ramps must not be.
  // Beside the deck at mid-span: that is where a boat goes, and where the
  // surface must still be water.
  int32_t peak_h=0, peak_t=0;
  for(int32_t t=0;t<=total;t+=TILE/2){
    int32_t x=ax+((fcos(ha)*t)>>10), y=ay+((fsin(ha)*t)>>10);
    int32_t d2,al2;
    if(!on_bridge(x,y,&d2,&al2)) continue;
    if(bridge_height(al2)>peak_h){ peak_h=bridge_height(al2); peak_t=t; }
  }
  int32_t px=ax+((fcos(ha)*peak_t)>>10), py=ay+((fsin(ha)*peak_t)>>10);
  int32_t sx2=px+((-fsin(ha)*(BRIDGE_KERB+90))>>10);
  int32_t sy2=py+(( fcos(ha)*(BRIDGE_KERB+90))>>10);
  snprintf(b,sizeof b,"peak deck height %ld; beside it the surface is %d",
           (long)peak_h, cell_at_point(sx2,sy2));
  ck("a boat can pass beside the raised span",
     peak_h>90 && cell_at_point(sx2,sy2)==CELL_WATER, b);

  // And you can still drive across the top of it.
  // Walk the deck itself, by distance, and check every step is driveable.
  int road=0,n=0;
  for(int32_t t=0;t<=total;t+=TILE/4){
    int32_t x=ax+((fcos(ha)*t)>>10), y=ay+((fsin(ha)*t)>>10);
    int32_t d3,al3;
    if(!on_bridge(x,y,&d3,&al3) || d3>=BRIDGE_HALF) continue;
    n++;
    if(cell_at_point(x,y)==CELL_ROAD) road++;
  }
  snprintf(b,sizeof b,"%d of %d deck samples are driveable road",road,n);
  ck("the deck is continuous across the span", road==n, b);

  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
