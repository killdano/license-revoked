#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"
static int dom_dispatch(int op, int arg);
#include "wr_shim.h"

#if WR_MONOLITH
void kill_entity(Ent *e);
void fire_bullet(uint16_t ang, uint8_t dmg);
void fire_bullet_kind(uint16_t ang, uint8_t dmg, uint8_t kind);
void damage_entity(Ent *e, uint8_t dmg);
#endif

// ---------------------------------------------------------------------------
// Hands: fists and the bat. What a swing does - fittings within reach get
// hit (that is how you rob a place with a bat), and anyone roughly in front
// of you takes the weapon's damage.
// ---------------------------------------------------------------------------
static void swing(void);
#define DRIVING 0
#define NO_DOOR 1
#define NO_VEHICLE_ACTION() swing()
#include "movement.inc"

static const uint8_t RANGE[2] = { 90, 125 };
static const uint8_t DMG[2]   = { 34, 120 };   // the bat kills in one

static void swing(void) {
  audio_sfx(g_p.weapon == W_BAT ? SFX_BAT : SFX_PUNCH);
  for (int i = 0; i < MAX_PROP; i++) {
    Prop *pr = &g_prop[i];
    if (!pr->alive || pr->loose) continue;
    if (wr_dist(pr->x - g_p.x, pr->y - g_p.y) > 120) continue;
    prop_hit(i, fcos(g_p.ang) >> 6, fsin(g_p.ang) >> 6, 30);
  }
  bool bat = g_p.weapon == W_BAT;
  int32_t range = RANGE[bat], c = fcos(g_p.ang), s = fsin(g_p.ang);
  int32_t ax = g_p.x + ((c * 60) >> 10), ay = g_p.y + ((s * 60) >> 10);   // where the swing lands
  bool connected = false;
  for (int i = 0; i < MAX_ENT; i++) {
    Ent *e = &g_ent[i];
    if (e->state == ST_DEAD) continue;
    if (e->type == ENT_PED || e->type == ENT_COP) {
      int32_t dx = e->x - g_p.x, dy = e->y - g_p.y;
      if (wr_dist(dx, dy) > range || ((dx * c + dy * s) >> 10) < 0) continue;
      damage_entity(e, DMG[bat]);
      connected = true;
    } else if (bat && (e->type == ENT_CAR || e->type == ENT_COPCAR || e->type == ENT_AMB) && !e->fire) {
      // The bat dents cars: bodywork off any car the swing lands on.
      if (!in_car_box(e, ax, ay, 30)) continue;
      if (e->hp > 18) e->hp -= 18; else { g_blame = 1; vehicle_destroy(i, false); g_blame = 0; }
      gib_burst_debris(e->x, e->y, 40, c >> 6, s >> 6, 3, ARGB(2,2,2));
      audio_sfx(SFX_CRASH); connected = true;
    }
  }
  if (connected) wr_vibe(VIBE_LIGHT);
}


static int dom_dispatch(int op, int arg) {
  (void)arg;
  if (op == DOP_MOVE)    { move_player(); return 1; }
  if (op == DOP_TURN)    { update_turn_and_speed(); return 1; }
  if (op == DOP_ATTACK) { swing(); return 1; }
  return 0;
}

DOMAIN_ENTRY(dom_h_melee_entry)
