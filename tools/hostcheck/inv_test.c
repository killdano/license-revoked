#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-48s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
int main(void){
  char b[170];
  setvbuf(stdout,NULL,_IONBF,0);
  wr_seed(6); audio_init(); game_init();

  // Pick up three different weapons.
  for(int i=0;i<MAX_PICKUP;i++) g_pickup[i].alive=0;
  const uint8_t want[3]={W_PISTOL,W_SHOTGUN,W_MG};
  for(int k=0;k<3;k++){
    pickup_drop(g_p.x, g_p.y, want[k]);
    for(int f=0;f<3;f++) game_step();
  }
  int carried=0;
  for(int w=1;w<W_COUNT;w++) if(g_p.inv[w]) carried++;
  snprintf(b,sizeof b,"carrying %d weapons, holding %d with %d rounds",
           carried,g_p.weapon,g_p.ammo);
  ck("you keep every weapon you pick up", carried>=3, b);

  // Cycling must visit all of them and come back.
  int seen[W_COUNT]; for(int i=0;i<W_COUNT;i++) seen[i]=0;
  for(int k=0;k<W_COUNT*2;k++){ seen[g_p.weapon]=1; weapon_next(); }
  int visited=0; for(int w=0;w<W_COUNT;w++) if(seen[w]) visited++;
  snprintf(b,sizeof b,"cycle visited %d of the %d carried (plus fists)",visited,carried+1);
  ck("the switch cycles through everything carried", visited>=carried+1, b);

  // Ammunition must survive a round trip.
  while(g_p.weapon!=W_SHOTGUN) weapon_next();
  int before=g_p.ammo;
  weapon_next(); weapon_next(); weapon_next(); weapon_next();
  while(g_p.weapon!=W_SHOTGUN) weapon_next();
  snprintf(b,sizeof b,"shotgun had %d rounds, has %d after cycling",before,g_p.ammo);
  ck("ammunition survives switching away and back", before>0 && g_p.ammo==before, b);

  // Arrests need you hurt first.
  g_p.hp=100; g_p.wanted=2; g_p.arrest=0; g_p.spd=0;
  for(int i=0;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
  { Ent*c=&g_ent[0]; memset(c,0,sizeof(Ent));
    c->type=ENT_COP; c->hp=100; c->state=ST_CHASE; c->x=g_p.x+30; c->y=g_p.y; }
  for(int k=0;k<90;k++){ g_p.hp=100; g_p.spd=0; game_step(); }
  int healthy=g_p.arrest;
  g_p.hp=20;
  for(int k=0;k<120;k++){ g_p.hp=20; g_p.spd=0; game_step(); }
  snprintf(b,sizeof b,"arrest meter %d at full health, %d when hurt",healthy,g_p.arrest);
  ck("they must beat you down before cuffing you", healthy==0 && g_p.arrest>0, b);

  // The weapon box must draw something for every weapon, and the tap zone
  // must still land on it now that the box is narrower.
  {
    extern uint32_t wr_prof_polys;
    int drew_all = 1;
    for (int w = 0; w < W_COUNT; w++) {
      g_p.weapon = (uint8_t)w;
      uint32_t before = wr_prof_polys;
      render_frame(NULL, GRect(0,0,200,228));
      if (wr_prof_polys == before) drew_all = 0;
    }
    snprintf(b,sizeof b,"every one of the %d weapons draws a HUD icon",W_COUNT);
    ck("each weapon has an icon", drew_all, b);

    // Fresh, and alive: the arrest section above leaves the player busted,
    // and a dead man cannot change weapon.
    wr_seed(6); game_init();
    g_menu.cam_left = 0;
    g_p.weapon = W_PISTOL;
    uint8_t was = g_p.weapon;
    game_touch_down(6 + 25, 33);          // middle of the box
    snprintf(b,sizeof b,"tapping the box moved from %d to %d",was,g_p.weapon);
    ck("the narrower box is still tappable", g_p.weapon != was, b);
  }

  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
