#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){ printf("  %-38s %s   %s\n",n,ok?"ok  ":"FAIL",d); if(!ok)fails++; }
int main(void){
  char b[90];

  // 1. Does the helicopter ever leave once you are clean?
  wr_seed(6); audio_init(); game_init();
  g_p.wanted=5; g_p.hostile=1; g_p.copkills=2; g_p.hot=800; g_p.heat=470; g_p.evade=0;
  for(int f=0;f<400;f++){ g_p.hp=100; game_step(); }
  int had=0; for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_HELI) had++;
  g_p.wanted=0; g_p.heat=0; g_p.evade=9999;
  int died=0;
  for(int f=0;f<900;f++){
    g_p.hp=100;
    if(g_p.dead){ died++; game_input_action(); g_p.wanted=0; g_p.heat=0; }
    game_step();
  }
  printf("    (player died %d times during the cool-down)\n",died);
  int still=0; for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_HELI) still++;
  snprintf(b,sizeof b,"deployed %d, still overhead at 0 stars: %d",had,still);
  ck("helicopter stands down", had>0 && still==0, b);

  // 2. Blowing up the car you are sitting in
  wr_seed(6); game_init(); memset(g_ent,0,sizeof(g_ent));
  g_ent[0].type=ENT_CAR; g_ent[0].hp=100; g_ent[0].state=ST_DRIVE;
  g_ent[0].x=g_p.x+40; g_ent[0].y=g_p.y; g_ent[0].ang=g_p.ang;
  game_input_action();
  int was=g_p.in_car;
  boom_add(g_p.x, g_p.y);
  for(int f=0;f<3;f++) game_step();
  snprintf(b,sizeof b,"boarded=%d, still 'driving' the wreck: %s",was,
     (g_p.in_car && g_ent[g_p.in_car-1].state==ST_DEAD)?"YES":"no");
  ck("explosion ejects you from the wreck",
     !(g_p.in_car && g_ent[g_p.in_car-1].state==ST_DEAD), b);

  // 3. Being rammed to death in a car
  wr_seed(6); game_init(); memset(g_ent,0,sizeof(g_ent));
  g_ent[0].type=ENT_CAR; g_ent[0].hp=100; g_ent[0].state=ST_DRIVE;
  g_ent[0].x=g_p.x+40; g_ent[0].y=g_p.y; g_ent[0].ang=g_p.ang;
  game_input_action();
  g_ent[1].type=ENT_CAR; g_ent[1].hp=100; g_ent[1].state=ST_DRIVE;
  g_ent[1].x=g_p.x+60; g_ent[1].y=g_p.y; g_ent[1].spd=30; g_ent[1].ang=32768;
  g_p.spd=30;
  int bad=0;
  for(int f=0;f<200;f++){ g_p.hp=100; game_step();
    if(g_p.in_car && g_ent[g_p.in_car-1].state==ST_DEAD) bad=1; }
  ck("crash never leaves you in a dead car", !bad, bad?"driving a wreck":"clean");

  // 4. Menu while inside the gun shop
  wr_seed(6); game_init();
  shop_enter(g_p.x,g_p.y,0,1);
  menu_toggle();
  int32_t sx=g_p.x;
  for(int f=0;f<60;f++) game_step();
  int frozen = (g_p.x==sx);
  menu_toggle(); shop_exit();
  snprintf(b,sizeof b,"world frozen: %s; left the shop: %s",frozen?"yes":"no",g_p.inside?"no":"yes");
  ck("pause works inside an interior", frozen && !g_p.inside, b);

  // 5. Dying inside the shop
  wr_seed(6); game_init();
  shop_enter(g_p.x,g_p.y,0,1);
  g_p.hp=0;
  for(int f=0;f<4;f++) game_step();
  // The death cinematic has to play out before SELECT works.
  for(int f=0;f<50;f++) game_step();
  game_input_action();                    // respawn
  for(int f=0;f<4;f++) game_step();
  snprintf(b,sizeof b,"inside=%d lots=%d surface=%d",g_p.inside,g_nlot,cell_at_point(g_p.x,g_p.y));
  ck("dying indoors puts you back outside", !g_p.inside && g_nlot>5, b);

  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
