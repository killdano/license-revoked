#include "wr.h"
#include <stdio.h>
static int fails=0;
// Step the world but keep it empty apart from the staged entities - the
// spawner trickles population in every frame and a wandering car ruins a
// staged scene.
static void step_only(int keep){
  game_step();
  for(int i=keep;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
}
static void ck(const char*n,int ok,const char*d){printf("  %-42s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
int main(void){
  char b[160];

  // --- traffic queues at a red -------------------------------------------
  wr_seed(6); audio_init(); game_init();
  g_p.wanted=0; g_p.heat=0;
  for(int i=0;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
  int32_t sx=g_p.x, sy=g_p.y; uint16_t sa=0;
  { int32_t tx=sx, ty=sy; road_snap(&tx,&ty,&sa); sx=tx; sy=ty; }
  int fam = (fcos(sa)*fsin(sa) < 0) ? 0 : 1;
  int32_t cc=fcos(sa), ss=fsin(sa);
  int32_t cross = (fam==0) ? (sx - sy) : (sx + sy);
  int32_t rate  = (fam==0) ? (cc - ss) : (cc + ss);
  int32_t m2 = cross % DIAG_PERIOD; if(m2<0)m2+=DIAG_PERIOD;
  int32_t remc = (rate>0) ? (DIAG_PERIOD - m2) : m2;
  int32_t jdist = (int32_t)(((int64_t)remc*1024)/(rate>0?rate:-rate));
  int32_t jx = sx + ((cc*jdist)>>10), jy = sy + ((ss*jdist)>>10);
  // Park the player just off the carriageway near the junction, on foot.
  g_p.x = jx - ((ss*3*TILE)>>10); g_p.y = jy + ((cc*3*TILE)>>10);
  Ent *c0=&g_ent[0], *c1=&g_ent[1];
  // Wait for a FRESH red for this family, so there is a full window.
  while(!traffic_green(fam)) game_step();
  while(traffic_green(fam))  game_step();
  for(int i=0;i<2;i++){
    Ent*e=&g_ent[i]; memset(e,0,sizeof(Ent));
    e->type=ENT_CAR; e->sub=CAR_SEDAN; e->hp=100;
    // The stop line is ROAD_HALF+130 = 380 out; start the leader well
    // upstream of it and the follower a car-length behind him.
    e->x=jx - ((cc*(700+i*340))>>10);
    e->y=jy - ((ss*(700+i*340))>>10);
    e->ang=sa; e->spd=10; e->state=ST_DRIVE; e->timer=1;
  }
  int both_stopped=0; int32_t p0=0,p1=0;
  for(int k=0;k<LIGHT_CYCLE-6;k++){
    step_only(2);
    p0 = ((jx-c0->x)*cc + (jy-c0->y)*ss)>>10;   // distance still to run
    p1 = ((jx-c1->x)*cc + (jy-c1->y)*ss)>>10;
    // The dist/42 braking curve has them creep the last few units at
    // walking pace rather than snap to zero - that IS a queue.
    if(c0->spd<=2 && c1->spd<=2){ both_stopped=1; break; }
  }
  snprintf(b,sizeof b,"stopped=%d dist-to-cross %ld and %ld, hp %d/%d",
           both_stopped,(long)p0,(long)p1,c0->hp,c1->hp);
  ck("cars queue at a red light",
     both_stopped && p0>=330 && p0<720 && p1>p0+150 && c0->hp==100 && c1->hp==100, b);
  while(!traffic_green(fam)) step_only(2);
  int32_t before=((jx-c0->x)*cc + (jy-c0->y)*ss)>>10;
  for(int k=0;k<70;k++) step_only(2);
  int32_t after=((jx-c0->x)*cc + (jy-c0->y)*ss)>>10;
  snprintf(b,sizeof b,"lead car ran %ld toward the box, spd %d",(long)(before-after),c0->spd);
  ck("the queue pulls away on green", before-after>200, b);

  // (the ambulance was retired to make room for the club and the store)

  // --- death cinematic gating --------------------------------------------
  wr_seed(6); audio_init(); game_init();
  g_p.hp=0; game_step();
  int died=g_p.dead && !g_p.busted;
  game_input_action();               // too early: must NOT respawn
  int early=g_p.dead;
  for(int f=0;f<50;f++){ game_step(); render_frame(NULL,GRect(0,0,200,228)); }
  uint16_t cine_after=g_p.cine;
  game_input_action();
  int back=!g_p.dead;
  snprintf(b,sizeof b,"died=%d held_early=%d cine=%u respawned=%d",died,early,cine_after,back);
  ck("death cinematic holds, SELECT continues",died&&early&&cine_after>45&&back,b);

  // --- arrest stages the cuffing officer ---------------------------------
  wr_seed(6); audio_init(); game_init();
  // spawn a cop next to the player and let the arrest complete
  { int i=-1; for(int k2=0;k2<MAX_ENT;k2++) if(g_ent[k2].type==ENT_NONE){i=k2;break;}
    Ent*e=&g_ent[i]; e->type=ENT_COP; e->hp=100; e->state=ST_CHASE;
    e->x=g_p.x+40; e->y=g_p.y; e->wep=3; }
  g_p.wanted=1; g_p.heat=100;
  int busted=0;
  for(int k=0;k<600 && !busted;k++){ game_step(); busted=g_p.busted; }
  int32_t cd=0x7FFFFFFF;
  for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_COP){
    int32_t d=wr_dist(g_ent[i].x-g_p.x,g_ent[i].y-g_p.y); if(d<cd)cd=d;}
  for(int f=0;f<50;f++) render_frame(NULL,GRect(0,0,200,228));
  snprintf(b,sizeof b,"busted=%d officer at %ld",busted,(long)cd);
  ck("arrest stages an officer for the cuffing",busted && cd<120,b);

  // --- every gun shop door opens onto walkable ground --------------------
  int bad=0, shops=0;
  for(uint32_t seed=1;seed<=6;seed++){
    wr_seed(seed); audio_init(); game_init();
    for(int s2=0;s2<40;s2++){
      // wander the city cache around random points to sample many lots
      g_p.x=(int32_t)(wr_rand_n(60)+30)*TILE; g_p.y=(int32_t)(wr_rand_n(60)+30)*TILE;
      city_refresh(g_p.x,g_p.y);
      for(int li=0;li<g_nlot;li++){
        if(g_lot[li].kind!=LOT_GUNSHOP)continue;
        shops++;
        int32_t dx,dy,nx,ny; city_shop_door(&g_lot[li],&dx,&dy,&nx,&ny);
        // The bar that matters: can a person stand on the door mat?
        if(city_blocked(dx+nx*60,dy+ny*60,false)) bad++;
      }
    }
  }
  snprintf(b,sizeof b,"%d shops sampled, %d with a blocked door",shops,bad);
  ck("gun shop doors face walkable ground",shops>10 && bad==0,b);

  // --- downtown towers ----------------------------------------------------
  wr_seed(6); audio_init(); game_init();
  g_p.x=ISLAND_CX*TILE; g_p.y=ISLAND_CY*TILE; city_refresh(g_p.x,g_p.y);
  int tall=0; int16_t tallest=0;
  for(int li=0;li<g_nlot;li++){
    if(g_lot[li].h>tallest)tallest=g_lot[li].h;
    if(g_lot[li].h>=6*TILE)tall++;
  }
  snprintf(b,sizeof b,"%d towers >=6 storeys, tallest %d storeys",tall,tallest/TILE);
  ck("downtown has a high-rise core",tall>=4 && tallest>=8*TILE,b);

  // --- wrecks stay put, and you can crash into them -----------------------
  wr_seed(6); audio_init(); game_init();
  for(int i=0;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
  { int32_t tx=g_p.x, ty=g_p.y; uint16_t ta=0; road_snap(&tx,&ty,&ta);
    Ent*w=&g_ent[0]; memset(w,0,sizeof(Ent));
    w->type=ENT_CAR; w->sub=CAR_SEDAN; w->hp=10; w->state=ST_DRIVE;
    w->x=tx; w->y=ty; w->ang=ta;
    vehicle_destroy(0,false);
    int32_t wx=w->x, wy=w->y;
    for(int k=0;k<120;k++) step_only(1);
    int moved = wr_dist(w->x-wx,w->y-wy);
    // Now drive a live car into the wreck.
    Ent*l=&g_ent[1]; memset(l,0,sizeof(Ent));
    l->type=ENT_CAR; l->sub=CAR_SPORTS; l->hp=100; l->state=ST_DRIVE;
    l->x=wx-((fcos(ta)*900)>>10); l->y=wy-((fsin(ta)*900)>>10);
    l->ang=ta; l->spd=17; l->timer=1;
    int hit=0; int32_t wx2=w->x, wy2=w->y;
    for(int k=0;k<120;k++){ step_only(2); if(l->hp<100) hit=1; }
    snprintf(b,sizeof b,"wreck drifted %d, live car hp %d, wreck moved on impact %ld",
             moved,l->hp,(long)wr_dist(w->x-wx2,w->y-wy2));
    ck("a wreck stays put and can be crashed into",
       moved<8 && w->state==ST_DEAD && wr_dist(w->x-wx2,w->y-wy2)<8, b);
  }

  // --- crash physics: the player rams traffic ------------------------------
  // AI traffic now avoids crashing into itself (lanes, following, lights),
  // so the crash path that matters is the one the player drives. Ram the
  // same car repeatedly at full speed: the first hit must never wreck it,
  // debris must fly, the impact must knock the player's own speed down,
  // and only several crashes finish the job.
  wr_seed(6); audio_init(); game_init();
  for(int i=0;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
  { int32_t tx=g_p.x, ty=g_p.y; uint16_t ta=0; road_snap(&tx,&ty,&ta);
    Ent*pc=&g_ent[0], *tgt=&g_ent[1];
    memset(pc,0,sizeof(Ent));
    pc->type=ENT_CAR; pc->sub=CAR_SPORTS; pc->hp=100; pc->occupied=1;
    g_p.in_car=1;
    memset(tgt,0,sizeof(Ent));
    tgt->type=ENT_CAR; tgt->sub=CAR_SEDAN; tgt->hp=100; tgt->state=ST_DRIVE;
    int crashes=0, first_tgt_hp=-1, saw_debris=0, felt_it=0;
    int32_t px0=tx+((fcos(ta)*520)>>10), py0=ty+((fsin(ta)*520)>>10);
    while(crashes<10 && tgt->state!=ST_DEAD){
      uint8_t hp0=tgt->hp;
      // Fresh run-up each time into a PARKED car - the thing a player
      // actually rams. Pin it so the AI cannot drive it out of the way.
      tgt->x=px0; tgt->y=py0; tgt->ang=(uint16_t)(ta+16384);
      tgt->spd=0; tgt->timer=1; tgt->state=ST_IDLE;
      g_p.x=tx-((fcos(ta)*300)>>10); g_p.y=ty-((fsin(ta)*300)>>10);
      g_p.ang=ta; g_p.spd=30;
      pc->hit_cd=0; tgt->hit_cd=0;
      int contact=0;
      for(int k=0;k<50;k++){
        g_p.stick_f=256;
        int16_t before_spd=g_p.spd;
        if(tgt->state!=ST_DEAD){ tgt->x=px0; tgt->y=py0; tgt->spd=0; }
        step_only(2);
        if(tgt->hp<hp0 || tgt->state==ST_DEAD){
          contact=1;
          if(g_p.spd<before_spd) felt_it=1;      // the impact cost him speed
          break;
        }
      }
      int g1=0; for(int i2=0;i2<MAX_GIB;i2++) if(g_gib[i2].life) g1++;
      if(g1>0) saw_debris=1;
      if(!contact) break;
      crashes++;
      if(crashes==1) first_tgt_hp=(tgt->state==ST_DEAD)?0:tgt->hp;
    }
    g_p.in_car=0;
    snprintf(b,sizeof b,"first ram left target at hp %d, debris=%d, felt=%d, wreck after %d rams",
             first_tgt_hp,saw_debris,felt_it,crashes);
    ck("a first crash never wrecks a car",
       first_tgt_hp>30 && saw_debris && felt_it, b);
    ck("repeated crashes do wreck it",
       crashes>=3 && crashes<10 && tgt->state==ST_DEAD, b);
  }

  // --- auto-aim ------------------------------------------------------------
  wr_seed(6); audio_init(); game_init();
  for(int i=0;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
  { Ent*t=&g_ent[0]; memset(t,0,sizeof(Ent));
    t->type=ENT_PED; t->sub=PED_MAN; t->hp=100; t->state=ST_IDLE;
    uint16_t bearing=(uint16_t)(g_p.ang+4200);        // ~23 degrees off
    t->x=g_p.x+((fcos(bearing)*600)>>10);
    t->y=g_p.y+((fsin(bearing)*600)>>10);
    g_p.weapon=W_PISTOL; g_p.ammo=5; g_p.attack_cd=0;
    for(int i=0;i<MAX_BULLET;i++) g_bullet[i].life=0;
    game_input_action();
    int bi=-1; for(int i=0;i<MAX_BULLET;i++) if(g_bullet[i].life){bi=i;break;}
    int32_t err=99999;
    if(bi>=0){
      // heading of the bullet vs bearing to the target
      uint16_t ba=angle_to(0,0,g_bullet[bi].vx,g_bullet[bi].vy);
      err=wr_abs((int16_t)(uint16_t)(ba-bearing));
    }
    snprintf(b,sizeof b,"bullet fired=%d, aim error %ld (target 23deg off centre)",bi>=0,(long)err);
    ck("guns lock onto the target nearest the aim", bi>=0 && err<900, b);
  }

  // --- no lot straddles any road ------------------------------------------
  { long badlots=0, nlots=0;
    for(uint32_t seed=1;seed<=3;seed++){
      wr_seed(seed); audio_init(); game_init();
      for(int a2=0;a2<40;a2++){
        g_p.x=(int32_t)(wr_rand_n(70)+20)*TILE; g_p.y=(int32_t)(wr_rand_n(70)+20)*TILE;
        city_refresh(g_p.x,g_p.y);
        for(int li=0;li<g_nlot;li++){
          Lot*L=&g_lot[li]; nlots++;
          for(int i2=0;i2<3;i2++)for(int j2=0;j2<3;j2++){
            int32_t x=L->x0+((L->x1-L->x0)*i2)/2, y=L->y0+((L->y1-L->y0)*j2)/2;
            if(cell_at_point(x,y)!=CELL_BLD){badlots++;goto nextlot;}
            { int32_t bd; if(on_bridge(x,y,&bd,NULL)&&bd<BRIDGE_KERB){badlots++;goto nextlot;} }
          }
          nextlot:;
        }
      }
    }
    snprintf(b,sizeof b,"%ld lots, %ld on a road/beach/bridge",nlots,badlots);
    ck("no building straddles a road", nlots>1000 && badlots==0, b);
  }

  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
