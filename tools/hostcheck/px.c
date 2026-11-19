#include "wr.h"
#include <stdio.h>
extern uint32_t wr_px_terrain, wr_px_street, wr_px_lots, wr_prof_pixels;
int main(void){
  wr_seed(6); audio_init(); game_init();
  for(int z=0;z<=256;z+=128){
    g_p.zoom=(int16_t)z;
    for(int i=0;i<140;i++) render_frame(NULL,GRect(0,0,200,228));
    long t=0,s=0,l=0,tot=0; int N=64;
    for(int k=0;k<N;k++){
      g_p.ang=(uint16_t)(k*1024);
      wr_px_terrain=wr_px_street=wr_px_lots=wr_prof_pixels=0;
      render_frame(NULL,GRect(0,0,200,228));
      t+=wr_px_terrain; s+=wr_px_street; l+=wr_px_lots; tot+=wr_prof_pixels;
    }
    printf("  zoom %3d: terrain %6ld  streets %6ld  everything-else %6ld   total %6ld  (screen 45600)\n",
      z,t/N,s/N,l/N,tot/N);
  }
  return 0;
}
