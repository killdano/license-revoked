#include "wr.h"
#include <stdio.h>
int main(void){
  wr_seed(5); audio_init(); game_init();
  int road=0,pave=0,bld=0,park=0,sand=0,water=0;
  for(int ty=0;ty<80;ty++) for(int tx=0;tx<80;tx++)
    switch(cell_at(tx,ty)){case CELL_ROAD:road++;break;case CELL_PAVE:pave++;break;
      case CELL_BLD:bld++;break;case CELL_PARK:park++;break;
      case CELL_SAND:sand++;break;default:water++;}
  printf("island 80x80 tiles: road=%d pave=%d bld=%d park=%d sand=%d water=%d\n",
     road,pave,bld,park,sand,water);
  printf("player spawn tile %d,%d cell=%d\n",(int)(g_p.x>>8),(int)(g_p.y>>8),
     cell_at_world(g_p.x,g_p.y));
  printf("lots cached: %d\n", g_nlot);
  int kinds[LOT_KIND_COUNT]={0};
  for(int i=0;i<g_nlot;i++) kinds[g_lot[i].kind]++;
  printf("  solid=%d garage=%d store=%d\n",kinds[0],kinds[1],kinds[2]);
  int props=0,pk[PROP_KIND_COUNT]={0};
  for(int i=0;i<MAX_PROP;i++) if(g_prop[i].alive){props++;pk[g_prop[i].kind]++;}
  printf("props placed: %d (lamp=%d sign=%d box=%d table=%d chair=%d hydrant=%d)\n",
     props,pk[0],pk[1],pk[2],pk[3],pk[4],pk[5]);

  // Walk to the coast and confirm the sea stops us.
  puts("\n--- swim test ---");
  int32_t sx=g_p.x, sy=g_p.y;
  g_p.x=(ISLAND_CX<<TILE_SH); g_p.y=((ISLAND_CY-SAND_R+1)<<TILE_SH);
  printf("  at beach edge cell=%d blocked=%d\n",
     cell_at_world(g_p.x,g_p.y), city_blocked(g_p.x,g_p.y-TILE*3,false));
  g_p.x=sx; g_p.y=sy;

  puts("\n--- 3000 frames of driving ---");
  for(int k=0;k<3000;k++){
    game_touch_down(STICK_HOME_X,STICK_HOME_Y);
    game_touch_move(STICK_HOME_X,(int16_t)(STICK_HOME_Y-STICK_RANGE));
    if(k%17==0) game_input_action();
    game_input_turn((k/60)%3==0?-1:((k/60)%3==1?1:0));
    game_step(); render_frame(NULL,GRect(0,0,200,228));
    if(g_p.dead) game_input_action();
  }
  int loose=0; for(int i=0;i<MAX_PROP;i++) if(g_prop[i].loose) loose++;
  int e=0; for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type) e++;
  printf("  survived. pos=%d,%d entities=%d props knocked loose=%d lots=%d\n",
     (int)(g_p.x>>8),(int)(g_p.y>>8),e,loose,g_nlot);

  puts("\n--- zoom slider ---");
  for(int z=0;z<=256;z+=64){
    g_p.zoom=(int16_t)z;
    for(int i=0;i<60;i++) render_frame(NULL,GRect(0,0,200,228));
    int32_t deg = PITCH_NEAR + (((PITCH_FAR-PITCH_NEAR)*z)>>8);
    printf("  zoom=%3d -> pitch approaches %2d deg (cp=%4d sp=%4d)\n",
       z,(int)deg,(int)g_cam_cp,(int)g_cam_sp);
  }
  return 0;
}
