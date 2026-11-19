#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"
static int dom_dispatch(int op, int arg);
#include "wr_shim.h"

// ---------------------------------------------------------------------------
// The national guard. The tier above air support: at five stars, once you
// have killed enough officers that the police have stopped winning, the
// helicopter stands down and this comes instead - a tank, and soldiers in
// olive with machine guns and more armour than SWAT.
//
// The tank is a police car with the CAR_TANK model, so the kernel's crashes
// know it (it crushes what it meets) and the car tenant can drive it if you
// take it - with military armour, a machine gun and shells as the prize.
// Its frame here: it drives at you and does not pull over, the turret
// tracks you, and it fires a shell every two seconds from eight tiles. The
// turret and the barrel are drawn here over the model.
// ---------------------------------------------------------------------------

static void make_soldier(Ent *e) { e->aux = PA_GUARD; e->hp = 200; e->wep = W_MG; }
static void make_tank(Ent *e) {
  // One tank at a time; the other cars are jeeps carrying troops.
  for (int i = 0; i < MAX_ENT; i++)
    if (&g_ent[i] != e && g_ent[i].type == ENT_COPCAR && g_ent[i].sub == CAR_TANK && g_ent[i].state != ST_DEAD) {
      e->sub = CAR_JEEP; e->hp = 160; return;
    }
  e->sub = CAR_TANK; e->hp = 250;
}

// The tank's frame. `anim` (unused on a car) is the turret's angle, in units of 256.
static void tank_tick(Ent *e) {
  uint16_t at = angle_to(e->x, e->y, g_p.x, g_p.y);
  int32_t d = wr_dist(g_p.x - e->x, g_p.y - e->y);
  drive_along(e, true, g_p.x, g_p.y, 92, d > 6 * TILE ? 14 : 8, true);
  int16_t diff = (int16_t)(uint16_t)(at - (uint16_t)(e->anim << 8));
  int32_t step = diff > 1200 ? 1200 : diff < -1200 ? -1200 : diff;    // slow: you can get behind it
  e->anim = (uint8_t)((((int32_t)e->anim << 8) + step) >> 8);
  if (d < 8 * TILE && wr_abs(diff) < 2000 && (g_frame % 50) == (uint32_t)((e - g_ent) % 50)) {
    uint16_t ta = (uint16_t)(e->anim << 8);
    ent_fire(e->x + fmul(90, fcos(ta)), e->y + fmul(90, fsin(ta)), ta, W_ROCKET, B_FROM_COP | 2);
  }
}

static void tank_draw(const Ent *e) {
  CarDraw c = { e->x, e->y, e->ang, CAR_TANK, 0, 0, 0, (uint8_t)(e->state == ST_DEAD), e->hp, 0, e->fire, e->z };
  model_car(&c, 0);
  if (e->state == ST_DEAD) return;
  uint16_t ta = e->occupied ? g_p.ang : (uint16_t)(e->anim << 8);   // yours: the turret is where you point
  gfx_part(e->x, e->y, ta, 0, 0, 34, 30, 26, 18, 0, ARGB(1,2,0), ARGB(1,1,0), ARGB(0,1,0));   // turret
  gfx_part(e->x, e->y, ta, 60, 0, 44, 46, 4, 4, 0, ARGB(1,1,1), ARGB(1,1,1), ARGB(0,0,0));    // barrel
  gfx_part(e->x, e->y, ta, 104, 0, 44, 6, 6, 6, 0, ARGB(0,0,0), ARGB(0,0,0), ARGB(0,0,0));    // muzzle
  gfx_part(e->x, e->y, e->ang, 0, -34, 6, 56, 6, 12, 0, ARGB(0,0,0), ARGB(1,1,1), ARGB(0,0,0)); // tracks
  gfx_part(e->x, e->y, e->ang, 0, 34, 6, 56, 6, 12, 0, ARGB(0,0,0), ARGB(1,1,1), ARGB(0,0,0));
}

// Arriving: the units already on the street become the guard's - the
// officers soldiers, one patrol car the tank, the rest jeeps.
static void take_over(void) {
  for (int i = 0; i < MAX_ENT; i++) {
    Ent *e = &g_ent[i];
    if (e->state == ST_DEAD || e->occupied) continue;
    if (e->type == ENT_COP) make_soldier(e);
    else if (e->type == ENT_COPCAR) make_tank(e);
  }
}

// Your tank's loadout, on the aircraft's weapon cycle (a tank on the street
// keeps this tenant resident, so this is always here when you drive one): the coaxial machine
// gun (held, it streams) or a shell from the main gun.
static void tank_attack(void) {
  Ent *t = &g_ent[g_p.in_car - 1];
  if (t->type != ENT_COPCAR || t->sub != CAR_TANK || g_p.attack_cd) return;
  if (g_p.heli_wep == 0) {
    g_p.attack_cd = 4; audio_sfx(SFX_PISTOL);
    fire_bullet_kind((uint16_t)(g_p.ang + (uint16_t)(wr_rand_n(1400) - 700)), 16, B_SLUG);
  } else {
    g_p.attack_cd = 40; audio_sfx(SFX_ROCKET);
    fire_bullet_kind(g_p.ang, 90, B_ROCKET);
  }
}

static int dom_dispatch(int op, int arg) {
  switch (op) {
    case DOP_ENTER:    take_over(); return 1;
    case DOP_ATTACK:   tank_attack(); return 1;
    case DOP_WANT:     return 0;                              // no helicopter: the air has stood down
    case DOP_UNIT:     if (g_ent[arg].type == ENT_COP) make_soldier(&g_ent[arg]); else make_tank(&g_ent[arg]); return 1;
    case DOP_ENT:      if (g_ent[arg].type == ENT_COPCAR) tank_tick(&g_ent[arg]); return 1;
    case DOP_DRAW_ENT: tank_draw(&g_ent[arg]); return 1;
    default: return 0;
  }
}

DOMAIN_ENTRY(dom_guard_entry)
