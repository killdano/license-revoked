#include "wr.h"
#include <stdio.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
unsigned char *stub_fb(void);

static int inv_ground(double sx,double sy,int zoom,double px,double py,double ang,int mirror,
                      double *wx,double *wy,double *dist){
  double back = CAM_BACK_NEAR + ((double)(CAM_BACK_FAR-CAM_BACK_NEAR)*zoom)/256.0;
  double h    = CAM_H_NEAR    + ((double)(CAM_H_FAR   -CAM_H_NEAR   )*zoom)/256.0;
  double deg  = PITCH_NEAR + ((double)(PITCH_FAR-PITCH_NEAR)*zoom)/256.0;
  double pit  = deg*M_PI/180.0, cp=cos(pit), sp=sin(pit);
  double depth0 = back*cp + h*sp; if(depth0<NEAR_Z) depth0=NEAR_Z;
  double up0    = back*sp - h*cp;
  double target = PLAYER_Y_NEAR + ((double)(PLAYER_Y_FAR-PLAYER_Y_NEAR)*zoom)/256.0;
  double pcy    = target + up0*FOCAL/depth0;
  double pcx    = mirror ? (SCR_W - PROJ_CX) : PROJ_CX;
  double A=(sx-pcx)/FOCAL, B=(pcy-sy)/FOCAL;
  double den = sp - B*cp;
  if (den <= 1e-6) return 0;
  double ry2 = h*(cp + B*sp)/den;
  double d   = ry2*cp + h*sp;
  if (d < NEAR_Z*0.5) return 0;
  double rx = A*d, ry = ry2 - back;
  double yawoff = mirror ? -CAM_YAW_OFF : CAM_YAW_OFF;
  double yaw = (ang + yawoff)*2.0*M_PI/65536.0, cy=cos(yaw), sy2=sin(yaw);
  *wx = px + ry*cy - rx*sy2;
  *wy = py + ry*sy2 + rx*cy;
  *dist = d;
  return 1;
}

static long settle_and_render(int n){ for(int i=0;i<n;i++) render_frame(NULL,GRect(0,0,200,228)); return 0; }

// Find the player's white shirt centre column.
static int player_x(void){
  unsigned char*fb=stub_fb(); long sx=0,n=0;
  for(int y=14;y<228;y++)for(int x=0;x<200;x++)
    if(fb[y*200+x]==0xFF){ sx+=x; n++; }
  return n? (int)(sx/n) : -1;
}

int main(void){
  wr_seed(6); audio_init(); game_init();
  g_p.zoom=40;

  // --- 1. Mirror mode: character swaps sides of the frame -------------------
  g_menu.cam_left=0; settle_and_render(200);
  int px_r = player_x();
  g_menu.cam_left=1; settle_and_render(5);
  int px_l = player_x();
  printf("mirror: player column right-shoulder=%d, left-shoulder=%d (screen mid 100)\n", px_r, px_l);

  // Slider touch zones respond on the correct edges.
  g_menu.cam_left=0;
  game_touch_down(195,100); int r_right=g_p.zoom_on; game_touch_up();
  game_touch_down(5,100);   int r_left =g_p.zoom_on; game_touch_up();
  g_menu.cam_left=1;
  game_touch_down(5,100);   int l_left =g_p.zoom_on; game_touch_up();
  game_touch_down(195,100); int l_right=g_p.zoom_on; game_touch_up();
  printf("slider zones: normal right=%d left=%d | mirrored left=%d right=%d (want 1 0 1 0)\n",
         r_right, r_left, l_left, l_right);
  g_menu.cam_left=0;

  // --- 2. Full render-vs-terrain consistency scan ---------------------------
  // Sample many positions across both islands and headings; for every ground
  // pixel, inverse-project and check the colour is plausible for the cell.
  long checked=0, road_over_sea=0, water_over_city=0, line_off_road=0, sand_in_city=0;
  int zoom=40;
  struct { int32_t tx,ty; } POS[6] = {
    {ISLAND_CX,ISLAND_CY},{ISLAND_CX+20,ISLAND_CY},{ISLAND_CX,ISLAND_CY-21},
    {ISLAND_BX,ISLAND_BY},{ISLAND_BX-14,ISLAND_BY},{(ISLAND_CX+ISLAND_BX)/2,(ISLAND_CY+ISLAND_BY)/2}
  };
  for(int p=0;p<6;p++){
    g_p.x=POS[p].tx*TILE; g_p.y=POS[p].ty*TILE;
    for(int k=0;k<8;k++){
      g_p.ang=(uint16_t)(k*8192);
      settle_and_render(p==0&&k==0?200:3);
      unsigned char*fb=stub_fb();
      int y0=(int)(g_horizon+14); if(y0<14) y0=14;
      for(int y=y0;y<228;y+=2)for(int x=0;x<200;x+=2){
        uint8_t c=fb[y*200+x]&0x3F;
        double wx,wy,d;
        if(!inv_ground(x+0.5,y+0.5,zoom,g_p.x,g_p.y,g_p.ang,0,&wx,&wy,&d)) continue;
        if(d>2600) continue;                        // beyond terrain draw radius
        uint8_t cell=cell_at_point((int32_t)wx,(int32_t)wy);
        double ta=hypot(wx/TILE-ISLAND_CX,wy/TILE-ISLAND_CY);
        double tb=hypot(wx/TILE-ISLAND_BX,wy/TILE-ISLAND_BY);
        int open_sea = (cell==CELL_WATER) && ta>SAND_R+2 && tb>SAND_BR+2;
        int32_t bd; if(open_sea&&on_bridge((int32_t)wx,(int32_t)wy,&bd,NULL)&&bd<BRIDGE_KERB+3*TILE) open_sea=0;
        int deep_city = (ta<CITY_R-9)||(tb<CITY_BR-9);
        checked++;
        if((c==0x15||c==0x2A)&&open_sea){
          road_over_sea++;
          if(road_over_sea<=6){
            int32_t bd2=0; bool ob2=on_bridge((int32_t)wx,(int32_t)wy,&bd2,NULL);
            printf("  offender: colour %02x at (%.1f,%.1f) tiles, dA=%.1f dB=%.1f "
                   "cell=%d on_bridge=%d bd=%ld\n",
              c, wx/TILE, wy/TILE, ta, tb, cell, ob2?1:0, (long)bd2);
          }
        }
        if((c==0x07||c==0x02)&&deep_city&&cell!=CELL_WATER) water_over_city++;
        if(c==0x3D&&cell!=CELL_ROAD&&cell!=CELL_PAVE) line_off_road++;
        if(c==0x3D&&(cell==CELL_SAND||cell==CELL_WATER)) line_off_road++;
      }
    }
  }
  printf("consistency: %ld px checked | road-over-sea %ld | water-over-city %ld | lines-off-road %ld\n",
         checked, road_over_sea, water_over_city, line_off_road);
  (void)sand_in_city;
  return 0;
}
