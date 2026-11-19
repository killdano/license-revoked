#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-48s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
static int flames(void){
  int n=0; for(int i=0;i<MAX_BULLET;i++) if(g_bullet[i].life&&g_bullet[i].kind==B_FIRE) n++;
  return n;
}
int main(void){
  char b[170];
  setvbuf(stdout,NULL,_IONBF,0);
  wr_seed(6); audio_init(); game_init();

  // Held: a continuous flame.
  g_p.weapon=W_FLAME; g_p.ammo=200; g_p.attack_cd=0;
  game_input_hold(true);                 // press; the hold fires once by itself
  int lit=0, peak=0;
  for(int k=0;k<60;k++){ game_step(); int f=flames(); if(f>peak)peak=f; if(f) lit++; }
  snprintf(b,sizeof b,"flame alight on %d of 60 frames, up to %d tongues at once",lit,peak);
  ck("holding the button keeps the flame lit", lit>48 && peak>3, b);   // the hold takes a third of a second to become one

  // Released: it stops.
  game_input_hold(false);
  int after=0;
  for(int k=0;k<40;k++){ game_step(); if(flames()) after++; }
  snprintf(b,sizeof b,"flame present on %d of 40 frames after release",after);
  ck("releasing puts it out", after<12, b);

  // A pistol must NOT auto-fire on a held button.
  wr_seed(6); game_init();
  g_p.weapon=W_PISTOL; g_p.ammo=48; g_p.attack_cd=0;
  int spent0=g_p.ammo;
  game_input_hold(true);                 // press: fires at once (no car in reach)
  for(int k=0;k<60;k++) game_step();
  snprintf(b,sizeof b,"pistol fired %d rounds while held",spent0-g_p.ammo);
  ck("a pistol stays one press, one shot", (spent0-g_p.ammo)==1, b);

  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
