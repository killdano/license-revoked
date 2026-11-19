// The ghetto's militia: they exist, they fight the police, they leave you alone.
#include "wr.h"
#include "wr_domain.h"
#include <stdio.h>
#include <string.h>
static int fails;
static void ck(const char *w, bool ok, const char *d){ printf("  %-46s %s  %s\n",w,ok?"ok  ":"FAIL",d); if(!ok) fails++; }
int main(void){ char b[160];
  wr_seed(6); game_init();
  g_p.x=ISLAND_GX*TILE; g_p.y=ISLAND_GY*TILE; int32_t x=g_p.x,y=g_p.y; uint16_t a; road_snap(&x,&y,&a); g_p.x=x; g_p.y=y;
  memset(g_ent,0,sizeof g_ent); domain_scenery();
  // The odds, tested directly: hand the domain 200 fresh pedestrians.
  int mil=0; Ent probe; memset(&probe,0,sizeof probe); probe.type=ENT_PED; probe.hp=100;
  Ent save=g_ent[0];
  for(int k=0;k<200;k++){ g_ent[0]=probe; domain_call(DOP_SPAWN,0); if((uint8_t)g_ent[0].aux==PA_MILITIA) mil++; }
  g_ent[0]=save;
  int cops=0; for(int f=0;f<400;f++) game_step();
  for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_COP&&g_ent[i].state!=ST_DEAD) cops++;
  snprintf(b,sizeof b,"%d of 200 fresh pedestrians made militia (%d%%), police on the street=%d",mil,mil/2,cops);
  ck("about a third of the street is militia, with the police", mil>=50 && mil<=90 && cops>=1, b);
  // do they fight? count police hurt over a window with nobody wanted
  g_p.wanted=0; g_p.heat=0; int hurt=0, milfire=0;
  for(int f=0;f<600;f++){ game_step(); for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_COP&&g_ent[i].hp<100) hurt++;
    for(int i=0;i<MAX_BULLET;i++) if(g_bullet[i].life && (g_bullet[i].from_cop&B_FROM_MILITIA)) milfire++; }
  snprintf(b,sizeof b,"militia rounds in the air (frame-sum)=%d, police-hurt frames=%d",milfire,hurt);
  ck("they fire on the police", milfire>0, b);
  ck("and hurt them", hurt>0, b);
  // they leave you alone unless you started it
  int hitme=0; g_p.hp=150; g_p.armor=0;
  for(int f=0;f<300;f++){ game_step(); }
  snprintf(b,sizeof b,"hp after 300 frames among them: %d, militia_hostile=%d",g_p.hp,g_p.militia_hostile);
  ck("they ignore you", g_p.hp>=120 && !g_p.militia_hostile, b);
  printf("%d failure(s)\n",fails); return fails?1:0; }
