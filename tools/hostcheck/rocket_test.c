#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-48s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
static int gibs(void){ int n=0; for(int i=0;i<MAX_GIB;i++) if(g_gib[i].life) n++; return n; }
// The pool steals the shortest-lived slot when it is full, so a saturated
// pool cannot grow - count FRESH pieces instead, which is what a shower of
// wreckage actually looks like.
// ...and a pool that is entirely fresh cannot grow that way either (a
// burning car keeps it topped up), so measure the life left in the pool:
// a burst replaces the shortest-lived pieces with new ones and raises it.
static int fresh(void){ int n=0; for(int i=0;i<MAX_GIB;i++) n+=g_gib[i].life; return n; }
int main(void){
  char b[180];
  setvbuf(stdout,NULL,_IONBF,0);
  wr_seed(6); audio_init(); game_init();
  for(int i=0;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
  int32_t tx=g_p.x, ty=g_p.y; uint16_t ta=0; road_snap(&tx,&ty,&ta);
  Ent*c=&g_ent[0]; memset(c,0,sizeof(Ent));
  c->type=ENT_CAR; c->sub=CAR_SEDAN; c->hp=100; c->state=ST_IDLE;
  c->x=tx+((fcos(ta)*500)>>10); c->y=ty+((fsin(ta)*500)>>10); c->ang=ta;
  g_p.x=tx; g_p.y=ty; g_p.ang=ta;
  int32_t sx=c->x, sy=c->y;

  g_p.weapon=W_ROCKET; g_p.ammo=6; g_p.attack_cd=0;
  game_input_action();
  int airborne=0, peak_z=0, bounces=0, was_down=1, fuse=0, boom_gibs=0;
  int gib_at_boom=0;
  for(int k=0;k<400;k++){
    int fire_before=c->fire;
    int gib_before=fresh();
    game_step();
    if(c->type!=ENT_CAR) break;
    if(!fuse && c->fire) fuse=c->fire;          // the fuse it was left with
    if(c->z>0){ airborne=1; if(c->z>peak_z) peak_z=c->z; was_down=0; }
    else if(!was_down){ bounces++; was_down=1; }
    // The frame the fuse runs out is the frame to count wreckage on.
    if(fire_before==1 && c->fire==0){ gib_at_boom=gib_before; boom_gibs=fresh(); break; }
  }
  snprintf(b,sizeof b,"peak height %d, %d touchdowns, moved %ld units",
           peak_z,bounces,(long)wr_dist(c->x-sx,c->y-sy));
  ck("a rocket throws the car into the air", airborne && peak_z>40, b);
  ck("and it bounces before it settles", bounces>=1, b);
  snprintf(b,sizeof b,"left burning with a %d frame fuse",fuse);
  ck("and it is left on a short fuse", fuse>0 && fuse<95, b);
  snprintf(b,sizeof b,"pool life %d -> %d on the frame it went",
           gib_at_boom,boom_gibs);
  ck("the explosion throws pieces of the car", boom_gibs>gib_at_boom, b);

  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
