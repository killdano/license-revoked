// The helicopter, end to end: it shows up where you can see it, it lands to
#include "wr_gfx.h"
// deploy SWAT, you can take it while it is down, fly it, shoot from it, and
// put it back down.
#include "wr.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int fails;
static void ck(const char *name, bool ok, const char *detail){
  printf("  %-42s %s   %s\n", name, ok?"ok  ":"FAIL", detail); if(!ok) fails++; }
static int find_heli(void){ for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_HELI) return i; return -1; }
static int count_swat(void){ int n=0; for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_COP&&g_ent[i].wep==W_MG&&g_ent[i].state!=ST_DEAD) n++; return n; }
int main(void){
  char b[160];
  wr_seed(9); audio_init(); game_init();
  // Stand still on a road, four stars, and watch.
  g_p.wanted=4; g_p.hostile=1; g_p.copkills=2; g_p.hot=800; g_p.heat=330; g_p.evade=0;
  int appeared=-1, onscreen=0, chasing=0, landed_at=-1, swat_at=-1, first_close=-1;
  int hi=-1;
  for(int k=0;k<1500;k++){
    g_p.hp=100; g_p.dead=0; g_p.heat=330; g_p.wanted=4; g_p.hostile=1; g_p.copkills=2; g_p.hot=800; g_p.evade=0;
    game_step();
    render_frame(NULL, GRect(0,0,SCR_W,SCR_H));
    hi=find_heli();
    if(hi<0) continue;
    Ent*h=&g_ent[hi];
    if(appeared<0) appeared=k;
    if(h->state==ST_CHASE && h->z>=HELI_ALT && wr_dist(h->x-g_p.x,h->y-g_p.y)<4*TILE){
      chasing++;
      Proj p=gfx_project(h->x,h->y,h->z);
      if(p.ok && p.sx>=0 && p.sx<SCR_W && p.sy>=0 && p.sy<SCR_H) onscreen++;
      if(first_close<0 && wr_dist(h->x-g_p.x,h->y-g_p.y)<4*TILE) first_close=k;
    }
    if(h->state==ST_PARK && landed_at<0) landed_at=k;
    if(getenv("AIRDBG") && k%10==0){ Proj q=gfx_project(h->x,h->y,h->z); printf("    k=%d st=%d z=%d d=%d sx=%d sy=%d ok=%d zoom=%d\n",k,h->state,h->z,(int)wr_dist(h->x-g_p.x,h->y-g_p.y),q.sx,q.sy,q.ok,g_p.zoom); }
    if(swat_at<0 && count_swat()>=2) swat_at=k;
    if(landed_at>=0 && h->state==ST_PARK) break;
  }
  snprintf(b,sizeof b,"appeared f=%d, within 4 tiles by f=%d",appeared,first_close);
  ck("air support arrives", appeared>=0 && first_close>=0, b);
  snprintf(b,sizeof b,"on screen %d of %d flying frames",onscreen,chasing);
  ck("helicopter is on screen while it hunts", chasing>0 && onscreen*100/chasing>=55, b);
  snprintf(b,sizeof b,"landed f=%d, 2 swat by f=%d, swat now %d",landed_at,swat_at,count_swat());
  ck("it lands and deploys SWAT", landed_at>=0 && swat_at>=0, b);

  // Take it. Clear the SWAT and walk up to the door.
  hi=find_heli();
  if(hi<0){ printf("no helicopter to test with\n"); return fails?1:0; }
  for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_COP||g_ent[i].type==ENT_CAR||g_ent[i].type==ENT_COPCAR) g_ent[i].type=ENT_NONE;
  Ent*h=&g_ent[hi];
  g_p.x=h->x+150; g_p.y=h->y; g_p.dead=0; g_p.hp=100;
  game_input_action();
  snprintf(b,sizeof b,"in_car=%d (heli slot %d) state=%d",g_p.in_car,hi+1,h->state);
  ck("you can take a landed helicopter", g_p.in_car==hi+1 && h->state==ST_DRIVE, b);

  // Fly: push the stick forward for three seconds.
  int32_t sx=g_p.x, sy=g_p.y;
  game_touch_down(STICK_HOME_X,STICK_HOME_Y); game_touch_move(STICK_HOME_X,(int16_t)(STICK_HOME_Y-STICK_RANGE));
  int maxz=0; for(int f=0;f<75;f++){ g_p.hp=100; game_step(); if(h->z>maxz) maxz=h->z; }
  int32_t flown=wr_dist(g_p.x-sx,g_p.y-sy);
  snprintf(b,sizeof b,"climbed to z=%d, flew %d units (%d tiles), spd %d",maxz,(int)flown,(int)(flown/TILE),g_p.spd);
  ck("it flies", maxz>=HELI_ALT-20 && flown>4*TILE, b);

  // Guns: hold SELECT for a second while flying and count rounds.
  int rounds=0; game_input_hold(true);
  for(int f=0;f<25;f++){ game_step(); for(int i=0;i<MAX_BULLET;i++) if(g_bullet[i].life && g_bullet[i].kind==B_SLUG && !(g_bullet[i].from_cop&1)) rounds++; }
  game_input_hold(false);
  snprintf(b,sizeof b,"%d slug-frames in a second of held fire",rounds);
  ck("the chain gun fires while SELECT is held", rounds>10, b);
  weapon_next();
  for(int f=0;f<6;f++) game_step();         // let the gun's cooldown clear
  int rk=h->wep; game_input_tap(); game_step();
  snprintf(b,sizeof b,"rockets %d -> %d",rk,h->wep);
  ck("rockets fire one per press", h->wep==rk-1, b);

  // Police fire cannot reach you up here.
  g_p.hp=100; g_p.armor=0;
  for(int i=0;i<MAX_BULLET;i++){ Bullet*bl=&g_bullet[i]; bl->x=g_p.x; bl->y=g_p.y; bl->vx=0; bl->vy=0; bl->life=3; bl->dmg=30; bl->from_cop=1; bl->kind=B_SLUG; }
  for(int f=0;f<4;f++) game_step();
  snprintf(b,sizeof b,"hp %d after a volley at your position",g_p.hp);
  ck("ground fire misses an aircraft", g_p.hp==100, b);

  // LAND in the weapon box brings it down; on the ground the hold gets you out.
  game_touch_up(); g_p.heli_wep = 2; g_p.attack_cd = 0; game_input_tap();   // select LAND, then fire it
  int f2=0; for(;f2<400 && h->z>0;f2++) game_step();
  snprintf(b,sizeof b,"z=%d after %d frames with LAND selected",h->z,f2);
  ck("LAND brings it down", h->z==0, b);
  g_p.spd=0; game_input_action();
  snprintf(b,sizeof b,"in_car=%d heli state=%d",g_p.in_car,h->state);
  ck("the hold on the ground gets you out", g_p.in_car==0 && h->state==ST_PARK, b);
  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
