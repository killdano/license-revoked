// Render frames headless and dump them as PPM so a human (or Claude) can look.
// Usage: snap <out_prefix> [scenario]
#include "wr.h"
#include "wr_domain.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern unsigned char *stub_fb(void);
static void dump(const char *name){
  FILE *f=fopen(name,"wb"); if(!f) return;
  fprintf(f,"P6\n200 228\n255\n");
  unsigned char *fb=stub_fb();
  for(int i=0;i<200*228;i++){ unsigned char c=fb[i];
    unsigned char rgb[3]={ (unsigned char)(((c>>4)&3)*85), (unsigned char)(((c>>2)&3)*85), (unsigned char)((c&3)*85) };
    fwrite(rgb,1,3,f);}
  fclose(f);
}
static void frames(int n){ for(int i=0;i<n;i++){ game_step(); render_frame(NULL,GRect(0,0,200,228)); } }
int main(int argc,char**argv){
  const char *pre = argc>1?argv[1]:"/tmp/snap";
  const char *sc  = argc>2?argv[2]:"street";
  wr_seed(6); audio_init(); game_init();
  char name[256];
  if(!strcmp(sc,"street")){
    // on foot at a junction, a few zoom levels
    int32_t x=g_p.x,y=g_p.y; uint16_t a; road_snap(&x,&y,&a); g_p.x=x; g_p.y=y; g_p.ang=a;
    int zs[3]={0,128,256};
    for(int k=0;k<3;k++){ g_p.zoom=(int16_t)zs[k]; frames(60); snprintf(name,sizeof name,"%s_%s_z%d.ppm",pre,sc,zs[k]); dump(name); }
  } else if(!strcmp(sc,"drive")){
    int32_t x=g_p.x,y=g_p.y; uint16_t a; road_snap(&x,&y,&a); g_p.x=x; g_p.y=y; g_p.ang=a;
    memset(g_ent,0,sizeof(g_ent));
    g_ent[0].type=ENT_CAR; g_ent[0].hp=100; g_ent[0].state=ST_DRIVE; g_ent[0].sub=CAR_SEDAN;
    g_ent[0].x=g_p.x+40; g_ent[0].y=g_p.y; g_ent[0].ang=a; game_input_action();
    game_touch_down(STICK_HOME_X,STICK_HOME_Y); game_touch_move(STICK_HOME_X,(int16_t)(STICK_HOME_Y-STICK_RANGE));
    for(int k=0;k<4;k++){ frames(60); snprintf(name,sizeof name,"%s_%s_%d.ppm",pre,sc,k); dump(name); }
  } else if(!strcmp(sc,"cars") || !strcmp(sc,"carslow")){
    // a line-up of every car model in front of the player, on foot
    int32_t x=g_p.x,y=g_p.y; uint16_t a; road_snap(&x,&y,&a); g_p.x=x; g_p.y=y; g_p.ang=a; g_p.zoom=sc[4]=='l'?0:100;
    memset(g_ent,0,sizeof(g_ent));
    int32_t c=fcos(a),s=fsin(a);
    for(int m=0;m<CAR_MODEL_COUNT;m++){
      Ent *e=&g_ent[m]; e->type=ENT_CAR; e->hp=100; e->state=ST_PARK; e->sub=(uint8_t)m;
      int32_t fw=(sc[4]=='l')?(260+(m/3)*200):(300+(m/3)*260), sd=(m%3-1)*(sc[4]=='l'?170:230);
      e->x=g_p.x+fmul(fw,c)-fmul(sd,s); e->y=g_p.y+fmul(fw,s)+fmul(sd,c); e->ang=(uint16_t)(a+ (sc[4]=='l'?16384+3000:(m&1?9000:0)));
    }
    frames(60); snprintf(name,sizeof name,"%s_%s.ppm",pre,sc); dump(name);
  } else if(!strcmp(sc,"at")){
    // snap <pre> at X Y ANG ZOOM [incar]
    g_p.x=atoi(argv[3]); g_p.y=atoi(argv[4]); g_p.ang=(uint16_t)atoi(argv[5]); g_p.zoom=(int16_t)atoi(argv[6]);
    domain_scenery(); city_refresh(g_p.x,g_p.y); props_refresh();    // the island's own furniture
    if(argc>7 && atoi(argv[7])){ memset(g_ent,0,sizeof(g_ent)); g_ent[0].type=ENT_CAR; g_ent[0].hp=100; g_ent[0].state=ST_PARK; g_ent[0].sub=CAR_SEDAN;
      g_ent[0].x=g_p.x+40; g_ent[0].y=g_p.y; g_ent[0].ang=g_p.ang; game_input_action(); }
    frames(40); snprintf(name,sizeof name,"%s.ppm",pre); dump(name);
  } else if(!strcmp(sc,"sideon")){
    int32_t x=g_p.x,y=g_p.y; uint16_t a; road_snap(&x,&y,&a); g_p.x=x; g_p.y=y; g_p.ang=a; g_p.zoom=0;
    memset(g_ent,0,sizeof(g_ent)); int32_t c=fcos(a),s2=fsin(a);
    for(int k=0;k<3;k++){ Ent*e=&g_ent[k]; e->type=ENT_PED; e->sub=PED_MAN; e->hp=100; e->state=ST_IDLE; e->x=g_p.x+fmul(120+k*40,c)+fmul((k-1)*70,s2); e->y=g_p.y+fmul(120+k*40,s2)-fmul((k-1)*70,c); e->ang=(uint16_t)(a+16384+k*8000); e->phase=(uint8_t)(k*60); }
    for(int k=0;k<3;k++) render_frame(NULL,GRect(0,0,200,228));
    snprintf(name,sizeof name,"%s.ppm",pre); dump(name);
  } else if(!strcmp(sc,"boats")){
    int32_t x=g_p.x,y=g_p.y; uint16_t a; road_snap(&x,&y,&a); g_p.x=x; g_p.y=y; g_p.ang=a; g_p.zoom=60;
    memset(g_ent,0,sizeof(g_ent)); int32_t c=fcos(a),s2=fsin(a);
    for(int m=0;m<BOAT_MODEL_COUNT;m++){ Ent *e=&g_ent[m]; e->type=m==BOAT_POLICE?ENT_COPBOAT:ENT_BOAT; e->hp=100; e->state=ST_PARK; e->sub=(uint8_t)m; e->spd=(m&1)?12:0;
      int32_t fw=300+(m/2)*260, sd=(m%2)*300-150; e->x=g_p.x+fmul(fw,c)-fmul(sd,s2); e->y=g_p.y+fmul(fw,s2)+fmul(sd,c); e->ang=(uint16_t)(a+14000); }
    Ent *k=&g_ent[6]; k->type=ENT_CAR; k->hp=100; k->state=ST_PARK; k->sub=CAR_BIKE; k->x=g_p.x+fmul(200,c)+fmul(200,s2); k->y=g_p.y+fmul(200,s2)-fmul(200,c); k->ang=(uint16_t)(a+9000);
    frames(20); snprintf(name,sizeof name,"%s.ppm",pre); dump(name);
  } else if(!strcmp(sc,"venue")){
    // stand outside the first club, facing its door
    for(int i=0;i<g_nvenue;i++) if(g_venue[i].club){ int32_t cx=(int32_t)g_venue[i].x<<2, cy=(int32_t)g_venue[i].y<<2; g_p.x=cx; g_p.y=cy; city_refresh(cx,cy);
      int li=lot_at(cx,cy); int32_t dx,dy,nx,ny; city_shop_door(&g_lot[li],&dx,&dy,&nx,&ny); g_p.x=dx+nx*330; g_p.y=dy+ny*330; g_p.ang=angle_to(g_p.x,g_p.y,dx,dy); g_p.zoom=atoi(argv[3]); break; }
    props_refresh(); pickups_refresh(); memset(g_ent,0,sizeof g_ent);
    for(int k=0;k<20;k++) render_frame(NULL,GRect(0,0,200,228));
    snprintf(name,sizeof name,"%s.ppm",pre); dump(name);
  } else if(!strcmp(sc,"diner")){
    g_p.x=ISLAND_BX*TILE; g_p.y=ISLAND_BY*TILE; g_p.cash=100; g_p.hp=60;
    inside_enter(IN_GUNSHOP, g_p.x, g_p.y, 0, 1);
    for(int f=0;f<40;f++){ if(f>5&&f<30&&(f%4)==0){ g_p.attack_cd=0; game_input_action(); } game_step(); render_frame(NULL,GRect(0,0,200,228)); }
    snprintf(name,sizeof name,"%s.ppm",pre); dump(name); printf("hp after eating: %d\n",g_p.hp);
  } else if(!strcmp(sc,"club") || !strcmp(sc,"store")){
    // snap <pre> club|store [frames] [cash] [face_door]
    int fr=argc>3?atoi(argv[3]):40; int cash=argc>4?atoi(argv[4]):0; int face=argc>5?atoi(argv[5]):0;
    g_p.cash=(uint16_t)cash;
    inside_enter(sc[0]=='c'?IN_CLUB:IN_GUNSHOP, g_p.x, g_p.y, 0, 1);
    if(face){ g_p.ang=16384; g_p.y=ROOM_OY+g_in.h/2; }
    // walk up toward the stage/counter and throw everything
    for(int k=0;k<fr;k++){ if(k<30){ g_p.stick_f=256; g_p.stick_on=1; } else { g_p.stick_f=0; g_p.stick_on=0; }
      if(k>=30 && g_p.cash>=10) game_input_action(); game_step(); render_frame(NULL,GRect(0,0,200,228)); }
    snprintf(name,sizeof name,"%s.ppm",pre); dump(name);
  } else if(!strcmp(sc,"helipark")){
    int32_t x=g_p.x,y=g_p.y; uint16_t a; road_snap(&x,&y,&a); g_p.x=x; g_p.y=y; g_p.ang=a; g_p.zoom=atoi(argv[3]);
    memset(g_ent,0,sizeof(g_ent)); Ent*h=&g_ent[3]; h->type=ENT_HELI; h->state=ST_PARK; h->hp=200; h->z=atoi(argv[4]);
    h->x=g_p.x+fmul(380,fcos(a)); h->y=g_p.y+fmul(380,fsin(a)); h->ang=(uint16_t)(a+20000);
    for(int k=0;k<30;k++){ render_frame(NULL,GRect(0,0,200,228)); h->phase+=40; }
    snprintf(name,sizeof name,"%s.ppm",pre); dump(name);
  } else if(!strcmp(sc,"heli")){
    int32_t x=g_p.x,y=g_p.y; uint16_t a; road_snap(&x,&y,&a); g_p.x=x; g_p.y=y; g_p.ang=a; g_p.zoom=120;
    int last=-1;
    for(int k=0;k<420;k++){ g_p.wanted=4; g_p.heat=330; g_p.evade=0; g_p.hp=100; g_p.dead=0; game_step(); render_frame(NULL,GRect(0,0,200,228));
      int hi=-1; for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_HELI) hi=i;
      int st = hi>=0 ? g_ent[hi].state : -1;
      if((k>=40 && k<=200 && k%20==0) || (st!=last && hi>=0)){ snprintf(name,sizeof name,"%s_%s_%03d_st%d.ppm",pre,sc,k,st); dump(name); }
      last=st; }
  }
  return 0;
}
