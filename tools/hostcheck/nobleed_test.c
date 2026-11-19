#include "wr.h"
#include <stdio.h>
static int fails=0;
static void ck(const char*n,int ok,const char*d){printf("  %-48s %s  %s\n",n,ok?"ok  ":"FAIL",d);if(!ok)fails++;}
static int blood_pools(void){
  int n=0; for(int i=0;i<MAX_BLOOD;i++) if(g_blood[i].life) n++;
  return n;
}
int main(void){
  char b[170];
  setvbuf(stdout,NULL,_IONBF,0);

  // Shot: blood, as before.
  wr_seed(6); audio_init(); game_init();
  for(int i=0;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
  { Ent*t=&g_ent[0]; memset(t,0,sizeof(Ent));
    t->type=ENT_PED; t->hp=100; t->state=ST_IDLE;
    t->x=g_p.x+((fcos(g_p.ang)*200)>>10); t->y=g_p.y+((fsin(g_p.ang)*200)>>10);
    g_p.weapon=W_PISTOL; g_p.ammo=40;
    for(int k=0;k<40 && t->state!=ST_DEAD;k++){ g_p.attack_cd=0; game_input_action(); game_step(); }
    for(int k=0;k<60;k++) game_step();
    snprintf(b,sizeof b,"%d blood pools after shooting him",blood_pools());
    ck("shooting still draws blood", blood_pools()>0, b);
  }

  // Burned: no blood at all.
  wr_seed(6); game_init();
  for(int i=0;i<MAX_ENT;i++) g_ent[i].type=ENT_NONE;
  for(int i=0;i<MAX_BLOOD;i++) g_blood[i].life=0;
  { Ent*t=&g_ent[0]; memset(t,0,sizeof(Ent));
    t->type=ENT_PED; t->hp=100; t->state=ST_IDLE;
    t->x=g_p.x+((fcos(g_p.ang)*200)>>10); t->y=g_p.y+((fsin(g_p.ang)*200)>>10);
    g_p.weapon=W_FLAME; g_p.ammo=120;
    for(int k=0;k<10;k++){ g_p.attack_cd=0; game_input_action(); game_step(); }
    int lit=t->fire>0;
    for(int k=0;k<400 && t->state!=ST_DEAD;k++) game_step();
    for(int k=0;k<80;k++) game_step();
    snprintf(b,sizeof b,"lit=%d, dead=%d, %d blood pools",lit,t->state==ST_DEAD,blood_pools());
    ck("burning to death leaves no blood", lit && t->state==ST_DEAD && blood_pools()==0, b);
  }
  printf("\n%d failure(s)\n",fails);
  return fails?1:0;
}
