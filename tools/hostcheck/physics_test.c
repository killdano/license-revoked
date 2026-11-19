#include "wr.h"
#include <stdio.h>
static void stick(int f,int s){ game_touch_down(STICK_HOME_X,STICK_HOME_Y);
  game_touch_move((int16_t)(STICK_HOME_X+s*STICK_RANGE),(int16_t)(STICK_HOME_Y-f*STICK_RANGE)); }
int main(void){
  wr_seed(2468); audio_init(); game_init();

  puts("=== can the character walk backwards? ===");
  g_p.ang=0; int32_t x0=g_p.x;
  stick(1,0); for(int i=0;i<40;i++) game_step();
  printf("  forward: spd=%+3d moved %+5d  phase=%3d\n",g_p.spd,(int)(g_p.x-x0),g_p.phase);
  game_touch_up(); for(int i=0;i<30;i++) game_step();
  x0=g_p.x; uint8_t ph0=g_p.phase;
  stick(-1,0); for(int i=0;i<40;i++) game_step();
  printf("  backward: spd=%+3d moved %+5d  walk cycle running %s\n\n",
     g_p.spd,(int)(g_p.x-x0), ((uint8_t)(g_p.phase-ph0) > 128)?"in reverse":"forward");
  game_touch_up();

  puts("=== ragdoll: run someone down ===");
  game_init(); memset(g_ent,0,sizeof(g_ent));
  g_ent[0].type=ENT_CAR; g_ent[0].sub=CAR_SEDAN; g_ent[0].hp=100;
  g_ent[0].state=ST_DRIVE; g_ent[0].x=g_p.x+40; g_ent[0].y=g_p.y;
  game_input_action();
  g_ent[1].type=ENT_PED; g_ent[1].sub=PED_MAN; g_ent[1].hp=100;
  g_ent[1].state=ST_IDLE; g_ent[1].x=g_p.x+900; g_ent[1].y=g_p.y;
  g_p.ang=0; stick(1,0);
  for(int f=0;f<200 && g_ent[1].state!=ST_DEAD;f++) game_step();
  printf("  hit at spd=%d -> launch v=(%d,%d,%d) spin=%d z=%d\n",
    g_p.spd,g_ent[1].vx,g_ent[1].vy,g_ent[1].vz,g_ent[1].spin,g_ent[1].z);
  int gibs=0; for(int i=0;i<MAX_GIB;i++) gibs+=g_gib[i].life?1:0;
  printf("  blood droplets airborne: %d\n",gibs);
  int peak=0,bounces=0; int16_t lastvz=g_ent[1].vz;
  for(int f=0;f<120;f++){ game_step();
    if(g_ent[1].z>peak) peak=g_ent[1].z;
    if(lastvz<0 && g_ent[1].vz>0) bounces++;
    lastvz=g_ent[1].vz; }
  printf("  tumbled to z=%d, bounced %d times, came to rest at z=%d v=(%d,%d)\n\n",
    peak,bounces,g_ent[1].z,g_ent[1].vx,g_ent[1].vy);
  int decals=0; for(int i=0;i<MAX_BLOOD;i++) decals+=g_blood[i].life?1:0;
  printf("  droplets settled into %d decals\n\n",decals);

  puts("=== car vs car ===");
  game_init(); memset(g_ent,0,sizeof(g_ent));
  g_ent[0].type=ENT_CAR; g_ent[0].hp=100; g_ent[0].state=ST_DRIVE;
  g_ent[0].x=g_p.x; g_ent[0].y=g_p.y; g_ent[0].ang=0; g_ent[0].spd=28;
  g_ent[1].type=ENT_CAR; g_ent[1].hp=100; g_ent[1].state=ST_DRIVE;
  g_ent[1].x=g_p.x+120; g_ent[1].y=g_p.y; g_ent[1].ang=32768; g_ent[1].spd=24;
  int32_t sep0=wr_dist(g_ent[1].x-g_ent[0].x,g_ent[1].y-g_ent[0].y);
  game_step();
  printf("  gap %d -> %d, hp %d/%d, speeds %d/%d\n\n",
    (int)sep0,(int)wr_dist(g_ent[1].x-g_ent[0].x,g_ent[1].y-g_ent[0].y),
    g_ent[0].hp,g_ent[1].hp,g_ent[0].spd,g_ent[1].spd);

  puts("=== arrest ===");
  game_init(); memset(g_ent,0,sizeof(g_ent));
  g_p.wanted=3; g_p.heat=250;
  g_ent[0].type=ENT_COP; g_ent[0].sub=PED_MAN; g_ent[0].hp=100;
  g_ent[0].state=ST_CHASE; g_ent[0].wep=W_PISTOL;
  g_ent[0].x=g_p.x+60; g_ent[0].y=g_p.y;
  for(int f=0;f<120 && !g_p.busted;f++){
    g_ent[0].x=g_p.x+60; g_ent[0].y=g_p.y; g_p.hp=100;
    game_step();
    if(f==20||f==40) printf("  f=%2d arrest=%2d in_car=%d\n",f,g_p.arrest,g_p.in_car);
  }
  printf("  busted=%d dead=%d after grab\n",g_p.busted,g_p.dead);
  return 0;
}
