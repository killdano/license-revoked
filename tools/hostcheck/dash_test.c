#include "wr.h"
#include "wr_gfx.h"
#include <stdio.h>
static int bad=0, total=0;
void wr_dash_trace(int32_t x, int32_t y, uint8_t kind){
  total++;
  // distance to the nearest centreline of each family
  int32_t lat[2]; for(int f=0;f<2;f++){ int32_t s=f?(x-y):(x+y); int32_t m=((s%DIAG_PERIOD)+DIAG_PERIOD)%DIAG_PERIOD; if(m>DIAG_PERIOD/2) m-=DIAG_PERIOD; lat[f]=wr_abs((m*724)>>10); }
  int on=(lat[0]<ROAD_HALF+30)+(lat[1]<ROAD_HALF+30);
  int32_t other = lat[0]<lat[1] ? lat[1] : lat[0];
  if(kind==RD_RING){ if(on>0){ bad++; if(bad<6) printf("  ring dash at (%d,%d) on a diagonal street (lat %d/%d)\n",(int)x,(int)y,(int)lat[0],(int)lat[1]); } return; }
  if(kind!=RD_BRIDGE && (on!=1 || other<ROAD_HALF+300)){ bad++; if(bad<6) printf("  dash at (%d,%d) kind=%d lat0=%d lat1=%d on=%d\n",(int)x,(int)y,kind,(int)lat[0],(int)lat[1],on); }
}
int main(void){ setvbuf(stdout,NULL,_IONBF,0); wr_seed(6); game_init();
  for(int k=0;k<80;k++){ g_p.x=(int32_t)(14+wr_rand_n(52))*TILE; g_p.y=(int32_t)(14+wr_rand_n(52))*TILE; g_p.ang=(uint16_t)wr_rand_n(65536); g_p.zoom=200; city_refresh(g_p.x,g_p.y); render_frame(NULL,GRect(0,0,SCR_W,SCR_H)); }
  printf("  %-56s %s  %d dashes painted, %d inside a junction's box\n","the painter keeps every dash out of every junction",bad==0?"ok  ":"FAIL",total,bad); return bad?1:0; }
