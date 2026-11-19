// Top-down map of the classifier plus junctions, as a PPM. 4 px per tile.
#include "wr.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv){
  const char *out = argc>1?argv[1]:"/tmp/world.ppm";
  int S = argc>2?atoi(argv[2]):4;             // px per tile
  int X0 = 0, Y0 = 0, W = 132, H = 90;         // tiles
  wr_seed(6); audio_init(); game_init();
  int pw=W*S, ph=H*S;
  unsigned char *img=calloc((size_t)pw*ph*3,1);
  for(int py=0;py<ph;py++) for(int px=0;px<pw;px++){
    int32_t x=(X0*TILE)+(px*TILE)/S, y=(Y0*TILE)+(py*TILE)/S;
    uint8_t c=cell_at_point(x,y);
    unsigned char r=0,g=0,b=0;
    switch(c){case CELL_ROAD:r=g=b=70;break;case CELL_PAVE:r=g=b=170;break;
      case CELL_BLD:r=200;g=180;b=120;break;case CELL_PARK:r=40;g=160;b=40;break;
      case CELL_SAND:r=240;g=230;b=140;break;default:r=30;g=80;b=200;}
    unsigned char *p=img+((size_t)py*pw+px)*3; p[0]=r;p[1]=g;p[2]=b;
  }
  // junctions: scan the lattice and mark real ones in red, dead ones in magenta
  for(int a=-2;a<60;a++) for(int b=-40;b<40;b++){
    int32_t u=a*DIAG_PERIOD, v=b*DIAG_PERIOD; int32_t jx=(u+v)/2, jy=(u-v)/2;
    if(cell_at_point(jx,jy)!=CELL_ROAD) continue;
    uint8_t arms=city_junction_arms(jx,jy);
    int px=((jx-X0*TILE)*S)/TILE, py=((jy-Y0*TILE)*S)/TILE;
    for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++){int qx=px+dx,qy=py+dy; if(qx<0||qy<0||qx>=pw||qy>=ph)continue;
      unsigned char *p=img+((size_t)qy*pw+qx)*3; if(arms){p[0]=255;p[1]=0;p[2]=0;} else {p[0]=255;p[1]=0;p[2]=255;}}
  }
  FILE *f=fopen(out,"wb"); fprintf(f,"P6\n%d %d\n255\n",pw,ph); fwrite(img,1,(size_t)pw*ph*3,f); fclose(f);
  return 0;
}
