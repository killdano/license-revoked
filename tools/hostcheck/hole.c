#include "wr.h"
#include "wr_gfx.h"
#include <stdio.h>
// Read the framebuffer directly and check the tarmac reaches the player.
extern unsigned char *stub_fb(void);
int main(void){
  wr_seed(6); audio_init(); game_init();
  int32_t x=g_p.x,y=g_p.y; uint16_t a; road_snap(&x,&y,&a);
  g_p.x=x; g_p.y=y; g_p.ang=a;
  const unsigned char ROAD=0xC0|(1<<4)|(1<<2)|1;   // ARGB(1,1,1) tarmac
  const unsigned char LINE=0xC0|(3<<4)|(3<<2)|1;   // ARGB(3,3,1) centre marking
  puts(" zoom  player screen   pixel just below him   road under the player?");
  int bad=0;
  for(int z=0;z<=256;z+=32){
    g_p.zoom=(int16_t)z;
    for(int i=0;i<160;i++) render_frame(NULL,GRect(0,0,200,228));
    Proj p=gfx_project(g_p.x,g_p.y,0);
    unsigned char *fb=stub_fb();
    // sample a few pixels around his feet, avoiding the character himself
    int hits=0,tries=0;
    for(int dy=4;dy<=8;dy+=2) for(int dx=-5;dx<=5;dx+=5){
      int px=p.sx+dx, py=p.sy+dy;
      if(px<0||px>=200||py<0||py>=228) continue;
      tries++; unsigned char c=fb[py*200+px]; if(c==ROAD||c==LINE) hits++;
    }
    int ok = tries && hits*100/tries >= 50;
    if(!ok) bad++;
    printf("  %3d   (%3d,%3d)        %2d/%2d tarmac            %s\n",
      z,p.sx,p.sy,hits,tries, ok?"yes":"NO - HOLE");
  }
  printf("\n%d zoom level(s) with a hole under the player\n",bad);
  return bad?1:0;
}
