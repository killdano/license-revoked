#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-46s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
int main(void){
  char b[160];
  setvbuf(stdout, NULL, _IONBF, 0);   // so output survives a timeout kill
  wr_seed(6); audio_init(); game_init();

  // Armour takes damage before health does.
  g_p.hp=100; g_p.armor=40;
  player_absorb(25);
  snprintf(b,sizeof b,"after 25 dmg: armour %d hp %d",g_p.armor,g_p.hp);
  ck("damage goes through armour first", g_p.armor==15 && g_p.hp==100, b);

  // And spills over once it is gone.
  player_absorb(35);
  snprintf(b,sizeof b,"after 35 more: armour %d hp %d",g_p.armor,g_p.hp);
  ck("damage past the vest reaches health", g_p.armor==0 && g_p.hp==80, b);

  // A cop car comes with a vest.
  wr_seed(6); game_init();
  { int32_t tx=g_p.x, ty=g_p.y; uint16_t ta=0; road_snap(&tx,&ty,&ta);
    for(int i=0;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
    Ent*c=&g_ent[0]; memset(c,0,sizeof(Ent));
    c->type=ENT_COPCAR; c->hp=100; c->state=ST_IDLE; c->x=tx+40; c->y=ty; c->ang=ta;
    g_p.x=tx; g_p.y=ty; g_p.armor=0; g_p.in_car=0;
    game_input_action();
    snprintf(b,sizeof b,"in_car=%d armour=%d",g_p.in_car,g_p.armor);
    ck("jacking a cop car gets you a vest", g_p.in_car && g_p.armor==ARMOR_MAX, b);
  }

  // The shop must have a door, and leaving must not drop you in the sea.
  wr_seed(6); game_init();
  { int32_t dx,dy,nx,ny; int li=-1;
    for(int a=0;a<400 && li<0;a++){
      g_p.x=(int32_t)(wr_rand_n(60)+30)*TILE; g_p.y=(int32_t)(wr_rand_n(60)+30)*TILE;
      if(cell_at_point(g_p.x,g_p.y)==CELL_WATER) continue;
      city_refresh(g_p.x,g_p.y);
      for(int i=0;i<g_nlot;i++) if(g_lot[i].kind==LOT_GUNSHOP){ li=i; break; }
    }
    if(li<0){ printf("  (no gun shop found in sampling - skipped)\n"); printf("\n%d failure(s)\n",fails); return fails?1:0; }
    city_shop_door(&g_lot[li],&dx,&dy,&nx,&ny);
    shop_enter(dx,dy,nx,ny);
    int doors=0;
    for(int i=0;i<MAX_PROP;i++) if(g_prop[i].alive&&g_prop[i].kind==PROP_DOOR) doors++;
    snprintf(b,sizeof b,"inside=%d, %d door props",g_p.inside,doors);
    ck("the shop has a door", g_p.inside && doors>0, b);

    // Press it.
    // The doorway is in the FRONT wall, at ROOM_OY + ROOM_H - the test was
    // standing at the back wall.
    g_p.x=ROOM_OX+ROOM_W/2; g_p.y=ROOM_OY+ROOM_H-30;
    game_input_action();
    snprintf(b,sizeof b,"inside=%d, standing on %d",g_p.inside,cell_at_point(g_p.x,g_p.y));
    ck("pressing the door returns you to the street",
       !g_p.inside && cell_at_point(g_p.x,g_p.y)!=CELL_WATER, b);

    // Being thrown clear of the room must put you back INSIDE it - you leave
    // by pressing the door, the same way you came in.
    shop_enter(dx,dy,nx,ny);
    g_p.x=ROOM_OX+ROOM_W/2; g_p.y=ROOM_OY+ROOM_H+200;   // shoved outside
    game_step();
    snprintf(b,sizeof b,"inside=%d at (%ld,%ld)",g_p.inside,
             (long)((g_p.x-ROOM_OX)/TILE),(long)((g_p.y-ROOM_OY)/TILE));
    ck("being thrown out of the room puts you back in it",
       g_p.inside && g_p.y <= ROOM_OY+ROOM_H && g_p.y >= ROOM_OY, b);
  }
  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
