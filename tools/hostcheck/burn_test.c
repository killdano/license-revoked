#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-46s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
int main(void){
  char b[170];
  setvbuf(stdout,NULL,_IONBF,0);
  wr_seed(6); audio_init(); game_init();
  for(int i=0;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
  Ent*t=&g_ent[0]; memset(t,0,sizeof(Ent));
  t->type=ENT_PED; t->hp=100; t->state=ST_IDLE;
  t->x=g_p.x+((fcos(g_p.ang)*200)>>10);
  t->y=g_p.y+((fsin(g_p.ang)*200)>>10);
  int32_t sx=t->x, sy=t->y;

  g_p.weapon=W_FLAME; g_p.ammo=40; g_p.attack_cd=0;
  for(int k=0;k<6;k++){ g_p.attack_cd=0; game_input_action(); game_step(); }
  snprintf(b,sizeof b,"fire=%d hp=%d",t->fire,t->hp);
  ck("the flamethrower sets people alight", t->fire>0, b);

  int moved=0, died=0;
  for(int k=0;k<300 && !died;k++){
    game_step();
    for(int i=1;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
    if(wr_dist(t->x-sx,t->y-sy)>90) moved=1;
    if(t->state==ST_DEAD) died=1;
  }
  snprintf(b,sizeof b,"ran=%d died=%d hp=%d",moved,died,t->hp);
  ck("a burning man runs, then burns down", moved && died, b);

  for(int k=0;k<120;k++) game_step();
  snprintf(b,sizeof b,"fire marker=%d state=%d",t->fire,t->state);
  ck("what is left stays charred", t->fire>0 && t->state==ST_DEAD, b);
  // Fire spreads to cars, and hitting a burning car brings the bang forward.
  wr_seed(6); game_init();
  for(int i=0;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
  { int32_t tx=g_p.x, ty=g_p.y; uint16_t ta=0; road_snap(&tx,&ty,&ta);
    Ent*c=&g_ent[0]; memset(c,0,sizeof(Ent));
    c->type=ENT_CAR; c->sub=CAR_SEDAN; c->hp=100; c->state=ST_IDLE;
    c->x=tx+((fcos(ta)*220)>>10); c->y=ty+((fsin(ta)*220)>>10); c->ang=ta;
    g_p.x=tx; g_p.y=ty; g_p.ang=ta;
    g_p.weapon=W_FLAME; g_p.ammo=90; g_p.attack_cd=0;
    for(int k=0;k<12;k++){ g_p.attack_cd=0; game_input_action(); game_step(); }
    snprintf(b,sizeof b,"car fire=%d hp=%d",c->fire,c->hp);
    ck("flame sets a car alight", c->fire>0, b);

    int fuse=c->fire;
    g_p.weapon=W_PISTOL; g_p.ammo=20; g_p.attack_cd=0;
    game_input_action();
    for(int k=0;k<6;k++) game_step();
    snprintf(b,sizeof b,"fuse %d -> %d after a shot",fuse,c->fire);
    ck("shooting a burning car shortens its fuse", c->fire < fuse - 6, b);
  }

  // A grenade must break the glass around it.
  wr_seed(6); game_init();
  { int li=-1;
    for(int a=0;a<400 && li<0;a++){
      g_p.x=(int32_t)(wr_rand_n(60)+30)*TILE; g_p.y=(int32_t)(wr_rand_n(60)+30)*TILE;
      if(cell_at_point(g_p.x,g_p.y)==CELL_WATER) continue;
      city_refresh(g_p.x,g_p.y);
      for(int i=0;i<g_nlot;i++) if(g_lot[i].kind==LOT_GUNSHOP){ li=i; break; }
    }
    int32_t dx,dy,nx,ny; city_shop_door(&g_lot[li],&dx,&dy,&nx,&ny);
    shop_enter(dx,dy,nx,ny);
    int cab_before=0;
    for(int i=0;i<MAX_PROP;i++)
      if(g_prop[i].alive && !g_prop[i].loose && g_prop[i].kind==PROP_CABINET) cab_before++;
    int pick_before=0;
    for(int i=0;i<MAX_PICKUP;i++) if(g_pickup[i].alive) pick_before++;
    // Set one off beside the cabinets, which line the left wall now.
    boom_add(ROOM_OX+120, ROOM_OY+TILE+130);
    for(int k=0;k<4;k++) game_step();
    int cab_after=0;
    for(int i=0;i<MAX_PROP;i++)
      if(g_prop[i].alive && !g_prop[i].loose && g_prop[i].kind==PROP_CABINET) cab_after++;
    int pick_after=0;
    for(int i=0;i<MAX_PICKUP;i++) if(g_pickup[i].alive) pick_after++;
    snprintf(b,sizeof b,"%d cabinets standing before, %d after; stock on the floor %d -> %d",
             cab_before,cab_after,pick_before,pick_after);
    ck("an explosion breaks the cabinets",
       cab_before>0 && (cab_after<cab_before || pick_after>pick_before), b);
  }

  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
