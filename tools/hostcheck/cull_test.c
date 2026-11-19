#include "wr.h"
#include "wr_gfx.h"
#include <stdio.h>
extern uint32_t wr_prof_polys;
int main(void){
  wr_seed(21); audio_init(); game_init();
  // Count how many of the lots in view get hidden, at each zoom level.
  for(int z=0;z<=256;z+=85){
    g_p.zoom=(int16_t)z;
    for(int i=0;i<90;i++) render_frame(NULL,GRect(0,0,200,228));
    int drawn=0,total=0;
    for(int f=0;f<40;f++){
      wr_prof_polys=0; render_frame(NULL,GRect(0,0,200,228));
      g_p.ang+=400;
    }
    total=g_nlot;
    // measure by disabling nothing - just report lots in cache and polys
    wr_prof_polys=0; render_frame(NULL,GRect(0,0,200,228));
    drawn=(int)wr_prof_polys;
    printf("  zoom %3d: %2d lots cached, %3d polys drawn\n",z,total,drawn);
  }
  puts("\n--- debris colours ---");
  memset(g_gib,0,sizeof(g_gib));
  for(int i=0;i<MAX_PROP;i++) if(g_prop[i].alive){ prop_hit(i,10,0,20); break; }
  int reds=0,other=0,stain=0;
  for(int i=0;i<MAX_GIB;i++) if(g_gib[i].life){
    if(g_gib[i].col==0xD0) reds++; else other++;
    stain+=g_gib[i].stains; }
  printf("  prop hit -> %d blood-coloured, %d material-coloured, %d that stain\n",
     reds,other,stain);
  memset(g_gib,0,sizeof(g_gib));
  gib_burst(g_p.x,g_p.y,40,0,0,5);
  reds=0;other=0;stain=0;
  for(int i=0;i<MAX_GIB;i++) if(g_gib[i].life){
    if(g_gib[i].col==0xD0) reds++; else other++; stain+=g_gib[i].stains; }
  printf("  a body    -> %d blood-coloured, %d material-coloured, %d that stain\n",
     reds,other,stain);
  return 0;
}
