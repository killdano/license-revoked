#include "wr.h"
#include <stdio.h>
int main(void){
  wr_seed(6); audio_init(); game_init();
  printf("lots built near player: %d\n", g_nlot);
  int k[LOT_KIND_COUNT]={0}; for(int i=0;i<g_nlot;i++) k[g_lot[i].kind]++;
  printf("  solid=%d garage=%d store=%d gunshop=%d\n\n",k[0],k[1],k[2],k[3]);

  puts("--- stick roundness: same push, eight directions ---");
  const char *nm[8]={"N ","NE","E ","SE","S ","SW","W ","NW"};
  int dxs[8]={0,1,1,1,0,-1,-1,-1}, dys[8]={-1,-1,0,1,1,1,0,-1};
  for(int d=0;d<8;d++){
    g_p.ang=0; g_p.spd=0; g_p.strafe=0;
    // push exactly to the rim in each direction
    int px=STICK_HOME_X, py=STICK_HOME_Y;
    game_touch_down((int16_t)px,(int16_t)py);
    game_touch_move((int16_t)(px+dxs[d]*STICK_RANGE),(int16_t)(py+dys[d]*STICK_RANGE));
    int32_t f=g_p.stick_f, s=g_p.stick_s;
    int32_t mag=wr_sqrt(f*f+s*s);
    int32_t x0=g_p.x,y0=g_p.y;
    for(int i=0;i<30;i++) game_step();
    int32_t moved=wr_sqrt((g_p.x-x0)*(g_p.x-x0)+(g_p.y-y0)*(g_p.y-y0));
    printf("  %s  f=%+4d s=%+4d  |v|=%3d   travelled %4d\n",nm[d],(int)f,(int)s,(int)mag,(int)moved);
    game_touch_up();
  }

  puts("\n--- driving onto sand ---");
  memset(g_ent,0,sizeof(g_ent));
  g_ent[0].type=ENT_CAR; g_ent[0].hp=100; g_ent[0].state=ST_DRIVE;
  g_ent[0].x=g_p.x+40; g_ent[0].y=g_p.y; game_input_action();
  g_p.spd=30;
  game_touch_down(STICK_HOME_X,STICK_HOME_Y);
  game_touch_move(STICK_HOME_X,(int16_t)(STICK_HOME_Y-STICK_RANGE));
  // force him onto sand
  for(int32_t t=0;t<200;t++){
    int32_t tx=(g_p.x>>TILE_SH)+t;
    if(cell_at(tx,g_p.y>>TILE_SH)==CELL_SAND){ g_p.x=(tx<<TILE_SH)+128; break; }
  }
  printf("  on %s, entering at spd=30\n", cell_at_world(g_p.x,g_p.y)==CELL_SAND?"sand":"other");
  for(int i=0;i<25;i++){ game_step(); if(i==5||i==12||i==24)
    printf("   after %2d frames (%.1fs): spd=%d\n",i+1,(i+1)/25.0,g_p.spd); }
  return 0;
}
