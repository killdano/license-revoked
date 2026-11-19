#include "wr.h"
#include "wr_gfx.h"
#include <stdio.h>
#include <stdlib.h>
// Hammer the whole simulation with random input for a long time, in every
// mode, and let ASan/UBSan catch anything that misbehaves.
static uint32_t rs = 12345;
static uint32_t rnd(void){ rs^=rs<<13; rs^=rs>>17; rs^=rs<<5; return rs; }
int main(int argc, char **argv){
  int frames = argc>1 ? atoi(argv[1]) : 60000;
  wr_seed(999); audio_init(); game_init();
  g_menu.volume=80; g_menu.sfx_on=1;
  int shops=0, deaths=0, boarded=0, menus=0;
  for(int f=0;f<frames;f++){
    uint32_t r=rnd();
    switch(r%14){
      case 0: game_input_turn(-1); break;
      case 1: game_input_turn(1); break;
      case 2: game_input_turn(0); break;
      case 3: { int16_t x=(int16_t)(rnd()%200), y=(int16_t)(rnd()%228);
                game_touch_down(x,y); } break;
      case 4: { int16_t x=(int16_t)(rnd()%200), y=(int16_t)(rnd()%228);
                game_touch_move(x,y); } break;
      case 5: game_touch_up(); break;
      case 6: game_input_action(); break;
      case 7: if((rnd()%40)==0){ menu_toggle(); menus++; } break;
      case 8: menu_nav((rnd()&1)?1:-1); break;
      case 9: menu_select(); break;
      case 10: if((rnd()%400)==0){ g_p.wanted=(uint8_t)(rnd()%6); g_p.heat=(uint16_t)(rnd()%480); } break;
      case 11: if((rnd()%600)==0){ g_p.weapon=(uint8_t)(rnd()%W_COUNT); g_p.ammo=(uint8_t)(rnd()%80); } break;
      case 12: if((rnd()%900)==0){ boom_add(g_p.x+(int32_t)(rnd()%800)-400, g_p.y+(int32_t)(rnd()%800)-400); } break;
      default: break;
    }
    if(g_menu.quit) g_menu.quit=0;
    game_step();
    render_frame(NULL,GRect(0,0,200,228));
    if(g_p.dead){ deaths++; game_input_action(); }
    // Force the states random poking rarely reaches on its own.
    // Put a boat under the player now and then so the marine code gets used.
    if((f%1100)==0){
      for(int i=0;i<MAX_ENT;i++) if(!g_ent[i].type){
        g_ent[i].type=ENT_BOAT; g_ent[i].hp=100; g_ent[i].state=ST_DRIVE;
        g_ent[i].sub=(uint8_t)(rnd()%3);
        g_ent[i].x=g_p.x+60; g_ent[i].y=g_p.y; g_ent[i].ang=g_p.ang;
        game_input_action();
        break;
      }
    }
    if((f%700)==0){
      memset(g_ent,0,sizeof(g_ent));
      g_ent[0].type=ENT_CAR; g_ent[0].hp=100; g_ent[0].state=ST_DRIVE;
      g_ent[0].sub=(uint8_t)(rnd()%CAR_MODEL_COUNT);
      g_ent[0].x=g_p.x+40; g_ent[0].y=g_p.y; g_ent[0].ang=g_p.ang;
      game_input_action();
    }
    if((f%2300)==0){
      int32_t bx=g_p.x, by=g_p.y;
      shop_enter(bx,by,0,1);
      for(int i=0;i<MAX_PROP;i++) if(g_prop[i].alive) prop_hit(i,20,0,90);
    }
    if((f%2300)==1500) shop_exit();
    if((f%310)==0){ g_p.weapon=(uint8_t)(rnd()%W_COUNT); g_p.ammo=90; g_p.attack_cd=0; }
    if((f%160)==0) g_p.zoom=(int16_t)(rnd()%257);
    if(g_p.inside) shops++;
    if(g_p.in_car) boarded++;
    // occasionally teleport somewhere hostile or interesting
    if((r%5000)==0){
      switch((r/5000)%8){
        case 0: g_p.x=0; g_p.y=0; break;
        case 1: g_p.x=CITY_MAX-TILE; g_p.y=CITY_MAX-TILE; break;
        case 2: g_p.x=ISLAND_CX*TILE; g_p.y=(ISLAND_CY+SAND_R+6)*TILE; break;
        case 3: g_p.x=-4*TILE; g_p.y=-4*TILE; break;
        case 4: {   // out to sea
          int m=(int)(rnd()%MARINA_COUNT);
          g_p.x=MARINA_TX[m]*TILE; g_p.y=MARINA_TY[m]*TILE; break; }
        case 5: {   // halfway across the bridge
          g_p.x=(ISLAND_CX+ISLAND_BX)*TILE/2;
          g_p.y=(ISLAND_CY+ISLAND_BY)*TILE/2; break; }
        case 6: g_p.x=ISLAND_BX*TILE; g_p.y=ISLAND_BY*TILE; break;   // far island
        default: g_p.x=ISLAND_CX*TILE; g_p.y=ISLAND_CY*TILE; break;
      }
      g_p.in_car=0; g_p.spd=0;
      city_set_interior(false);
      city_refresh(g_p.x,g_p.y);
    }
  }
  printf("fuzz: %d frames survived. deaths=%d, frames in a shop=%d, frames driving=%d, menu opens=%d\n",
     frames,deaths,shops,boarded,menus);
  return 0;
}
