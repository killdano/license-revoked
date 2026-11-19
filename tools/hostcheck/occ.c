#include "wr.h"
#include <stdio.h>
extern uint32_t wr_prof_culled;
int main(void){
  wr_seed(6); audio_init(); game_init();
  puts(" zoom   lots in cache   avg culled/frame   max culled");
  for(int z=0;z<=256;z+=64){
    g_p.zoom=(int16_t)z;
    for(int i=0;i<140;i++) render_frame(NULL,GRect(0,0,200,228));
    long tot=0; uint32_t mx=0;
    for(int t=0;t<96;t++){
      g_p.ang=(uint16_t)(t*683);
      render_frame(NULL,GRect(0,0,200,228));
      wr_prof_culled=0;
      render_frame(NULL,GRect(0,0,200,228));
      tot+=wr_prof_culled; if(wr_prof_culled>mx) mx=wr_prof_culled;
    }
    printf("  %3d       %3d             %.2f              %u\n",z,g_nlot,tot/96.0,mx);
  }
  return 0;
}
