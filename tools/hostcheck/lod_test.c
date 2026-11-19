#define _POSIX_C_SOURCE 200809L
#include "wr.h"
#include "wr_gfx.h"
#include <stdio.h>
extern uint32_t wr_prof_polys, wr_prof_pixels;
static void hold_fwd(void){ game_touch_down(STICK_HOME_X,STICK_HOME_Y);
  game_touch_move(STICK_HOME_X,(int16_t)(STICK_HOME_Y-STICK_RANGE)); }
static void run(const char *label,int drive){
  wr_seed(77); audio_init(); game_init();
  g_p.heat=400; g_p.wanted=5; g_p.evade=0;
  for(int k=0;k<420;k++){ if(drive){hold_fwd(); if(k%12==0) game_input_action();} game_step(); }
  uint32_t P=0,wP=0,wX=0; int N=900;
  for(int f=0;f<N;f++){
    if(drive){ hold_fwd(); game_input_turn((f/50)%3==0?-1:((f/50)%3==1?1:0)); }
    else game_touch_up();
    game_step();
    wr_prof_polys=wr_prof_pixels=0;
    render_frame(NULL,GRect(0,0,200,228));
    P+=wr_prof_polys;
    if(wr_prof_polys>wP)wP=wr_prof_polys;
    if(wr_prof_pixels>wX)wX=wr_prof_pixels;
    if(g_p.dead) game_input_action();
  }
  printf("%-22s avg %5.1f polys   worst %4u polys   worst %6u px\n",
     label,P/(double)N,wP,wX);
}
int main(void){ run("driving (zoomed out)",1); run("on foot (zoomed in)",0); return 0; }
