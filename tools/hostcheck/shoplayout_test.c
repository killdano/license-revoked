#include "wr.h"
#include <stdio.h>
int main(void){
  setvbuf(stdout,NULL,_IONBF,0);
  wr_seed(6); audio_init(); game_init();
  int li=-1;
  for(int a=0;a<400 && li<0;a++){
    g_p.x=(int32_t)(wr_rand_n(60)+30)*TILE; g_p.y=(int32_t)(wr_rand_n(60)+30)*TILE;
    if(cell_at_point(g_p.x,g_p.y)==CELL_WATER) continue;
    city_refresh(g_p.x,g_p.y);
    for(int i=0;i<g_nlot;i++) if(g_lot[i].kind==LOT_GUNSHOP){ li=i; break; }
  }
  if(li<0){ printf("no shop found\n"); return 1; }
  int32_t dx,dy,nx,ny; city_shop_door(&g_lot[li],&dx,&dy,&nx,&ny);
  shop_enter(dx,dy,nx,ny);
  int clerks=0, cab=0, rack=0, doors=0, inside_room=0;
  for(int i=0;i<MAX_ENT;i++)
    if(g_ent[i].type==ENT_PED){
      clerks++;
      if(g_ent[i].x>ROOM_OX && g_ent[i].x<ROOM_OX+ROOM_W &&
         g_ent[i].y>ROOM_OY && g_ent[i].y<ROOM_OY+ROOM_H) inside_room++;
    }
  for(int i=0;i<MAX_PROP;i++){
    if(!g_prop[i].alive) continue;
    if(g_prop[i].kind==PROP_CABINET) cab++;
    if(g_prop[i].kind==PROP_WALLGUN) rack++;
    if(g_prop[i].kind==PROP_DOOR) doors++;
  }
  printf("shop: %d clerks (%d inside the room), %d cabinets, %d racked guns, %d door\n",
         clerks,inside_room,cab,rack,doors);
  // The rebuilt store: one clerk and two customers, four cabinets (one per
  // tier), three racked guns on the wall, one door.
  int ok = (clerks==3 && inside_room==3 && cab==4 && rack==3 && doors==1);
  printf("%s\n", ok ? "  ok   clerk and two customers, four cabinets, three racked guns, one door"
                    : "  FAIL layout is not what was intended");
  return ok?0:1;
}
