#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"
static int dom_dispatch(int op, int arg);
#include "wr_shim.h"

#if WR_MONOLITH
extern int32_t s_seen_x, s_seen_y;
void fire_bullet_kind(uint16_t ang, uint8_t dmg, uint8_t kind);
void steer(Ent *e, uint16_t target, int32_t rate);
void cop_shoot(int32_t x, int32_t y, uint16_t aim, int32_t spd, uint8_t life,
               uint8_t dmg, uint8_t flags, uint8_t sfx);
#endif

// ---------------------------------------------------------------------------
// Air support: the helicopter, the door gunner, SWAT on the ropes - and the
// controls for the one you steal, since it is flown, drawn and armed from
// here. The first tenant of the THREAT arena. Resident from four stars and
// while any aircraft exists; never resident otherwise, so the national guard
// and whatever comes after can have the same bench later.
// ---------------------------------------------------------------------------

// on_heli()/airborne() are the kernel's, tiny; the domain has its own copies.
static bool on_heli(void) { return g_p.in_car && g_ent[g_p.in_car - 1].type == ENT_HELI; }

#include "aircraft.inc"
void model_heli(const Ent *e) {
  int32_t z = e->z;
  gfx_parts(e->x, e->y, e->ang, z, 0, HELIP, (int)(sizeof HELIP / sizeof HELIP[0]), 2, NULL);
  uint8_t bl = (g_frame & 8) ? ARGB(3,0,0) : ARGB(0,0,3);
  gfx_part(e->x, e->y, e->ang, -30, 0, z + 58, 6, 6, 6, 0, bl, bl, bl);
  uint16_t rot = (uint16_t)(e->phase << 8);
  gfx_part(e->x, e->y, e->ang, 0, 0, z + 64, 110, 5, 3, rot, K0, K0, K0);
  gfx_part(e->x, e->y, e->ang, 0, 0, z + 64, 110, 5, 3, (uint16_t)(rot + 16384), K0, K0, K0);
  gfx_part(e->x, e->y, e->ang, -128, 0, z + 50, 3, 24, 3, (uint16_t)(rot * 2), K0, K0, K0);
}

static int want_heli(void) {
  if (g_p.tier == 2) return 0;                    // the guard has this now
  if (g_p.evade >= EVADE_CLEAR) return 0;
  return (g_p.wanted >= HELI_WANTED && g_p.hostile && g_p.copkills >= 2 && g_p.hot > 750) ? 1 : 0;
}

static bool heli_landing_spot(int32_t *ox, int32_t *oy) {
  static const int8_t K[6] = { 0, 1, -1, 2, -2, 6 };
  for (int i = 0; i < 6; i++) {
    uint16_t a = (uint16_t)(g_p.ang + K[i] * 5461);
    int32_t x = g_p.x + fmul(3 * TILE, fcos(a)), y = g_p.y + fmul(3 * TILE, fsin(a));
    uint8_t c = cell_at_point(x, y);
    if (c != CELL_ROAD && c != CELL_PAVE && c != CELL_PARK) continue;
    bool clear = true;
    for (int k = 0; k < 4 && clear; k++) {
      uint16_t b = (uint16_t)(k * 16384);
      if (city_blocked(x + fmul(120, fcos(b)), y + fmul(120, fsin(b)), true)) clear = false;
    }
    if (!clear) continue;
    *ox = x; *oy = y;
    return true;
  }
  return false;
}

static void heli_drop_swat(Ent *e, int n) {
  for (int k = 0; k < n; k++) {
    int s = free_slot_evict();
    if (s < 0) return;
    Ent *c = &g_ent[s];
    memset(c, 0, sizeof(Ent));
    c->type = ENT_COP;
    c->sub = PED_MAN;
    c->hp = 130;                       // body armour
    c->wep = W_MG;
    c->state = ST_CHASE;
    c->spd = 6;
    uint16_t side = (uint16_t)(e->ang + (k & 1 ? 16384 : 49152));
    c->x = e->x + fmul(70, fcos(side)); c->y = e->y + fmul(70, fsin(side));
    c->ang = side;
    c->timer = (uint8_t)(k * 7);
  }
  game_caption(CAP_SWAT, 60);
  audio_sfx(SFX_ARREST);
}

// The police helicopter.
//
// It holds station AHEAD of the player rather than on top of him, because
// the camera looks over his shoulder: overhead is the one place the player
// can never see. The door gunner works from there. Every so often, with
// nobody on the ground, it comes in to land on the nearest clear stretch in
// front of him and the SWAT team gets out of the doors - and for the few
// seconds it sits there with the rotor turning, it can be taken.
//
// States: CHASE = flying and holding station; ROPE = coming in to land;
// PARK = on the ground; FLEE = lifting off; DRIVE = the player has it.
static void update_heli(Ent *e) {
  // The guard has this now: the police helicopter leaves. (Not one you
  // are flying, and not the airliner.)
  if (g_p.tier == 2 && !e->occupied && e->sub == 0) { e->type = ENT_NONE; return; }
  if (e->sub == 1 && !e->occupied) { e->type = ENT_NONE; return; }   // an airliner nobody is flying flies off
  if (e->occupied) return;             // flown by the player, see wr_game.c
  const int32_t px = s_seen_x, py = s_seen_y;
  int32_t d = wr_dist(e->x - px, e->y - py);
  e->phase += 40;                      // rotor

  if (e->state == ST_PARK) {
    // On the ground with the doors open. Sits for a while, then leaves -
    // unless it has been stolen, in which case it stays exactly where it
    // was left and the police send another.
    e->spd = 0;
    if (e->wep == 0) return;           // abandoned by the player: scenery
    if (++e->timer > 140) { e->state = ST_FLEE; e->timer = 0; }
    return;
  }
  if (e->state == ST_FLEE) {
    e->z = (int16_t)(e->z + 8);
    if (e->z >= HELI_ALT) { e->z = HELI_ALT; e->state = ST_CHASE; }
    return;
  }
  if (e->state == ST_ROPE) {
    // Coming in: fly to the spot, then settle.
    int32_t lx = (int32_t)e->vx * 4, ly = (int32_t)e->vy * 4;
    int32_t ld = wr_dist(e->x - lx, e->y - ly);
    steer(e, angle_to(e->x, e->y, lx, ly), 2200);
    if (ld > 40) {
      int32_t v = ld > 300 ? e->spd : 6;
      e->x += fmul(v, fcos(e->ang)); e->y += fmul(v, fsin(e->ang));
    } else {
      e->z = (int16_t)(e->z - 7);
      if (e->z <= 0) {
        e->z = 0; e->state = ST_PARK; e->timer = 0;
        heli_drop_swat(e, 2);
      }
    }
    return;
  }

  // Flying. Station: two tiles ahead of where he is looking, which with
  // the camera pulled out puts it a third of the way down the frame.
  int32_t sx = px + fmul(TILE * 2, fcos(g_p.ang));
  int32_t sy = py + fmul(TILE * 2, fsin(g_p.ang));
  int32_t sd = wr_dist(e->x - sx, e->y - sy);
  steer(e, angle_to(e->x, e->y, sd > 200 ? sx : px, sd > 200 ? sy : py), 2200);
  if (sd > 120) {
    int32_t v = sd > 2 * TILE ? e->spd : 8;
    e->x += fmul(v, fcos(e->ang)); e->y += fmul(v, fsin(e->ang));
  }
  if (e->z < HELI_ALT) e->z = (int16_t)(e->z + 8);

  // It has to be losable: it is slower than a fast car, and if you keep
  // it well behind you for long enough it turns for home. `anim` counts
  // the frames it has spent out of touch - but only once it has actually
  // found you (`door`); it spawns nine tiles out and used to give up
  // before it arrived.
  if (d < 5 * TILE) e->door = 1;
  if (d > 7 * TILE && e->door) {
    if (e->anim < 250) e->anim = (uint8_t)(e->anim + 2);
    if (e->anim > 90) { e->type = ENT_NONE; return; }
  }

  int swat = 0;
  for (int i = 0; i < MAX_ENT; i++)
    if (g_ent[i].type == ENT_COP && g_ent[i].wep == W_MG &&
        g_ent[i].state != ST_DEAD) swat++;

  // Door gunner: fired from overhead with a spread, through the same
  // police fire budget as everyone else.
  if (g_p.hostile && d < 9 * TILE && (e->timer % 20) == 0)
    cop_shoot(e->x, e->y, (uint16_t)(angle_to(e->x, e->y, px, py) + (uint16_t)(wr_rand_n(9000) - 4500)),
              52, (uint8_t)wr_clamp(d / 52 + 3, 4, 30), 8, 3, SFX_PISTOL);

  // Land to deploy, once it is actually ON station (not merely near) and
  // the ground is empty - you get to see it hunt before it comes down.
  if (swat < 2 && sd < 400 && e->door && (e->timer % 90) == 0) {
    int32_t lx, ly;
    if (heli_landing_spot(&lx, &ly)) {
      e->vx = (int16_t)(lx >> 2); e->vy = (int16_t)(ly >> 2);
      e->state = ST_ROPE;
      e->wep = HELI_ROCKETS;             // the armament it lands with
    }
  }
  e->timer++;
}

// Car crashes, done as physics rather than dice. Momentum is exchanged
// along the line between the two cars: a head-on stops both, a glancing
// blow deflects, a rear-ender shunts the car in front forward - all of
// which falls out of one impulse rather than being scripted. Damage comes
// from the CLOSING speed along that line (so two cars travelling together
// touching bumpers hurts nobody), fires once per contact with a short
// cooldown (so one crash is one damage event, not fifteen frames of them,
// which is what was writing cars off after a single prang), and every real
// hit throws a burst of painted-metal and glass debris off the point of
// impact. Wrecks stay in the sweep as immovable scenery: you can pile into
// one, and it neither moves nor cares.
// Bodies get dragged. Until now a ragdoll that had come to rest was inert
// scenery you could drive straight through, which is the clearest possible
// tell that the physics stops caring once something is dead. A wheel over a
// body now kicks it along the car's heading with a bit of lift, so it
// tumbles down the street ahead of you and leaves a fresh smear where it
// lands. Only moving vehicles do it, and only to bodies already settled on
// the ground - a fresh kill has its own arc and does not want interfering
// with halfway through it.


// A motorcycle is not a car. Hit one and the rider comes off over the
// bonnet, the bike is flung along your heading and slides, and you barely
// slow down. Hit the sliding bike again and it takes damage like anything
// else - the third or fourth knock sets it alight and it goes up.

// The aircraft's steering and throttle: the same movement source, with the
// helicopter flag on. (move_player() from it is not used here - move_heli()
// is - but it is small and it keeps the include whole.)


// The helicopter's marker: red hunting, green parked and yours to take.
static void draw_hud_t(GContext *ctx) {
  for (int i = 0; i < MAX_ENT; i++) {
    Ent *e = &g_ent[i];
    if (e->type == ENT_HELI && !e->occupied) hud_marker(ctx, e->x, e->y, e->z + 60, e->state == ST_PARK ? GColorGreen : GColorRed);
  }
}

static int dom_dispatch(int op, int arg) {
  switch (op) {
    case DOP_DRAW_HUD: draw_hud_t((GContext *)(intptr_t)arg); return 1;
    case DOP_WANT:     return want_heli();
    case DOP_ENT:      update_heli(&g_ent[arg]); return 1;
    case DOP_DRAW_ENT: model_heli(&g_ent[arg]); return 1;
    default: return 0;
  }
}

DOMAIN_ENTRY(dom_air_entry)
