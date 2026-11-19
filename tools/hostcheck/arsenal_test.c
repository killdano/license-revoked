// The store sells everything; bought stock is on the counter every visit;
// grenades lob and go off; an empty gun is gone from your hands.
#include "wr.h"
#include "wr_domain.h"
#include <stdio.h>
#include <string.h>
static int fails;
static void ck(const char *w, bool ok, const char *d){ printf("  %-50s %s  %s\n",w,ok?"ok  ":"FAIL",d); if(!ok) fails++; }
static int counter_items(int *vest){ int n=0; *vest=0; for(int i=0;i<MAX_PICKUP;i++) if(g_pickup[i].alive){ n++; if(g_pickup[i].weapon==PICKUP_ARMOR) (*vest)++; } return n; }
int main(void){ char b[160]; setvbuf(stdout,NULL,_IONBF,0);
  wr_seed(6); game_init(); g_p.x=ISLAND_CX*TILE; g_p.y=ISLAND_CY*TILE; domain_scenery(); g_p.cash=1000;
  int32_t doorx=g_p.x, doory=g_p.y;
  inside_enter(IN_GUNSHOP,doorx,doory,0,1);
  Prop *c=NULL; for(int i=0;i<MAX_PROP;i++) if(g_prop[i].alive&&g_prop[i].kind==PROP_COUNTER){ c=&g_prop[i]; break; }
  g_p.x=c->x; g_p.y=c->y+150; g_p.ang=49152;   // back from the counter, or you pick things up as they drop
  for(int k=0;k<42;k++){ g_p.attack_cd=0; game_input_action(); game_step(); }
  for(int f=0;f<80;f++) game_step();
  int vest; int n=counter_items(&vest);
  printf("    unlocked=%02x pickups:",g_in.unlocked); for(int i=0;i<MAX_PICKUP;i++) if(g_pickup[i].alive) printf(" w%d",g_pickup[i].weapon); printf("\n");
  snprintf(b,sizeof b,"credit=%d items on the counter=%d (vests %d)",g_in.credit,n,vest);
  ck("$400 puts all seven items on the counter, vest included", n==7 && vest==1, b);
  inside_exit(); for(int f=0;f<5;f++) game_step();
  inside_enter(IN_GUNSHOP,doorx,doory,0,1); g_p.y=ROOM_OY+g_in.h-80; for(int f=0;f<5;f++) game_step();   // the same door
  n=counter_items(&vest);
  snprintf(b,sizeof b,"items on the counter after coming back=%d",n);
  ck("bought stock is on the counter again next visit", n==7, b);
  inside_exit();
  // grenades: a lob that goes off
  g_p.x=ISLAND_CX*TILE; g_p.y=ISLAND_CY*TILE; { int32_t x=g_p.x,y=g_p.y; uint16_t a; road_snap(&x,&y,&a); g_p.x=x; g_p.y=y; g_p.ang=a; }
  memset(g_ent,0,sizeof g_ent); g_p.weapon=W_GRENADE; g_p.ammo=3; g_p.inside=0;
  game_step(); game_input_tap();
  int flying=0, booms=0; for(int f=0;f<40;f++){ game_step(); for(int i=0;i<MAX_BULLET;i++) if(g_bullet[i].life&&g_bullet[i].kind==B_GRENADE) flying++; for(int i=0;i<MAX_BOOM;i++) if(g_boom[i].life) { booms++; break; } }
  snprintf(b,sizeof b,"grenade in the air for %d frames, explosion frames=%d, ammo left=%d",flying,booms,g_p.ammo);
  ck("a grenade lobs and goes off", flying>10 && booms>0 && g_p.ammo==2, b);
  // an empty gun is gone
  g_p.weapon=W_PISTOL; g_p.ammo=1; g_p.inv[W_PISTOL]=1; game_step(); g_p.attack_cd=0; game_input_tap(); game_step();
  weapon_next(); bool back=false; for(int i=0;i<W_COUNT;i++){ if(g_p.weapon==W_PISTOL) back=true; weapon_next(); }
  snprintf(b,sizeof b,"after the last round: weapon=%d inv[pistol]=%d; cycling finds the pistol=%d",g_p.weapon,g_p.inv[W_PISTOL],back);
  ck("an empty gun is gone from your hands and the cycle", !back, b);
  printf("%d failure(s)\n",fails); return fails?1:0; }
