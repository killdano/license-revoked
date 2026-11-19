#include "wr.h"
#include <stdio.h>
int main(void){
  wr_seed(1234); audio_init(); game_init();
  g_menu.volume=80; g_menu.sfx_on=1;

  puts("=== pause menu ===");
  menu_toggle();
  printf("  open=%d row=%d\n",g_menu.open,g_menu.row);
  menu_nav(1); menu_nav(1);
  printf("  after 2 down: row=%d (%s)\n",g_menu.row, g_menu.row==MENU_SFX?"Sound":"?");
  menu_nav(-1); menu_select();
  int16_t z0=g_p.zoom;
  printf("  editing Volume=%d; zoom before %d\n",g_menu.editing,z0);
  menu_nav(-1); menu_nav(-1);
  printf("  volume now %d\n",g_menu.volume);
  g_menu.row=MENU_VOLUME; g_menu.editing=1;
  for(int i=0;i<8;i++) menu_nav(1);
  printf("  zoom slider -> %d\n",g_p.zoom);
  // paused means paused
  int32_t px=g_p.x; for(int i=0;i<60;i++) game_step();
  printf("  world frozen while paused: %s\n", px==g_p.x?"yes":"NO");
  menu_toggle();

  puts("\n=== find a gun shop ===");
  int found=-1;
  for(int tries=0;tries<400 && found<0;tries++){
    g_p.x=(ISLAND_CX*TILE)+((int32_t)wr_rand_n(40)-20)*TILE;
    g_p.y=(ISLAND_CY*TILE)+((int32_t)wr_rand_n(40)-20)*TILE;
    city_set_interior(false); city_refresh(g_p.x,g_p.y);
    for(int i=0;i<g_nlot;i++) if(g_lot[i].kind==LOT_GUNSHOP){found=i;break;}
  }
  if(found<0){ puts("  no gun shop found"); return 1; }
  printf("  gun shop lot %d at %d,%d\n",found,(int)(g_lot[found].x0>>8),(int)(g_lot[found].y0>>8));

  int32_t dx=(g_lot[found].x0+g_lot[found].x1)/2, dy=g_lot[found].y1;
  shop_enter(dx,dy,0,1);
  printf("  inside=%d pos tile %d,%d cell=%d lots=%d\n",
     g_p.inside,(int)(g_p.x>>8),(int)(g_p.y>>8),cell_at_world(g_p.x,g_p.y),g_nlot);
  int cab=0,cnt=0,keeper=0;
  for(int i=0;i<MAX_PROP;i++) if(g_prop[i].alive){
    if(g_prop[i].kind==PROP_CABINET)cab++; if(g_prop[i].kind==PROP_COUNTER)cnt++; }
  for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_PED) keeper++;
  printf("  cabinets=%d counters=%d shopkeeper=%d\n",cab,cnt,keeper);

  puts("\n=== rob the place ===");
  for(int i=0;i<MAX_PICKUP;i++) g_pickup[i].alive=0;
  for(int i=0;i<MAX_PROP;i++) if(g_prop[i].alive&&g_prop[i].kind==PROP_CABINET)
    prop_hit(i,0,0,90);
  int flame=0,rocket=0,other=0;
  for(int i=0;i<MAX_PICKUP;i++) if(g_pickup[i].alive){
    if(g_pickup[i].weapon==W_FLAME)flame++;
    else if(g_pickup[i].weapon==W_ROCKET)rocket++; else other++; }
  printf("  smashed cabinets dropped: flame=%d rocket=%d other=%d\n",flame,rocket,other);

  puts("\n=== heavy weapons ===");
  g_p.weapon=W_FLAME; g_p.ammo=90; g_p.attack_cd=0;
  memset(g_ent,0,sizeof(g_ent));
  g_ent[0].type=ENT_PED; g_ent[0].hp=100; g_ent[0].state=ST_IDLE;
  g_ent[0].x=g_p.x+180; g_ent[0].y=g_p.y; g_p.ang=0;
  for(int f=0;f<40 && g_ent[0].state!=ST_DEAD;f++){
    if(!g_p.attack_cd) game_input_action(); game_step(); }
  int fire=0; for(int i=0;i<MAX_BULLET;i++) if(g_bullet[i].life&&g_bullet[i].kind==B_FIRE) fire++;
  printf("  flamethrower: target dead=%d, %d flame puffs in flight, ammo %d\n",
     g_ent[0].state==ST_DEAD,fire,g_p.ammo);

  memset(g_ent,0,sizeof(g_ent)); memset(g_boom,0,sizeof(g_boom));
  g_ent[0].type=ENT_CAR; g_ent[0].hp=100; g_ent[0].state=ST_DRIVE;
  g_ent[0].x=g_p.x+400; g_ent[0].y=g_p.y;
  g_p.weapon=W_ROCKET; g_p.ammo=5; g_p.attack_cd=0;
  for(int f=0;f<60 && g_ent[0].state!=ST_DEAD;f++){
    if(!g_p.attack_cd) game_input_action(); game_step(); }
  int booms=0; for(int i=0;i<MAX_BOOM;i++) booms+=g_boom[i].life?1:0;
  printf("  rocket: car wrecked=%d in one hit, explosions=%d, ammo %d\n",
     g_ent[0].state==ST_DEAD,booms,g_p.ammo);

  shop_exit();
  printf("\n  back outside: inside=%d tile %d,%d lots=%d\n",
     g_p.inside,(int)(g_p.x>>8),(int)(g_p.y>>8),g_nlot);
  return 0;
}
