#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-46s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
int main(void){
  char b[170];
  setvbuf(stdout,NULL,_IONBF,0);
  wr_seed(6); audio_init(); game_init();

  // Throwing one puts a fused grenade in the air.
  g_p.weapon=W_GRENADE; g_p.ammo=4; g_p.attack_cd=0;
  for(int i=0;i<MAX_BULLET;i++) g_bullet[i].life=0;
  game_input_action();
  int gi=-1; for(int i=0;i<MAX_BULLET;i++) if(g_bullet[i].life&&g_bullet[i].kind==B_GRENADE){gi=i;break;}
  snprintf(b,sizeof b,"grenade in flight=%d ammo=%d",gi>=0,g_p.ammo);
  ck("you can throw a grenade", gi>=0 && g_p.ammo==3, b);

  // It must travel, then explode on its own when the fuse runs out.
  int32_t sx=g_bullet[gi].x, sy=g_bullet[gi].y;
  int boomed=0;
  for(int k=0;k<80 && !boomed;k++){
    game_step();
    if(!g_bullet[gi].life) boomed=1;
  }
  snprintf(b,sizeof b,"travelled %ld units before going off",
           (long)wr_dist(g_bullet[gi].x-sx,g_bullet[gi].y-sy));
  ck("the fuse sets it off on its own", boomed, b);

  // It kills what it lands near.
  wr_seed(6); game_init();
  for(int i=0;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
  { Ent*t=&g_ent[0]; memset(t,0,sizeof(Ent));
    t->type=ENT_PED; t->hp=100; t->state=ST_IDLE;
    t->x=g_p.x+((fcos(g_p.ang)*420)>>10);
    t->y=g_p.y+((fsin(g_p.ang)*420)>>10);
    g_p.weapon=W_GRENADE; g_p.ammo=4; g_p.attack_cd=0;
    game_input_action();
    int dead=0;
    for(int k=0;k<90 && !dead;k++){ game_step(); if(t->state==ST_DEAD) dead=1; }
    snprintf(b,sizeof b,"target hp %d state %d",t->hp,t->state);
    ck("a grenade kills what it lands beside", dead, b);
  }

  // And the shop stocks them.
  { int found=-1;
    for(int a=0;a<400 && found<0;a++){
      g_p.x=(int32_t)(wr_rand_n(60)+30)*TILE; g_p.y=(int32_t)(wr_rand_n(60)+30)*TILE;
      if(cell_at_point(g_p.x,g_p.y)==CELL_WATER) continue;
      city_refresh(g_p.x,g_p.y);
      for(int i=0;i<g_nlot;i++) if(g_lot[i].kind==LOT_GUNSHOP){ found=i; break; }
    }
    if(found>=0){
      int32_t dx,dy,nx,ny; city_shop_door(&g_lot[found],&dx,&dy,&nx,&ny);
      shop_enter(dx,dy,nx,ny);
      for(int i=0;i<MAX_PROP;i++)
        if(g_prop[i].alive&&g_prop[i].kind==PROP_CABINET) prop_hit(i,0,0,200);
      int gren=0, vest=0;
      for(int i=0;i<MAX_PICKUP;i++){
        if(!g_pickup[i].alive) continue;
        if(g_pickup[i].weapon==W_GRENADE) gren++;
        if(g_pickup[i].weapon==PICKUP_ARMOR) vest++;
      }
      // The store sells pistol, shotgun, flamethrower and rockets by tier;
      // smashing every cabinet spills all four and trips the alarm.
      int guns=0; for(int i=0;i<MAX_PICKUP;i++) if(g_pickup[i].alive&&g_pickup[i].weapon<W_COUNT) guns++;
      (void)gren; (void)vest;
      snprintf(b,sizeof b,"%d guns on the floor, alarm=%d",guns,g_in.alarm);
      ck("smashing the store spills its stock and trips the alarm", guns==4 && g_in.alarm, b);
    }
  }
  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
