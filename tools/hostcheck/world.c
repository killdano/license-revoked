#include "wr.h"
#include <stdio.h>
int main(void){
  wr_seed(6); audio_init(); game_init();
  // Wide low-res map of the whole world.
  printf("world map (. road  p pave  B bld  # park  s sand  ~ water)\n\n");
  for(int32_t ty=10;ty<=100;ty+=3){
    for(int32_t tx=6;tx<=132;tx+=2){
      const char *sym=".pB#s~";
      putchar(sym[cell_at(tx,ty)%6]);
    }
    putchar('\n');
  }
  int counts[6]={0};
  for(int32_t ty=0;ty<140;ty++) for(int32_t tx=0;tx<140;tx++) counts[cell_at(tx,ty)%6]++;
  printf("\nmix: road=%d pave=%d bld=%d park=%d sand=%d water=%d\n",
    counts[0],counts[1],counts[2],counts[3],counts[4],counts[5]);
  printf("bridge heading = %u\n", bridge_angle());
  // walk the highway from one island centre to the other
  int gap=0,deck=0;
  for(int t=0;t<=1024;t+=8){
    int32_t x=ISLAND_CX*TILE + (ISLAND_BX-ISLAND_CX)*TILE*t/1024;
    int32_t y=ISLAND_CY*TILE + (ISLAND_BY-ISLAND_CY)*TILE*t/1024;
    if(cell_at_point(x,y)==CELL_ROAD) deck++;
    // is the water either side of the deck?
    int32_t off=BRIDGE_KERB+TILE;
    uint16_t a=bridge_angle();
    int32_t px=x+fmul(-off,fsin(a)), py=y+fmul(off,fcos(a));
    if(cell_at_point(px,py)==CELL_WATER) gap++;
  }
  printf("highway: %d/129 samples are drivable deck, %d of those span open water\n",deck,gap);
  for(int i=0;i<MARINA_COUNT;i++)
    printf("  marina %d at tile %d,%d  surface=%d\n",i,MARINA_TX[i],MARINA_TY[i],
       cell_at(MARINA_TX[i],MARINA_TY[i]));
  return 0;
}
