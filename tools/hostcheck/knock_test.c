// Hit by a car on foot: you fly, bounce, bleed and get up; cars are solid
// on foot at their real size; the club's money lands on the stage and
// taking it gets you thrown out.
#include "wr.h"
#include "wr_domain.h"
#include <stdio.h>
#include <string.h>
static int fails;
static void ck(const char *w, bool ok, const char *d){ printf("  %-54s %s  %s\n",w,ok?"ok  ":"FAIL",d); if(!ok) fails++; }
int main(void){ char b[160]; setvbuf(stdout,NULL,_IONBF,0);
  wr_seed(6); game_init(); g_p.x=ISLAND_CX*TILE; g_p.y=ISLAND_CY*TILE; { int32_t x=g_p.x,y=g_p.y; uint16_t a; road_snap(&x,&y,&a); g_p.x=x; g_p.y=y; g_p.ang=a; }
  domain_scenery(); memset(g_ent,0,sizeof g_ent); g_p.hp=150; g_p.armor=0; g_p.wanted=3; g_p.heat=250; g_p.hostile=1; game_step();   // wanted and hostile: the car rams
  // a police car coming at you fast from behind
  Ent*c=&g_ent[0]; c->type=ENT_COPCAR; c->hp=100; c->state=ST_CHASE; c->sub=CAR_SEDAN; c->spd=28; c->ang=g_p.ang;   // a real chase car, driven by the AI
  c->x=g_p.x-fmul(300,fcos(g_p.ang)); c->y=g_p.y-fmul(300,fsin(g_p.ang));
  int knocked=0, peak=0, bloods=0, frames=0, gotup=-1; int32_t startx=g_p.x, starty=g_p.y; int hp0=g_p.hp;
  for(int f=0;f<160;f++){
    game_step(); if(g_p.knock){ if(!knocked){ memset(g_ent,0,sizeof g_ent); g_p.wanted=0; g_p.heat=0; g_p.hostile=0; }   // one hit: everyone else is gone after it
      knocked=1; frames++; if(g_p.kz>peak) peak=g_p.kz; } else if(knocked&&gotup<0) gotup=f;
    for(int i=0;i<MAX_GIB;i++) if(g_gib[i].life) { bloods++; break; } }
  snprintf(b,sizeof b,"knocked=%d airborne %d frames, peak z=%d, thrown %d units, hp %d->%d, blood frames=%d, up at f=%d",knocked,frames,peak,(int)wr_dist(g_p.x-startx,g_p.y-starty),hp0,g_p.hp,bloods,gotup);
  ck("a fast car knocks you flying, bouncing and bleeding, and you get up", knocked && peak>40 && frames>8 && gotup>0 && bloods>0, b);
  // solid at its real size: a van's box is bigger than a sedan's
  memset(g_ent,0,sizeof g_ent); g_p.knock=0; g_p.hp=150; g_p.dead=0; Ent*v=&g_ent[0]; v->type=ENT_CAR; v->sub=CAR_VAN; v->state=ST_PARK; v->hp=100; v->ang=0;
  v->x=g_p.x+180; v->y=g_p.y; int32_t hl,hw; car_half_dims(v,&hl,&hw);
  g_p.ang=0; game_touch_down(STICK_HOME_X,STICK_HOME_Y); game_touch_move(STICK_HOME_X,(int16_t)(STICK_HOME_Y-STICK_RANGE)); // walk forward (+x) into the van's side
  int32_t bx=g_p.x; for(int f=0;f<60;f++) game_step();
  snprintf(b,sizeof b,"van half-length=%d; walked from %d to %d (van at %d)",(int)hl,(int)bx,(int)g_p.x,(int)v->x);
  ck("a parked van is solid at its own size", g_p.x < v->x - hl + 12 && g_p.x > bx, b);
  game_touch_up();
  // the club: money on the stage, and what taking it costs
  g_p.hp=150; g_p.dead=0; g_p.knock=0;
  inside_enter(IN_CLUB,ISLAND_CX*TILE,ISLAND_CY*TILE,0,1); g_p.cash=0;
  for(int f=0;f<300;f++){ if(f==5||f==100) for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_PED&&(uint8_t)g_ent[i].aux==PA_PATRON){ g_ent[i].vx=(int16_t)(g_ent[i].x>>2); g_ent[i].vy=(int16_t)(g_ent[i].y>>2); g_ent[i].state=ST_WALK; }   // send the patrons up
    game_step(); }
  int onstage=0,total=0; for(int i=0;i<MAX_PICKUP;i++) if(g_pickup[i].alive&&g_pickup[i].weapon==PICKUP_CASH){ total++; if(wr_abs(g_pickup[i].x-(ROOM_OX+g_in.w/2))<200&&wr_abs(g_pickup[i].y-(ROOM_OY+TILE/2+20))<130) onstage++; }
  snprintf(b,sizeof b,"cash pickups=%d on the stage=%d",total,onstage);
  ck("the patrons' money lands on the stage", total>0 && onstage*10>=total*6, b);
  g_p.x=ROOM_OX+g_in.w/2; g_p.y=ROOM_OY+TILE/2+20; for(int f=0;f<40;f++) game_step();
  snprintf(b,sizeof b,"cash=%d angry=%d",g_p.cash,g_in.angry);
  ck("taking it makes the house angry", g_p.cash>0 && g_in.angry, b);
  printf("%d failure(s)\n",fails); return fails?1:0; }
