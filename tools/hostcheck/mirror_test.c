// Mirror mode: the weapon box is where it is drawn and tappable there; the
// radar dot sits on the camera's side of the character, both ways round.
#include "wr.h"
#include "wr_domain.h"
#include <stdio.h>
static int fails;
static void ck(const char *w, bool ok, const char *d){ printf("  %-52s %s  %s\n",w,ok?"ok  ":"FAIL",d); if(!ok) fails++; }
int main(void){ char b[160]; setvbuf(stdout,NULL,_IONBF,0);
  wr_seed(6); game_init(); g_p.weapon=W_PISTOL; g_p.ammo=10; g_p.inv[W_PISTOL]=10; g_p.inv[W_BAT]=1;
  for(int side=0; side<2; side++){
    g_menu.cam_left=(uint8_t)side; game_step();
    int wx = side ? (SCR_W - PX(62)) : PX(6);
    uint8_t before=g_p.weapon;
    game_touch_down((int16_t)(wx+PX(20)),(int16_t)PX(34)); game_touch_up();
    snprintf(b,sizeof b,"%s: box at x=%d; weapon %d -> %d",side?"mirror":"normal",wx,before,g_p.weapon);
    ck(side?"mirrored: a tap on the moved weapon box swaps the weapon":"normal: a tap on the weapon box swaps the weapon", g_p.weapon!=before, b);
    // a tap where the box WAS should not swap
    before=g_p.weapon; int ox = side ? PX(6) : (SCR_W - PX(62));
    game_touch_down((int16_t)(ox+PX(20)),(int16_t)PX(34)); game_touch_up();
    ck(side?"mirrored: the old spot is not the box":"normal: the mirrored spot is not the box", g_p.weapon==before, "");
  }
  printf("%d failure(s)\n",fails); return fails?1:0; }
