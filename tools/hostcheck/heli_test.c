#include "wr.h"
#include <stdio.h>
int main(void){
  wr_seed(9); audio_init(); game_init();
  g_p.wanted=5; g_p.heat=470; g_p.evade=0; g_p.hp=100;
  int heli=0,swat=0,frames=0;
  for(int k=0;k<900;k++){
    g_p.hp=100; game_step();
    heli=swat=0;
    for(int i=0;i<MAX_ENT;i++){
      if(g_ent[i].type==ENT_HELI) heli++;
      if(g_ent[i].type==ENT_COP && g_ent[i].wep==W_MG && g_ent[i].state!=ST_DEAD) swat++;
    }
    if(heli&&!frames){ frames=k; }
    if(k%200==0) printf("  k=%3d heli=%d swat=%d\n",k,heli,swat);
  }
  printf("heli appeared at frame %d; heli=%d swat=%d\n",frames,heli,swat);
  int roping=0,down=0;
  for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_COP&&g_ent[i].wep==W_MG){
    if(g_ent[i].state==ST_ROPE) roping++; else down++; }
  printf("swat on the rope=%d, boots down=%d\n",roping,down);
  // If the player died during the build-up, clear it here. Otherwise the very
  // first SELECT below is read as "respawn", which resets the loadout AND
  // wipes every entity - so the test would then be shooting an empty slot.
  if(g_p.dead){ game_input_action(); for(int f=0;f<200;f++){ g_p.hp=100; game_step(); } }
  g_p.wanted=5; g_p.heat=470; g_p.evade=0;
  for(int f=0;f<500;f++){ g_p.hp=100; g_p.dead=0; game_step(); }

  // kill one and check the MG drops
  // SELECT boards a car if one is within reach, so it would never fire a shot
  // with traffic around. Clear the vehicles first.
  for(int i=0;i<MAX_ENT;i++)
    if(g_ent[i].type==ENT_CAR||g_ent[i].type==ENT_COPCAR) g_ent[i].type=ENT_NONE;
  for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_COP&&g_ent[i].wep==W_MG&&g_ent[i].state!=ST_ROPE){
    g_ent[i].x=g_p.x+380; g_ent[i].y=g_p.y; g_p.weapon=W_PISTOL; g_p.ammo=60; g_p.ang=0;
    for(int f=0;f<300 && g_ent[i].state!=ST_DEAD;f++){
      g_ent[i].x=g_p.x+380; g_ent[i].y=g_p.y;
      g_p.hp=100; g_p.dead=0;          // SWAT shoot back; stay alive to finish
      if(!g_p.attack_cd) game_input_action(); game_step(); }
    printf("swat down=%d\n",g_ent[i].state==ST_DEAD); break; }
  int mg=0; for(int i=0;i<MAX_PICKUP;i++) if(g_pickup[i].alive&&g_pickup[i].weapon==W_MG) mg++;
  printf("machine guns on the ground: %d\n",mg);
  return 0;
}
