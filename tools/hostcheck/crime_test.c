// Blowing up a car is a crime; death halves the store; no nightstick at
// zero stars; the bat kills in one and dents cars; the ambulance comes.
#include "wr.h"
#include "wr_domain.h"
#include "wr_tune.h"
#include <stdio.h>
#include <string.h>
static int fails;
static void ck(const char *w, bool ok, const char *d){ printf("  %-54s %s  %s\n",w,ok?"ok  ":"FAIL",d); if(!ok) fails++; }
static void road(void){ int32_t x=ISLAND_CX*TILE,y=ISLAND_CY*TILE; uint16_t a; road_snap(&x,&y,&a); g_p.x=x; g_p.y=y; g_p.ang=a; }
int main(void){ char b[160]; setvbuf(stdout,NULL,_IONBF,0);
  wr_seed(6); game_init(); road(); domain_scenery(); memset(g_ent,0,sizeof g_ent); game_step();
  // 1. a rocket into a car
  Ent*c=&g_ent[0]; c->type=ENT_CAR; c->hp=100; c->state=ST_PARK; c->sub=CAR_SEDAN; c->x=g_p.x+fmul(200,fcos(g_p.ang)); c->y=g_p.y+fmul(200,fsin(g_p.ang)); c->ang=g_p.ang;
  g_p.weapon=W_ROCKET; g_p.ammo=3; g_p.heat=0; g_p.wanted=0; game_step(); g_p.attack_cd=0; game_input_tap();
  for(int f=0;f<30;f++) game_step();
  snprintf(b,sizeof b,"car state=%d fire=%d heat=%d (a kill is %d)",c->state,c->fire,g_p.heat,HEAT_KILL);
  ck("blowing up a car is two bodies' worth of heat", (c->fire||c->state==ST_DEAD) && g_p.heat>=2*HEAT_KILL, b);
  // 2. death halves what the store owes you
  g_in.spent[0]=400; g_p.hp=1; g_p.wanted=0; g_p.heat=0; player_absorb(50); for(int f=0;f<200;f++){ if(g_p.dead){ g_p.cine=100; game_input_tap(); } game_step(); if(!g_p.dead && g_in.spent[0]!=400) break; }
  snprintf(b,sizeof b,"spent went 400 -> %d",g_in.spent[0]);
  ck("death halves what you had paid the gun store", g_in.spent[0]==200, b);
  // 3. an officer beside you at zero stars does not club you
  memset(g_ent,0,sizeof g_ent); road(); g_p.hp=150; g_p.wanted=0; g_p.heat=0; g_p.hostile=0; g_p.dead=0; g_p.knock=0;
  Ent*k=&g_ent[0]; k->type=ENT_COP; k->sub=PED_MAN; k->hp=100; k->state=ST_CHASE; k->wep=W_PISTOL; k->x=g_p.x+50; k->y=g_p.y;
  for(int f=0;f<150;f++){ k->x=g_p.x+50; k->y=g_p.y; game_step(); }
  snprintf(b,sizeof b,"hp after 6 seconds beside an officer at zero stars: %d",g_p.hp);
  ck("no heat on you, no nightstick", g_p.hp==150, b);
  // 4. the bat: one hit kills, and dents a car
  memset(g_ent,0,sizeof g_ent); g_p.weapon=W_BAT; g_p.ammo=0;
  Ent*p=&g_ent[0]; p->type=ENT_PED; p->sub=PED_MAN; p->hp=100; p->state=ST_IDLE; p->x=g_p.x+fmul(60,fcos(g_p.ang)); p->y=g_p.y+fmul(60,fsin(g_p.ang));
  game_step(); g_p.attack_cd=0; game_input_tap(); game_step();
  snprintf(b,sizeof b,"pedestrian after one swing: state=%d hp=%d",p->state,p->hp);
  ck("the bat kills in one", p->state==ST_DEAD, b);
  p->type=ENT_NONE; Ent*v=&g_ent[1]; v->type=ENT_CAR; v->hp=100; v->state=ST_PARK; v->sub=CAR_SEDAN; v->x=g_p.x+fmul(90,fcos(g_p.ang)); v->y=g_p.y+fmul(90,fsin(g_p.ang)); v->ang=g_p.ang;
  game_step(); g_p.attack_cd=0; game_input_tap(); game_step();
  snprintf(b,sizeof b,"car bodywork after a swing: %d",v->hp);
  ck("the bat dents cars", v->hp<100, b);
  // 5. the ambulance comes for a body at zero stars, and not at four
  memset(g_ent,0,sizeof g_ent); g_p.wanted=0; g_p.heat=0; g_p.hostile=0; g_p.weapon=W_FIST;
  Ent*bd=&g_ent[0]; bd->type=ENT_PED; bd->sub=PED_MAN; bd->state=ST_DEAD; bd->x=g_p.x+300; bd->y=g_p.y;
  int amb=-1; for(int f=0;f<600 && amb<0;f++){ game_step(); for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_AMB) amb=i; }
  int gone=0; if(amb>=0) for(int f=0;f<900;f++){ game_step(); if(bd->type==ENT_NONE){ gone=1; break; } }
  snprintf(b,sizeof b,"ambulance=%d body collected=%d",amb,gone);
  ck("an ambulance turns out for a body and takes it away", amb>=0 && gone, b);
  g_p.wanted=4; g_p.heat=400; for(int f=0;f<3;f++) game_step(); int still=0; for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_AMB) still=1;
  ck("at four stars there are no ambulances on the street", !still, "");
  // 6. four officers killed fast: they pull back, then come back harder
  memset(g_ent,0,sizeof g_ent); g_p.wanted=1; g_p.heat=90; g_p.hostile=1; g_p.lull=0; g_p.copkills=0; g_p.copkill_t=0;
  for(int i=0;i<4;i++){ Ent*o=&g_ent[i]; o->type=ENT_COP; o->sub=PED_MAN; o->hp=100; o->state=ST_CHASE; o->x=g_p.x+100+i*30; o->y=g_p.y; }
  for(int i=0;i<4;i++){ kill_entity(&g_ent[i]); game_step(); }
  snprintf(b,sizeof b,"lull=%d frames, caption=%d",g_p.lull,g_p.caption);
  ck("four kills in a window: they call for reinforcements", g_p.lull>0 && g_p.caption==CAP_REINF, b);
  memset(g_ent,0,sizeof g_ent);   // whoever was already out: gone, so only NEW dispatch counts
  int cops_during=0; for(int f=0;f<100;f++){ game_step(); for(int i=0;i<MAX_ENT;i++) if((g_ent[i].type==ENT_COP||g_ent[i].type==ENT_COPCAR)&&g_ent[i].state!=ST_DEAD){ cops_during++; if(cops_during<3) printf("    unit type=%d state=%d at f=%d\n",g_ent[i].type,g_ent[i].state,f); } }
  while(g_p.lull>1) game_step(); g_p.heat=100; uint16_t heat_before=g_p.heat; game_step();
  snprintf(b,sizeof b,"units dispatched during the lull=%d; heat %d -> %d when it ended",cops_during,heat_before,g_p.heat);
  ck("no dispatch during the lull, and a star more when it ends", cops_during==0 && g_p.heat>=heat_before+HEAT_PER_STAR, b);
  // 7. landing: LAND selected does nothing; fired, it comes down; landed, it does not drive
  memset(g_ent,0,sizeof g_ent); g_p.wanted=0; g_p.heat=0; g_p.dead=0; g_p.knock=0;
  Ent*h=&g_ent[0]; h->type=ENT_HELI; h->hp=200; h->state=ST_DRIVE; h->z=HELI_ALT; h->occupied=1; h->x=g_p.x; h->y=g_p.y;
  g_p.in_car=1; g_p.heli_wep=2; g_p.spd=0; for(int f=0;f<40;f++) game_step();
  int z_sel=h->z; g_p.attack_cd=0; game_input_tap(); for(int f=0;f<120;f++) game_step(); int z_fired=h->z;
  game_touch_down(STICK_HOME_X,STICK_HOME_Y); game_touch_move(STICK_HOME_X,(int16_t)(STICK_HOME_Y-STICK_RANGE)); int32_t gx=g_p.x,gy=g_p.y; for(int f=0;f<40;f++) game_step(); game_touch_up();
  int32_t moved=wr_dist(g_p.x-gx,g_p.y-gy);
  snprintf(b,sizeof b,"z with LAND selected=%d, after firing it=%d, moved on the ground with throttle=%d",z_sel,z_fired,(int)moved);
  ck("LAND must be fired; landed, the aircraft does not drive", z_sel==HELI_ALT && z_fired==0 && moved==0, b);
  weapon_next(); for(int f=0;f<30;f++) game_step();
  snprintf(b,sizeof b,"after cycling off LANDING: heli_wep=%d z=%d",g_p.heli_wep,h->z);
  ck("select anything else and it lifts off", g_p.heli_wep!=3 && h->z>0, b);
  printf("%d failure(s)\n",fails); return fails?1:0; }
