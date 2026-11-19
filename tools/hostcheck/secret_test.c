// The secret island: terrain, no police, the airliner's passengers staying,
// the house and the bed, the rescue from the club, and a population that
// survives death.
#include "wr.h"
#include "wr_domain.h"
#include <stdio.h>
#include <string.h>
static int fails;
static void ck(const char *w, bool ok, const char *d){ printf("  %-58s %s  %s\n",w,ok?"ok  ":"FAIL",d); if(!ok) fails++; }
static Prop* fp(uint8_t k,uint8_t s){ for(int i=0;i<MAX_PROP;i++) if(g_prop[i].alive&&g_prop[i].kind==k&&g_prop[i].spin==s) return &g_prop[i]; return NULL; }
int main(void){ char b[160]; setvbuf(stdout,NULL,_IONBF,0);
  wr_seed(6); game_init();
  int32_t sx=ISLAND_SX*TILE, sy=ISLAND_SY*TILE;
  snprintf(b,sizeof b,"centre=%d runway=%d house=%d sea between=%d bridge=%d",cell_at_point(sx,sy),cell_at_point(sx,sy-2*TILE),cell_at_point(sx,sy-4*TILE),cell_at_point((sx+ISLAND_GX*TILE)/2,(sy+ISLAND_GY*TILE)/2),bridge_find(sx-4*TILE,sy,NULL,NULL));
  ck("an island of lawn with a runway and a house, water all round, no bridge", cell_at_point(sx,sy)==CELL_PARK && cell_at_point(sx,sy-2*TILE)==CELL_ROAD && cell_at_point(sx,sy-4*TILE)==CELL_BLD && cell_at_point((sx+ISLAND_GX*TILE)/2,(sy+ISLAND_GY*TILE)/2)==CELL_WATER && bridge_find(sx-4*TILE,sy,NULL,NULL)<0, b);
  // fly the airliner there and land it
  g_p.x=ISLAND_GX*TILE; g_p.y=ISLAND_GY*TILE; domain_scenery(); memset(g_ent,0,sizeof g_ent);
  g_p.residents=0; domain_air_spawn_plane(); game_step();
  Ent*pl=&g_ent[g_p.in_car-1]; pl->x=sx-4*TILE; pl->y=sy-2*TILE; pl->z=300; g_p.x=pl->x; g_p.y=pl->y; g_p.ang=0;
  for(int f=0;f<5;f++) game_step(); domain_scenery();
  g_p.heli_wep=2; g_p.attack_cd=0; game_input_tap(); for(int f=0;f<200;f++) game_step();
  snprintf(b,sizeof b,"plane z=%d island=%d residents=%d caption=%d",pl->z,island_at(g_p.x,g_p.y),g_p.residents,g_p.caption);
  ck("land the airliner on the island: the passengers get off and stay", pl->z==0 && g_p.residents==6, b);
  // no police, ever
  g_p.wanted=3; g_p.heat=250; g_p.hostile=1; int cops=0; for(int f=0;f<300;f++){ game_step(); for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_COP||g_ent[i].type==ENT_COPCAR) cops++; }
  snprintf(b,sizeof b,"police unit-frames at three stars on the island=%d threat=%d(island=%d)",cops,domain2_current(),DOM_ISLAND);
  ck("no police on the island, whatever the stars", cops==0 && domain2_current()==DOM_ISLAND, b);
  int res=0; for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_PED) res++;
  snprintf(b,sizeof b,"people on the lawn=%d",res);
  ck("the residents are on the lawn", res>=6, b);
  // out of the plane, into the house, to the bed
  g_p.wanted=0; g_p.heat=0; g_p.hostile=0; game_input_hold(true); for(int f=0;f<12;f++) game_step(); game_input_hold(false);
  city_refresh(g_p.x,g_p.y); int li=-1; for(int i=0;i<g_nlot;i++) if(lot_is_home(&g_lot[i])) li=i;
  snprintf(b,sizeof b,"in_car=%d house lot=%d",g_p.in_car,li);
  ck("out of the plane; the house is a lot on the island", !g_p.in_car && li>=0, b);
  int32_t dx,dy,nx,ny; city_shop_door(&g_lot[li],&dx,&dy,&nx,&ny); g_p.x=dx; g_p.y=dy; game_step(); game_input_tap(); for(int f=0;f<3;f++) game_step();
  int wife=0; for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_PED&&(uint8_t)g_ent[i].aux==PA_CLERK) wife=1;
  snprintf(b,sizeof b,"inside=%d (home=%d) wife=%d",g_p.inside,IN_HOME,wife);
  ck("through the door: the safe house, and your wife is there", g_p.inside==IN_HOME && wife, b);
  g_p.hp=40; g_p.x=ROOM_OX+g_in.w/2; g_p.y=ROOM_OY+160; game_step(); game_input_tap(); int black=g_in.wait>0; for(int f=0;f<60;f++) game_step();
  snprintf(b,sizeof b,"faded=%d hp after=%d",black,g_p.hp);
  ck("the bed: a fade to black, and full health", black && g_p.hp==HP_MAX, b);
  // the rescue: from the club, carried here
  g_p.x=ISLAND_CX*TILE; g_p.y=ISLAND_CY*TILE; g_p.inside=0; inside_enter(IN_CLUB,g_p.x,g_p.y,0,1);
  for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_PED&&(uint8_t)g_ent[i].aux==PA_BOUNCER) g_ent[i].state=ST_DEAD;
  int dn=-1; for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_PED&&(uint8_t)g_ent[i].aux==PA_DANCER){ dn=i; break; }
  g_p.x=g_ent[dn].x+40; g_p.y=g_ent[dn].y; game_step(); game_input_tap();
  snprintf(b,sizeof b,"carry=%d caption=%d",g_p.carry,g_p.caption);
  ck("with the bouncers down, a press beside a dancer picks her up", g_p.carry==1, b);
  inside_exit(); g_p.x=sx; g_p.y=sy-TILE; g_p.in_car=0; domain_scenery(); int r0=g_p.residents; for(int f=0;f<5;f++) game_step();
  snprintf(b,sizeof b,"carry=%d residents %d -> %d",g_p.carry,r0,g_p.residents);
  ck("at the house she is safe: one more resident, and your arms are free", !g_p.carry && g_p.residents==r0+1, b);
  // death costs one
  r0=g_p.residents; g_p.hp=1; g_p.armor=0; player_absorb(50); for(int f=0;f<200;f++){ if(g_p.dead){ g_p.cine=100; game_input_tap(); } game_step(); if(!g_p.dead) break; }
  snprintf(b,sizeof b,"residents %d -> %d",r0,g_p.residents);
  ck("the population survives your death, one fewer", g_p.residents==r0-1, b);
  printf("%d failure(s)\n",fails); return fails?1:0; }
