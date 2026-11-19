#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-44s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
int main(void){
  char b[128];
  wr_seed(6); audio_init(); game_init();
  g_stats.kills=0; g_stats.thefts=0; g_stats.best_escape=0; wr_stats_save();

  // Die with a tally and it should be banked.
  g_p.kills=7; g_p.stolen=3;
  g_p.hp=0; for(int k=0;k<3;k++) game_step();
  snprintf(b,sizeof b,"lifetime kills=%lu thefts=%lu",
    (unsigned long)g_stats.kills,(unsigned long)g_stats.thefts);
  ck("death banks the tally", g_stats.kills==7 && g_stats.thefts==3, b);

  // And it survives a fresh start.
  wr_seed(6); game_init();
  snprintf(b,sizeof b,"after restart kills=%lu",(unsigned long)g_stats.kills);
  ck("the tally survives a restart", g_stats.kills==7, b);

  // A clean escape records a best time.
  g_p.wanted=2; g_p.heat=160; g_p.evade=0;
  for(int k=0;k<40;k++) game_step();
  g_p.heat=0; g_p.wanted=0;
  for(int k=0;k<4;k++) game_step();
  snprintf(b,sizeof b,"best escape %lu frames",(unsigned long)g_stats.best_escape);
  ck("a clean getaway records a best time", g_stats.best_escape>0, b);

  // A slower one must not overwrite a faster one.
  uint32_t first=g_stats.best_escape;
  g_p.wanted=2; g_p.heat=160;
  for(int k=0;k<200;k++) game_step();
  g_p.heat=0; g_p.wanted=0;
  for(int k=0;k<4;k++) game_step();
  snprintf(b,sizeof b,"was %lu, now %lu",(unsigned long)first,(unsigned long)g_stats.best_escape);
  ck("a slower run does not overwrite the best", g_stats.best_escape<=first, b);

  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
