#include "wr.h"
#include <stdio.h>
extern int32_t g_proj_cy;
static int strip(void){ return SCR_W - 26; }
static void frames(int n){ for(int i=0;i<n;i++){ game_step(); render_frame(NULL,GRect(0,0,200,228)); } }
// Where does the HUD knob sit for a given zoom?
static int knob_y(int zoom){ int span=SCR_H-54; return 24 + (zoom*span)/256; }
int main(void){
  wr_seed(6); audio_init(); game_init();
  printf("start: zoom=%d proj_cy=%d\n\n", g_p.zoom, (int)g_proj_cy);

  puts("--- drag UP on the strip (finger 170 -> 90), then lift ---");
  game_touch_down((int16_t)(strip()+10), 170);
  printf("  touchdown: zoom_on=%d base=%d ref=%d\n",g_p.zoom_on,g_p.zoom_base,g_p.zoom_ref);
  for(int y=170;y>=90;y-=10){
    game_touch_move((int16_t)(strip()+10),(int16_t)y);
  }
  int held=g_p.zoom;
  printf("  while held: zoom=%d  knob_y=%d (finger went 170->90, i.e. UP)\n",held,knob_y(held));
  game_touch_up();
  printf("  right after lift: zoom=%d\n",g_p.zoom);
  frames(120);
  printf("  120 frames later: zoom=%d proj_cy=%d\n",g_p.zoom,(int)g_proj_cy);
  printf("  -> value held? %s\n", g_p.zoom==held ? "yes" : "NO, it moved");

  puts("\n--- does the eased camera actually reach the set value? ---");
  for(int target=0;target<=256;target+=128){
    g_p.zoom=(int16_t)target; g_p.spd=0;
    frames(200);
    // recompute what s_zoom_t settled on by probing proj_cy against expectation
    printf("  set zoom=%3d -> proj_cy=%4d\n",target,(int)g_proj_cy);
  }

  puts("\n--- knob direction check ---");
  printf("  zoom=0   (closest) knob_y=%d\n",knob_y(0));
  printf("  zoom=256 (furthest) knob_y=%d\n",knob_y(256));
  puts("  finger up = zoom decreases (closer). Knob should ALSO go up (smaller y).");
  return 0;
}
