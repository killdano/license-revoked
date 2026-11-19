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

// Hands: the rocket launcher. Flat and fast; one hit wrecks a car.
static uint16_t aim_assist(uint16_t ang) {
  int32_t best = 5200;
  uint16_t out = ang;
  for (int i = 0; i < MAX_ENT; i++) {
    Ent *e = &g_ent[i];
    if (e->type == ENT_NONE || e->state == ST_DEAD) continue;
    if (g_p.in_car && (int)g_p.in_car - 1 == i) continue;
    bool person = e->type == ENT_PED || e->type == ENT_COP;
    bool vehicle = e->type == ENT_CAR || e->type == ENT_COPCAR;
    if (!person && !vehicle) continue;
    int32_t d = wr_dist(e->x - g_p.x, e->y - g_p.y);
    if (d < 60 || d > 7 * TILE) continue;
    uint16_t to = angle_to(g_p.x, g_p.y, e->x, e->y);
    int32_t off = (int16_t)(uint16_t)(to - ang);   // signed shortest arc
    if (wr_abs(off) < best) { best = wr_abs(off); out = to; }
  }
  return out;
}

static void fire(void) {
  if (g_p.ammo == 0) { g_p.inv[g_p.weapon] = 0; g_p.weapon = W_FIST; return; }
  g_p.ammo--;
  audio_sfx(SFX_ROCKET);
  fire_bullet_kind(aim_assist(g_p.ang), 90, B_ROCKET);
  if (g_p.ammo == 0) { g_p.inv[g_p.weapon] = 0; g_p.weapon = W_FIST; }   // empty: gone from your hands and the cycle
}

#define DRIVING 0
#define NO_DOOR 1
#define NO_VEHICLE_ACTION() fire()
#include "movement.inc"

static int dom_dispatch(int op, int arg) {
  (void)arg;
  if (op == DOP_MOVE)    { move_player(); return 1; }
  if (op == DOP_TURN)    { update_turn_and_speed(); return 1; }
  if (op == DOP_ATTACK) { fire(); return 1; }
  return 0;
}

DOMAIN_ENTRY(dom_h_rocket_entry)
