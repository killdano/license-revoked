// Trace AI traffic paths over the map. Writes /tmp/traffic.ppm and prints stats.
#include "wr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define S 20
static unsigned char *img; static int pw, ph; static int X0, Y0;
static void px(int32_t x, int32_t y, int r, int g, int b){
  int qx=((x-X0*TILE)*S)/TILE, qy=((y-Y0*TILE)*S)/TILE;
  if(qx<0||qy<0||qx>=pw||qy>=ph) return;
  unsigned char *p=img+((size_t)qy*pw+qx)*3; p[0]=r;p[1]=g;p[2]=b;
}
int main(int argc,char**argv){
  int frames=argc>1?atoi(argv[1]):1500; int cop=argc>2?atoi(argv[2]):0;
  wr_seed(6); audio_init(); game_init();
  // centre the map on the player
  X0=(g_p.x>>8)-16; Y0=(g_p.y>>8)-16; int W=32,H=32; pw=W*S; ph=H*S; img=calloc((size_t)pw*ph*3,1);
  for(int qy=0;qy<ph;qy++)for(int qx=0;qx<pw;qx++){
    int32_t x=X0*TILE+(qx*TILE)/S, y=Y0*TILE+(qy*TILE)/S; uint8_t c=cell_at_point(x,y);
    unsigned char r=0,g=0,b=0;
    switch(c){case CELL_ROAD:r=g=b=60;break;case CELL_PAVE:r=g=b=150;break;case CELL_BLD:r=190;g=170;b=110;break;case CELL_PARK:r=40;g=150;b=40;break;case CELL_SAND:r=230;g=220;b=130;break;default:r=30;g=70;b=190;}
    unsigned char *p=img+((size_t)qy*pw+qx)*3; p[0]=r;p[1]=g;p[2]=b;}
  if(cop){ g_p.wanted=3; g_p.heat=250; }
  static int32_t sx[MAX_ENT], sy[MAX_ENT]; static long dist[MAX_ENT], onroad[MAX_ENT], seen[MAX_ENT]; static int turns[MAX_ENT], stalls[MAX_ENT], lastwep[MAX_ENT];
  static int32_t lx[MAX_ENT], ly[MAX_ENT];
  int uturns=0;
  for(int f=0;f<frames;f++){
    game_step();
    for(int i=0;i<MAX_ENT;i++){ Ent*e=&g_ent[i]; bool car=(e->type==ENT_CAR&&e->state==ST_DRIVE)||e->type==ENT_COPCAR||e->type==ENT_CYCLIST||e->type==ENT_AMB;
      if(!car){ seen[i]=0; continue; }
      if(!seen[i]){ seen[i]=1; sx[i]=e->x; sy[i]=e->y; lx[i]=e->x; ly[i]=e->y; dist[i]=0; onroad[i]=0; lastwep[i]=0; }
      dist[i]+=wr_dist(e->x-lx[i],e->y-ly[i]); lx[i]=e->x; ly[i]=e->y;
      if(cell_at_point(e->x,e->y)==CELL_ROAD) onroad[i]++;
      if(e->spd<=1) stalls[i]++;
      if(e->wep!=lastwep[i]){ if(e->wep==2||e->wep==3) turns[i]++; if(e->wep==4) uturns++; lastwep[i]=e->wep; }
      int r=e->type==ENT_COPCAR?0:(e->type==ENT_CYCLIST?255:255), g=e->type==ENT_COPCAR?0:(e->type==ENT_CYCLIST?255:(i*37)&255), b=e->type==ENT_COPCAR?255:(e->type==ENT_CYCLIST?0:(i*91)&255);
      if(e->type==ENT_CAR){ r=255; g=(i*37)&255; b=(i*91)&255; }
      px(e->x,e->y,r,g,b);
    }
  }
  px(g_p.x,g_p.y,255,255,255);
  int ncars=0, moving=0; long tot=0, road=0, frames_on=0;
  for(int i=0;i<MAX_ENT;i++) if(seen[i]){ ncars++; tot+=dist[i]; if(dist[i]>4*TILE) moving++; }
  for(int i=0;i<MAX_ENT;i++){ Ent*e=&g_ent[i]; if(!seen[i]) continue; printf("  ent %2d type %d: moved %5ld units, onroad %ld, stalls %d, turns %d\n",i,e->type,dist[i],onroad[i],stalls[i],turns[i]); }
  printf("vehicles alive at end: %d, moved >4 tiles: %d, u-turns: %d\n",ncars,moving,uturns);
  FILE*f=fopen("/tmp/traffic.ppm","wb"); fprintf(f,"P6\n%d %d\n255\n",pw,ph); fwrite(img,1,(size_t)pw*ph*3,f); fclose(f);
  return 0;
}
