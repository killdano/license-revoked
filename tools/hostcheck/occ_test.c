#include "wr.h"
#include "wr_gfx.h"
#include <stdio.h>
unsigned char *stub_fb(void);
extern uint32_t wr_prof_culled;
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-48s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
int main(void){
  char b[160];
  wr_seed(6); audio_init(); game_init();
  g_p.zoom=0;                      // low chase camera: things get in the way
  int frames=0, occ_frames=0; long stipple_px=0;
  // The camera sits behind the player, so an occluder is a building the
  // player has his back to. Stand just clear of a tall lot facing away.
  for(int a=0;a<260 && occ_frames<6;a++){
    g_p.x=(int32_t)(wr_rand_n(60)+30)*TILE; g_p.y=(int32_t)(wr_rand_n(60)+30)*TILE;
    if(cell_at_point(g_p.x,g_p.y)==CELL_WATER) continue;
    city_refresh(g_p.x,g_p.y);
    int li=-1; int32_t lcx=0, lcy=0;
    for(int i=0;i<g_nlot;i++){
      if(g_lot[i].h < 3*TILE) continue;
      li=i; lcx=(g_lot[i].x0+g_lot[i].x1)/2; lcy=(g_lot[i].y0+g_lot[i].y1)/2; break;
    }
    if(li<0) continue;
    for(int t=0;t<16;t++){
      uint16_t out=(uint16_t)(t*4096);
      int32_t sx=lcx+((fcos(out)*(2*TILE))>>10);
      int32_t sy=lcy+((fsin(out)*(2*TILE))>>10);
      if(city_blocked(sx,sy,false)) continue;
      g_p.x=sx; g_p.y=sy;
      g_p.ang=out;                       // facing away: building behind us
      uint32_t before=wr_prof_culled;
      for(int f=0;f<3;f++) render_frame(NULL,GRect(0,0,200,228));
      frames++;
      if(wr_prof_culled==before) continue;
      occ_frames++;
      // A stippled wall leaves alternating pixels: count runs where a pixel
      // differs from its right-hand neighbour along a row in the upper half.
      unsigned char*fb=stub_fb();
      long alt=0;
      for(int y=40;y<110;y++){
        int run=0;
        for(int x=20;x<180;x++){
          if(fb[y*200+x]!=fb[y*200+x+1]) run++;
        }
        if(run>40) alt++;          // a row full of alternation = stipple
      }
      stipple_px+=alt;
      break;
    }
  }
  snprintf(b,sizeof b,"%d frames with an occluder, %ld stippled rows",occ_frames,stipple_px);
  ck("occluding buildings are stippled, not deleted", occ_frames>0 && stipple_px>0, b);
  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
