#define _POSIX_C_SOURCE 200809L
#include "wr.h"
#include <stdio.h>
#include <time.h>
int main(void){
  wr_seed(6); audio_init(); game_init();
  g_p.zoom=128;
  for(int i=0;i<200;i++){ game_step(); render_frame(NULL,GRect(0,0,200,228)); }
  int N=4000;
  struct timespec a,b; clock_gettime(CLOCK_MONOTONIC,&a);
  for(int f=0;f<N;f++){
    g_p.ang=(uint16_t)(f*37);
    render_frame(NULL,GRect(0,0,200,228));
  }
  clock_gettime(CLOCK_MONOTONIC,&b);
  double ms=((b.tv_sec-a.tv_sec)*1e3+(b.tv_nsec-a.tv_nsec)/1e6)/N;
  printf("  render only: %.4f ms/frame on this host\n",ms);
  return 0;
}
