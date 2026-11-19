#include "wr.h"
#include <stdio.h>
static int32_t HX,HY;
static void stick(int f){ game_touch_down(STICK_HOME_X,STICK_HOME_Y);
  game_touch_move(STICK_HOME_X, (int16_t)(STICK_HOME_Y - f*STICK_RANGE)); }
// Find a long clear straight so the car never touches anything.
static void park(void){
  g_p.x=HX; g_p.y=HY; g_p.ang=0; g_p.turn=0; g_p.spd=0;
  if(g_p.in_car){ Ent*c=&g_ent[g_p.in_car-1]; c->x=HX;c->y=HY;c->ang=0;c->spd=0; }
}
int main(void){
  wr_seed(88); audio_init(); game_init();
  // A road tile running along +X, with clear road either side of it.
  for(int ty=ISLAND_CY-CITY_R;ty<ISLAND_CY+CITY_R;ty++) for(int tx=ISLAND_CX-CITY_R;tx<ISLAND_CX+CITY_R;tx++)
    if(cell_at(tx,ty)==CELL_ROAD && cell_at(tx+4,ty)==CELL_ROAD && cell_at(tx-4,ty)==CELL_ROAD){
      HX=(tx<<TILE_SH)+128; HY=(ty<<TILE_SH)+128; tx=ISLAND_CX+CITY_R; ty=ISLAND_CY+CITY_R; }
  memset(g_ent,0,sizeof(g_ent));
  g_p.x=HX; g_p.y=HY;
  g_ent[0].type=ENT_CAR; g_ent[0].sub=CAR_SEDAN; g_ent[0].hp=100;
  g_ent[0].state=ST_DRIVE; g_ent[0].x=HX+40; g_ent[0].y=HY;
  game_input_action();
  printf("in car %d, on a clear straight\n\n", g_p.in_car);

  for(int d=0;d<2;d++) for(int gear=0;gear<2;gear++){
    park(); game_input_turn(0);
    stick(gear? -1 : 1);
    for(int i=0;i<60;i++) game_step();
    int16_t spd=g_p.spd; uint16_t a0=g_p.ang;
    game_input_turn(d?1:-1);
    for(int i=0;i<25;i++) game_step();
    int16_t swing=(int16_t)(g_p.ang-a0);
    printf("hold %-5s  %-8s  spd=%+3d  swing %-5s  rate %5d/frame\n",
      d?"DOWN":"UP", gear?"REVERSE":"FORWARD", spd,
      swing<0?"left":"right", (swing<0?-swing:swing)/25);
    game_input_turn(0); game_touch_up();
  }
  return 0;
}
