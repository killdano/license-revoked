// The national guard: at five stars with enough officers dead, air support
// stands down and a tank comes with soldiers; the tank fires shells and is
// yours to take - with military armour, an MG and shells.
#include "wr.h"
#include "wr_domain.h"
#include <stdio.h>
#include <string.h>
static int fails;
static void ck(const char *w, bool ok, const char *d){ printf("  %-56s %s  %s\n",w,ok?"ok  ":"FAIL",d); if(!ok) fails++; }
int main(void){ char b[160]; setvbuf(stdout,NULL,_IONBF,0);
  wr_seed(6); game_init(); { int32_t x=ISLAND_CX*TILE,y=ISLAND_CY*TILE; uint16_t a; road_snap(&x,&y,&a); g_p.x=x; g_p.y=y; g_p.ang=a; }
  domain_scenery(); memset(g_ent,0,sizeof g_ent); g_p.hp=150; g_p.wanted=4; g_p.heat=330; g_p.hostile=1; g_p.hot=1000;
  // The real way: officers die one at a time, with the helicopter overhead.
  int killed=0, heli_seen=0; for(int f=0;f<1200 && g_p.tier<2;f++){ game_step();
    for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_HELI) heli_seen=1;
    if(f%60==0){ for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_COP&&g_ent[i].state!=ST_DEAD){ kill_entity(&g_ent[i]); killed++; break; } } }
  snprintf(b,sizeof b,"killed %d officers one by one (helicopter seen=%d): tier=%d copkills=%d",killed,heli_seen,g_p.tier,g_p.copkills);
  ck("killing officers one at a time, through the lulls, reaches the guard", g_p.tier==2 && heli_seen, b);
  int tank=-1, soldiers=0, helis=0; for(int f=0;f<900;f++){ g_p.hp=150; g_p.dead=0; game_step(); /* kept alive: the guard is deadly */ for(int i=0;i<MAX_ENT;i++){ if(g_ent[i].type==ENT_COPCAR&&g_ent[i].sub==CAR_TANK&&tank<0) tank=i; if(g_ent[i].type==ENT_HELI && f>100) helis++; } }
  for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_COP&&(uint8_t)g_ent[i].aux==PA_GUARD&&g_ent[i].state!=ST_DEAD) soldiers++;
  snprintf(b,sizeof b,"tier=%d tank=%d soldiers=%d helicopter-frames=%d",g_p.tier,tank,soldiers,helis);
  ck("then the guard - a tank, soldiers, and the helicopter gone", g_p.tier==2 && tank>=0 && soldiers>=1 && helis==0, b);
  int shells=0; if(tank>=0){ for(int f=0;f<200;f++){ g_p.hp=150; g_p.dead=0; g_ent[tank].x=g_p.x+4*TILE; g_ent[tank].y=g_p.y; game_step(); for(int i=0;i<MAX_BULLET;i++) if(g_bullet[i].life&&g_bullet[i].kind==B_ROCKET&&(g_bullet[i].from_cop&B_FROM_COP)) { shells++; break; } } }
  snprintf(b,sizeof b,"shell-frames in 8 seconds=%d",shells);
  ck("the tank fires shells at you", shells>0, b);
  if(tank>=0){ for(int i=0;i<MAX_ENT;i++) if(i!=tank) g_ent[i].type=ENT_NONE;   // just you and the tank
    for(int i=0;i<MAX_BULLET;i++) g_bullet[i].life=0; g_p.hp=150; g_p.dead=0; g_p.knock=0; g_p.armor=0; g_ent[tank].x=g_p.x+100; g_ent[tank].y=g_p.y; g_ent[tank].spd=0; g_p.armor=0; game_step(); game_input_hold(true); for(int f=0;f<12;f++){ g_ent[tank].spd=0; game_step(); } game_input_hold(false); }
  snprintf(b,sizeof b,"in_car=%d (tank slot %d) armor=%d",g_p.in_car,tank+1,g_p.armor);
  ck("stealing the tank gives military armour", g_p.in_car==tank+1 && g_p.armor==ARMOR_MIL, b);
  int mg=0, shell=0; g_p.heli_wep=0; g_p.attack_cd=0; game_input_tap(); game_step(); for(int i=0;i<MAX_BULLET;i++) if(g_bullet[i].life&&g_bullet[i].kind==B_SLUG&&g_bullet[i].from_cop==0) mg=1;
  weapon_next(); g_p.attack_cd=0; game_input_tap(); game_step(); for(int i=0;i<MAX_BULLET;i++) if(g_bullet[i].life&&g_bullet[i].kind==B_ROCKET&&g_bullet[i].from_cop==0) shell=1;
  snprintf(b,sizeof b,"mg=%d shell=%d",mg,shell);
  ck("its MG and its shells are yours", mg && shell, b);
  printf("%d failure(s)\n",fails); return fails?1:0; }
