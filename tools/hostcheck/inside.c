// The economy: money off bodies, the club, the store.
#include "wr.h"
#include <stdio.h>
#include <string.h>
static int fails;
static void ck(const char *n, bool ok, const char *d){ printf("  %-44s %s   %s\n",n,ok?"ok  ":"FAIL",d); if(!ok) fails++; }
static int count_role(uint8_t role, int *undress){ int n=0; if(undress)*undress=0; for(int i=0;i<MAX_ENT;i++){ Ent*e=&g_ent[i]; if(e->type==ENT_NONE||e->state==ST_DEAD) continue; if((uint8_t)e->aux==role){ n++; if(undress && ((e->aux>>8)&3)>*undress) *undress=(e->aux>>8)&3; } } return n; }
static void throw_cash(int n){ for(int k=0;k<n;k++){ g_p.attack_cd=0; game_input_action(); game_step(); } for(int f=0;f<40;f++) game_step(); }
int main(void){
  char b[160];
  wr_seed(6); audio_init(); game_init();

  // --- money off a body -----------------------------------------------
  int vi=-1; for(int w=0;w<300&&vi<0;w++){ game_step(); for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_PED&&g_ent[i].state!=ST_DEAD){vi=i;break;} }
  if(vi<0){ printf("no pedestrian to shoot\n"); return 1; }
  g_ent[vi].type=ENT_COP; g_ent[vi].hp=1; g_p.x=g_ent[vi].x-100; g_p.y=g_ent[vi].y; g_p.ang=0; g_p.weapon=W_PISTOL; g_p.ammo=20; g_p.attack_cd=0;
  game_input_action(); for(int f=0;f<80;f++) game_step();
  int cashpk=0; for(int i=0;i<MAX_PICKUP;i++) if(g_pickup[i].alive&&g_pickup[i].weapon==PICKUP_CASH) cashpk++;
  snprintf(b,sizeof b,"%d cash pickups around the body (was cop: always some)",cashpk);
  ck("bodies drop money", g_ent[vi].state==ST_DEAD && cashpk>0, b);
  uint16_t before=g_p.cash; for(int i=0;i<MAX_PICKUP;i++) if(g_pickup[i].alive&&g_pickup[i].weapon==PICKUP_CASH){ g_p.x=g_pickup[i].x; g_p.y=g_pickup[i].y; game_step(); }
  snprintf(b,sizeof b,"cash %d -> %d",before,g_p.cash);
  ck("walking over it pays", g_p.cash>before, b);

  // --- the club ----------------------------------------------------------
  g_p.cash=1000; g_p.wanted=0; g_p.heat=0;
  int32_t doorx=g_p.x, doory=g_p.y;
  inside_enter(IN_CLUB, doorx, doory, 0, 1);
  int patrons=count_role(PA_PATRON,NULL), bouncers=count_role(PA_BOUNCER,NULL), dancers=count_role(PA_DANCER,NULL);
  snprintf(b,sizeof b,"weapon=%d (cash=%d) patrons=%d bouncers=%d dancers=%d",g_p.weapon,W_CASH,patrons,bouncers,dancers);
  ck("the club is open", g_p.inside==IN_CLUB && g_p.weapon==W_CASH && patrons>=3 && bouncers==2 && dancers>=1, b);
  // walk up near the stage and throw a little
  g_p.x=ROOM_OX+g_in.w/2; g_p.y=ROOM_OY+TILE+TILE/2+60; g_p.ang=49152;
  throw_cash(5);
  int ud; count_role(PA_DANCER,&ud);
  snprintf(b,sizeof b,"$50: meter=%d level=%d undress=%d cash=%d",g_in.meter,g_in.level,ud,g_p.cash);
  ck("a little money gets a little show", g_in.level==1 && ud==1 && g_p.cash==950, b);
  throw_cash(20);
  count_role(PA_DANCER,&ud);
  snprintf(b,sizeof b,"$250 total: meter=%d level=%d undress=%d pinned=%d dancers=%d",g_in.meter,g_in.level,ud,g_in.pinned,count_role(PA_DANCER,NULL));
  ck("a lot of money: everything off, over to your chair", g_in.level>=3 && ud==2 && g_in.pinned, b);
  int32_t px=g_p.x; g_p.stick_f=256; g_p.stick_on=1; for(int f=0;f<30;f++) game_step(); g_p.stick_on=0; g_p.stick_f=0;
  snprintf(b,sizeof b,"moved %d while pinned",(int)wr_abs(g_p.x-px));
  ck("you stay in the chair", wr_abs(g_p.x-px)<8, b);
  // the show runs down by itself
  for(int f=0;f<3000&&g_in.level>0;f++) game_step();
  snprintf(b,sizeof b,"meter=%d level=%d pinned=%d",g_in.meter,g_in.level,g_in.pinned);
  ck("...and it ends when the money stops", g_in.level==0 && !g_in.pinned, b);
  // trouble: fire a gun
  g_p.weapon=W_PISTOL; g_p.ammo=10; g_p.attack_cd=0; game_input_action();
  bool corner=true; int inside_frames=0;
  for(int f=0;f<600 && g_p.inside;f++){ game_step(); inside_frames++; }
  int ud2=0; 
  snprintf(b,sizeof b,"angry=%d banned=%02x out after %d frames, inside=%d",g_in.angry,g_in.banned,inside_frames,g_p.inside);
  ck("a shot gets you walked out", !g_p.inside && g_in.banned, b);
  inside_enter(IN_CLUB, doorx, doory, 0, 1);
  snprintf(b,sizeof b,"angry on entry=%d",g_in.angry);
  ck("...and remembered", g_in.angry==1, b);
  inside_exit();

  // --- the store -----------------------------------------------------------
  g_p.cash=1000; g_p.heat=0; g_p.wanted=0;
  (g_p.x=ISLAND_CX*TILE, g_p.y=ISLAND_CY*TILE), inside_enter(IN_GUNSHOP, doorx, doory, 0, 1);
  int cab=0; for(int i=0;i<MAX_PROP;i++) if(g_prop[i].alive&&g_prop[i].kind==PROP_CABINET) cab++;
  snprintf(b,sizeof b,"clerk=%d customers=%d cabinets=%d room %dx%d",count_role(PA_CLERK,NULL),count_role(PA_CUSTOMER,NULL),cab,(int)(g_in.w/TILE),(int)(g_in.h/TILE));
  ck("the store is open", g_p.inside==IN_GUNSHOP && count_role(PA_CLERK,NULL)>=1 && cab>=4, b);
  throw_cash(7);
  int pk=0, kind=-1; for(int i=0;i<MAX_PICKUP;i++) if(g_pickup[i].alive&&g_pickup[i].weapon<W_COUNT){ pk++; kind=g_pickup[i].weapon; }
  snprintf(b,sizeof b,"credit=%d unlocked=%02x pickups=%d (weapon %d)",g_in.credit,g_in.unlocked,pk,kind);
  ck("$70 opens the pistol cabinet", g_in.credit>=60 && (g_in.unlocked&1) && pk==1 && kind==W_PISTOL, b);
  throw_cash(9);
  pk=0; for(int i=0;i<MAX_PICKUP;i++) if(g_pickup[i].alive&&g_pickup[i].weapon<W_COUNT) pk++;
  snprintf(b,sizeof b,"credit=%d unlocked=%02x pickups=%d alarm=%d wanted=%d",g_in.credit,g_in.unlocked,pk,g_in.alarm,g_p.wanted);
  ck("$160 opens the shotgun (and the vest, grenades), no alarm", (g_in.unlocked&2) && pk>=2 && !g_in.alarm && g_p.wanted==0, b);
  // now smash a cabinet
  for(int i=0;i<MAX_PROP;i++) if(g_prop[i].alive&&g_prop[i].kind==PROP_CABINET&&g_prop[i].spin==3){ prop_hit(i,0,0,90); break; }
  int gunmen=0; for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_COP&&((uint8_t)g_ent[i].aux==PA_CLERK||(uint8_t)g_ent[i].aux==PA_CUSTOMER)) gunmen++;
  game_step();
  snprintf(b,sizeof b,"alarm=%d staff armed=%d heat=%d wanted=%d",g_in.alarm,gunmen,g_p.heat,g_p.wanted);
  ck("breaking the glass: alarm, staff draw, police called", g_in.alarm && gunmen>=3 && g_p.wanted>=3, b);
  uint8_t took=g_in.took[g_in.venue];
  inside_exit();
  (g_p.x=ISLAND_CX*TILE, g_p.y=ISLAND_CY*TILE), inside_enter(IN_GUNSHOP, doorx, doory, 0, 1);
  snprintf(b,sizeof b,"took mask=%02x still %02x, credit reset=%d",took,g_in.took[g_in.venue],g_in.credit);
  ck("stolen stock never comes back", took==g_in.took[g_in.venue] && (took&4), b);
  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
