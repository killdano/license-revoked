// The airport, room by room, and both ways out of the cabin.
#include "wr.h"
#include "wr_domain.h"
#include <stdio.h>
#include <string.h>
static int fails;
static void ck(const char *w, bool ok, const char *d){ printf("  %-48s %s  %s\n",w,ok?"ok  ":"FAIL",d); if(!ok) fails++; }
static void walk_to(int32_t x,int32_t y){ g_p.x=x; g_p.y=y; g_p.spd=0; }
static Prop* find_prop(uint8_t kind,uint8_t spin){ for(int i=0;i<MAX_PROP;i++) if(g_prop[i].alive&&g_prop[i].kind==kind&&g_prop[i].spin==spin) return &g_prop[i]; return NULL; }
int main(void){ char b[160]; setvbuf(stdout,NULL,_IONBF,0);
  wr_seed(6); game_init();
  g_p.x=(ISLAND_GX+2)*TILE; g_p.y=(ISLAND_GY+2)*TILE; g_p.cash=200; g_p.weapon=W_PISTOL; g_p.ammo=10;
  domain_scenery();
  inside_enter(IN_AIRPORT, g_p.x, g_p.y, 0, 1);
  snprintf(b,sizeof b,"inside=%d stage=%d domain=%d",g_p.inside,g_in.stage,domain_current());
  ck("the airport door opens on ticketing", g_p.inside==IN_AIRPORT && g_in.stage==0 && domain_current()==DOM_AIRPORT, b);
  // through the back door without a ticket: refused
  Prop *d=find_prop(PROP_DOOR,1); walk_to(d->x,d->y+40); game_input_tap();
  ck("no ticket, no passage", g_in.stage==0, "");
  // $50 on the counter
  Prop *c=find_prop(PROP_COUNTER,0); walk_to(c->x,c->y+60); g_p.ang=49152;
  for(int k=0;k<6;k++){ g_p.attack_cd=0; game_input_action(); game_step(); }
  for(int f=0;f<60;f++) game_step();
  snprintf(b,sizeof b,"credit=%d ticket=%d cash=%d",g_in.credit,g_in.ticket,g_p.cash);
  ck("$50 on the counter is a ticket", g_in.ticket==1, b);
  d=find_prop(PROP_DOOR,1); walk_to(d->x,d->y+40); game_input_tap();
  ck("with a ticket you are through to security", g_in.stage==1, "");
  // the guard stops you with a pistol; the bin fixes that
  d=find_prop(PROP_DOOR,1); walk_to(d->x,d->y+40); game_input_tap();
  snprintf(b,sizeof b,"stage=%d weapon=%d",g_in.stage,g_p.weapon);
  ck("armed, the gate refuses you", g_in.stage==1, b);
  Prop *bin=find_prop(PROP_CABINET,0); walk_to(bin->x+60,bin->y); game_input_tap();
  snprintf(b,sizeof b,"weapon=%d pistol ammo left=%d",g_p.weapon,g_p.inv[W_PISTOL]);
  ck("the bin takes your guns", g_p.inv[W_PISTOL]==0, b);   // (indoors you hold the money gun)
  d=find_prop(PROP_DOOR,1); walk_to(d->x,d->y+40); game_input_tap();
  snprintf(b,sizeof b,"stage=%d domain=%d (airside=%d)",g_in.stage,domain_current(),DOM_AIRSIDE);
  ck("unarmed, through to the terminal - the airside tenant", g_in.stage==2 && domain_current()==DOM_AIRSIDE, b);
  // the gate stays shut until boarding is called
  d=find_prop(PROP_DOOR,1); walk_to(d->x,d->y+40); game_input_tap();
  ck("the gate is shut until boarding", g_in.stage==2, "");
  for(int f=0;f<260;f++) game_step();
  walk_to(d->x,d->y+40); game_input_tap();
  ck("boarding called, into the cabin", g_in.stage==3, "");
  // A: sit, and the flight lands you elsewhere
  walk_to(ROOM_OX+g_in.w/2-100,ROOM_OY+700); game_input_tap();   // along the rows: the nearest seat
  ck("a seat seats you", g_in.pinned==1, "");
  for(int f=0;f<320;f++) game_step();
  int isl=island_at(g_p.x,g_p.y);
  snprintf(b,sizeof b,"inside=%d island=%d at (%d,%d) dead=%d (boarded on %d)",g_p.inside,isl,(int)g_p.x,(int)g_p.y,g_p.dead,ISL_GHETTO);
  ck("the flight lands you on the other island", !g_p.inside && isl==ISL_COAST, b);
  // B: the hijack. Through the rooms again, quickly.
  g_p.x=ISLAND_GX*TILE; g_p.y=ISLAND_GY*TILE; domain_scenery(); g_p.cash=200;
  inside_enter(IN_AIRPORT, g_p.x, g_p.y, 0, 1);
  c=find_prop(PROP_COUNTER,0); walk_to(c->x,c->y+60); g_p.ang=49152;
  for(int k=0;k<6;k++){ g_p.attack_cd=0; game_input_action(); game_step(); }
  for(int f=0;f<60;f++) game_step();
  d=find_prop(PROP_DOOR,1); walk_to(d->x,d->y+40); game_input_tap();
  d=find_prop(PROP_DOOR,1); walk_to(d->x,d->y+40); game_input_tap();
  for(int f=0;f<260;f++) game_step();
  d=find_prop(PROP_DOOR,1); walk_to(d->x,d->y+40); game_input_tap();
  ck("back in a cabin", g_in.stage==3 && g_p.inside, "");
  // punch through the cockpit door
  Prop *cd=find_prop(PROP_COUNTER,2); walk_to(cd->x,cd->y+60); g_p.ang=49152; g_p.weapon=W_BAT;
  for(int k=0;k<12 && !cd->loose;k++){ g_p.attack_cd=0; game_input_tap(); game_step(); }
  snprintf(b,sizeof b,"cockpit door loose=%d",cd->loose);
  ck("the cockpit door gives", cd->loose, b);
  snprintf(b,sizeof b,"cabin %dx%d units (%d.%d tiles wide)",g_in.w,g_in.h,g_in.w/TILE,(g_in.w%TILE)*10/TILE);
  ck("the cabin is a fuselage: narrow and long", g_in.w<2*TILE && g_in.h>=5*TILE, b);
  // the pilots
  for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_PED&&(uint8_t)g_ent[i].aux==PA_CLERK){ g_ent[i].state=ST_DEAD; }
  walk_to(ROOM_OX+g_in.w/2, ROOM_OY+30); game_input_tap();
  int plane=-1; for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_HELI&&g_ent[i].sub==1) plane=i;
  snprintf(b,sizeof b,"inside=%d in_car=%d plane=%d threat domain=%d",g_p.inside,g_p.in_car,plane,0);
  ck("the controls: out of the room and into the air", !g_p.inside && g_p.in_car && plane>=0, b);
  for(int f=0;f<60;f++){ game_touch_down(STICK_HOME_X,STICK_HOME_Y); game_touch_move(STICK_HOME_X,(int16_t)(STICK_HOME_Y-STICK_RANGE)); game_step(); }
  snprintf(b,sizeof b,"z=%d spd=%d",plane>=0?g_ent[plane].z:-1,g_p.spd);
  ck("and flying", plane>=0 && g_ent[plane].z>100 && g_p.spd>=16, b);
  printf("%d failure(s)\n",fails); return fails?1:0; }
