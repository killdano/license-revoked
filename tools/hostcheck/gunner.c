#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){ printf("  %-42s %s   %s\n",n,ok?"ok  ":"FAIL",d); if(!ok)fails++; }
int main(void){
  char b[110];
  wr_seed(11); audio_init(); game_init();
  g_p.wanted=5; g_p.hostile=1; g_p.copkills=2; g_p.hot=800; g_p.heat=470; g_p.evade=0;
  for(int f=0;f<300;f++){ g_p.hp=100; g_p.dead=0; game_step(); }
  int heli=-1; for(int i=0;i<MAX_ENT;i++) if(g_ent[i].type==ENT_HELI){heli=i;break;}
  snprintf(b,sizeof b,"helicopter %s",heli>=0?"on station":"absent");
  ck("air support arrives", heli>=0, b);
  if(heli<0){ printf("\n%d failure(s)\n",++fails); return 1; }

  // Remove ground units so anything that hits us came from the sky.
  for(int i=0;i<MAX_ENT;i++)
    if(g_ent[i].type==ENT_COP||g_ent[i].type==ENT_COPCAR) g_ent[i].type=ENT_NONE;
  int rounds=0, hits=0, hp=100;
  for(int f=0;f<400;f++){
    g_ent[heli].x=g_p.x+3*TILE; g_ent[heli].y=g_p.y; g_ent[heli].z=HELI_ALT;
    int before=0; for(int i=0;i<MAX_BULLET;i++) before+=g_bullet[i].life?1:0;
    g_p.dead=0;
    game_step();
    int after=0; for(int i=0;i<MAX_BULLET;i++) after+=g_bullet[i].life?1:0;
    if(after>before) rounds+=after-before;
    if(g_p.hp<hp){ hits++; }
    hp=g_p.hp; if(hp<40){ g_p.hp=100; hp=100; }
  }
  snprintf(b,sizeof b,"%d rounds fired from the air, %d landed on the player",rounds,hits);
  ck("door gunner opens fire", rounds>=8, b);   // capped by the global fire rate
  ck("and can actually hit you", hits>0, b);

  // it should not shoot when you are clean
  wr_seed(11); game_init();
  g_p.wanted=0; g_p.heat=0;
  int quiet=0;
  for(int f=0;f<300;f++){ g_p.hp=100; g_p.dead=0; game_step();
    for(int i=0;i<MAX_BULLET;i++) if(g_bullet[i].life&&g_bullet[i].from_cop) quiet++; }
  snprintf(b,sizeof b,"%d police rounds in the air at zero stars",quiet);
  ck("no gunfire when you are not wanted", quiet==0, b);

  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
