#define _POSIX_C_SOURCE 200809L
#include "wr.h"
#include <stdio.h>
#include <time.h>
static void hold_fwd(void){ game_touch_down(STICK_HOME_X,STICK_HOME_Y);
  game_touch_move(STICK_HOME_X, (int16_t)(STICK_HOME_Y - STICK_RANGE)); }
static void release(void){ game_touch_up(); }
extern uint32_t wr_prof_polys, wr_prof_pixels, wr_prof_items;
int main(void){
  wr_seed(2024); audio_init(); game_init();
  g_p.heat=400; g_p.wanted=5; g_p.evade=0;
  for(int k=0;k<400;k++){ hold_fwd(); if(k%12==0) game_input_action(); game_step(); }
  uint32_t P=0,X=0,I=0,worstP=0,worstX=0; int N=1500;
  struct timespec a,b; clock_gettime(CLOCK_MONOTONIC,&a);
  for(int f=0;f<N;f++){
    hold_fwd(); game_input_turn((f/50)%3==0?-1:((f/50)%3==1?1:0));
    game_step();
    wr_prof_polys=wr_prof_pixels=0;
    render_frame(NULL,GRect(0,0,200,228));
    P+=wr_prof_polys; X+=wr_prof_pixels; I+=wr_prof_items;
    if(wr_prof_polys>worstP)worstP=wr_prof_polys;
    if(wr_prof_pixels>worstX)worstX=wr_prof_pixels;
    if(g_p.dead) game_input_action();
  }
  clock_gettime(CLOCK_MONOTONIC,&b);
  double ms=((b.tv_sec-a.tv_sec)*1e3+(b.tv_nsec-a.tv_nsec)/1e6)/N;
  printf("per frame  avg: %5.1f polys, %6.0f px, %4.1f sorted items\n",P/(double)N,X/(double)N,I/(double)N);
  printf("per frame worst: %5u polys, %6u px   (screen is %d px)\n",worstP,worstX,200*228);
  printf("overdraw avg %.2fx | host x86 render time %.3f ms/frame\n",X/(double)N/(200*228.0),ms);
  return 0;
}
