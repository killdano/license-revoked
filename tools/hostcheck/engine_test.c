#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-46s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
extern void audio_sfx(uint8_t k);
int main(void){
  char b[128];
  wr_seed(6); audio_init(); game_init();
  g_menu.sfx_on=1; audio_set_enabled(true);
  // Get in a car and drive, so the engine is running.
  // Park a car on the player rather than hoping traffic wanders past.
  {
    int32_t tx=g_p.x, ty=g_p.y; uint16_t ta=0; road_snap(&tx,&ty,&ta);
    g_p.x=tx; g_p.y=ty; g_p.ang=ta;
    for(int i=0;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
    Ent*c=&g_ent[0]; memset(c,0,sizeof(Ent));
    c->type=ENT_CAR; c->sub=CAR_SEDAN; c->hp=100; c->state=ST_IDLE;
    c->x=tx+40; c->y=ty; c->ang=ta;
    game_input_action();                       // climb in
  }
  for(int k=0;k<80;k++){ g_p.stick_f=256; game_step(); }   // get rolling
  snprintf(b,sizeof b,"in_car=%d spd=%d",g_p.in_car,g_p.spd);
  ck("got a car running", g_p.in_car && wr_abs(g_p.spd)>0, b);

  // Now unload everything at once: shotgun, explosion, smashes. Under the
  // old allocator these evict the engine; with a reserved slot they cannot.
  g_p.weapon=W_SHOTGUN; g_p.ammo=20;
  int quiet=0, frames=0;
  for(int k=0;k<200;k++){
    g_p.stick_f=256;
    // Fire the sounds directly: pressing SELECT in a car would climb out
    // of it, which is not what this test is about.
    if((k&3)==0) audio_sfx(SFX_SHOTGUN);
    if((k&7)==0) audio_sfx(SFX_PISTOL);
    if((k&15)==0) boom_add(g_p.x+200, g_p.y);
    game_step();
    if(wr_abs(g_p.spd)>4){
      frames++;
      if(!audio_engine_active()) quiet++;
    }
  }
  snprintf(b,sizeof b,"%d of %d driving frames with the engine voice gone",quiet,frames);
  ck("gunfire cannot silence the engine", frames>40 && quiet*10 < frames, b);
  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
