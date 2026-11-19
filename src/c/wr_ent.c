#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"
#include "wr_tune.h"

// Kept out of line on purpose. These are called once each, so -Os folds them
// into ent_update, which grew to 6,100 bytes - a tenth of the whole image in
// one symbol, and nothing inside it can be measured, moved or replaced. A
// domain cannot be carved out of a function that does not exist. The cost is
// a prologue each; the benefit is a codebase with parts. See DESIGN.md.
#define WR_PART __attribute__((noinline))

// -DWR_TRACE prints one vehicle's driver state per frame (wr_trace_ent)
// and every spawn.
#ifdef WR_TRACE
#include <stdio.h>
int wr_trace_ent = -1;
#define TRACE_SPAWN(...) printf(__VA_ARGS__)
#define TRACE(...) do { if (e == &g_ent[wr_trace_ent]) printf(__VA_ARGS__); } while (0)
#else
#define TRACE_SPAWN(...) do {} while (0)
#define TRACE(...) do {} while (0)
#endif


Ent    g_ent[MAX_ENT];
Pickup g_pickup[MAX_PICKUP];
Bullet g_bullet[MAX_BULLET];
Blood  g_blood[MAX_BLOOD];
Boom   g_boom[MAX_BOOM];
Gib    g_gib[MAX_GIB];

// How many officers have opened fire this frame. Five units converging and
// all firing at once is not difficulty, it is a firing squad: you see the
// helicopter, and five seconds later you are dead with nothing you could
// have done. Two at a time still pins you down but leaves the fight
// readable and gives you somewhere to run.
#define MAX_SHOOTERS 2
static uint8_t s_shooters;
uint8_t g_heli_up;
// ...and a global cadence on top: one police round every COP_FIRE_GAP
// frames, whoever fires it. A dozen officers can be on screen; the player
// is still only being shot at by one gun at a time.
#define COP_FIRE_GAP 12
static uint8_t s_cop_fire_cd;
static bool cop_may_fire(void) {
  if (s_shooters >= MAX_SHOOTERS || s_cop_fire_cd) return false;
  s_shooters++;
  s_cop_fire_cd = COP_FIRE_GAP;
  return true;
}
// One police round, if the budget allows it. `flags` is Bullet.from_cop.
void cop_shoot(int32_t x, int32_t y, uint16_t aim, int32_t spd, uint8_t life,
                      uint8_t dmg, uint8_t flags, uint8_t sfx) {
  if (!cop_may_fire()) return;
  aim = (uint16_t)(aim + (uint16_t)(wr_rand_n(5200) - 2600));    // 15 degrees of nerves
  for (int i = 0; i < MAX_BULLET; i++) {
    Bullet *b = &g_bullet[i];
    if (b->life) continue;
    b->x = x; b->y = y;
    b->vx = (int16_t)fmul(spd, fcos(aim));
    b->vy = (int16_t)fmul(spd, fsin(aim));
    b->life = life; b->dmg = dmg; b->from_cop = flags; b->kind = B_SLUG;
    audio_sfx(sfx);
    return;
  }
}

// One round from a weapon, for anyone who is not the player. The militia
// fire these through their domain; police could too.
void ent_fire(int32_t x, int32_t y, uint16_t aim, uint8_t wep, uint8_t flags) {
  switch (wep) {
    case W_PISTOL:  cop_shoot(x, y, aim, 52, 30, 12, flags, SFX_PISTOL); break;
    case W_SHOTGUN: for (int k = -1; k <= 1; k++)
                      cop_shoot(x, y, (uint16_t)(aim + k * 1800), 46, 12, 9, flags, SFX_SHOTGUN);
                    break;
    case W_MG:      cop_shoot(x, y, aim, 56, 30, 8, flags, SFX_PISTOL); break;
    case W_FLAME: case W_ROCKET: {
      // Fire and rockets are the player's bullet kinds; a militia flame or
      // rocket is the same object with the faction bit.
      for (int i = 0; i < MAX_BULLET; i++) {
        Bullet *b = &g_bullet[i];
        if (b->life) continue;
        int32_t spd = wep == W_FLAME ? 22 : 55;
        b->x = x; b->y = y;
        b->vx = (int16_t)fmul(spd, fcos(aim)); b->vy = (int16_t)fmul(spd, fsin(aim));
        b->life = wep == W_FLAME ? 9 : 40; b->dmg = wep == W_FLAME ? 6 : 90;
        b->from_cop = flags; b->kind = wep == W_FLAME ? B_FIRE : B_ROCKET;
        audio_sfx(wep == W_FLAME ? SFX_FLAME : SFX_ROCKET);
        return;
      }
      break;
    }
    default: break;
  }
}

int ent_nearest_of(uint8_t type, int32_t x, int32_t y, int32_t range, uint8_t aux_or_0) {
  int best = -1; int32_t bd = range;
  for (int i = 0; i < MAX_ENT; i++) {
    Ent *e = &g_ent[i];
    if (e->type != type || e->state == ST_DEAD) continue;
    if (aux_or_0 && (uint8_t)e->aux != aux_or_0) continue;
    int32_t d = wr_dist(e->x - x, e->y - y);
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}

#define SPAWN_MIN  (6 * TILE)
#define SPAWN_MAX  (11 * TILE)
#define DESPAWN    (15 * TILE)

// How many entities of each sort we want alive near the player. Police counts
// scale with the wanted level - that is the whole difficulty curve.
// Once the player has been out of sight long enough, dispatch stops sending
// anyone new. Units already on the road keep chasing until they fall behind
// and despawn.
static int want_cops(void) {
  // On the ghetto the police are always on the street - two officers'
  // worth, wanted or not. That is the island.
  int isl = island_at(g_p.x, g_p.y);
  if (isl == ISL_SECRET) return 0;                 // no police there, ever
  int base = (isl == ISL_GHETTO) ? 2 : 0;
  if (g_p.lull && g_p.tier < 2) return 0;          // regrouping: nobody comes for a while (the military does not regroup)
  if (g_p.evade >= EVADE_CLEAR) return base;
  return g_p.wanted >= 1 ? g_p.wanted + base : base;
}
static int want_copcars(void) {
  if (g_p.lull && g_p.tier < 2) return 0;
  if (island_at(g_p.x, g_p.y) == ISL_SECRET) return 0;
  if (g_p.evade >= EVADE_CLEAR) return 0;
  return g_p.wanted >= 2 ? (g_p.wanted - 1) : 0;
}
// The marine unit only turns out if you have taken to the water.
#if WR_BOATS
static bool player_afloat(void) {
  return cell_at_point(g_p.x, g_p.y) == CELL_WATER;
}
static int want_copboats(void) {
  if (g_p.evade >= EVADE_CLEAR) return 0;
  if (!player_afloat()) return 0;
  return g_p.wanted >= 2 ? (g_p.wanted >= 4 ? 2 : 1) : 0;
}
#else
static inline int want_copboats(void) { return 0; }
#endif
// Air support is the police stepping it up, not their opening move: four
// stars, guns already out, two officers down, and half a minute of it.
static int want_heli(void) { return domain2_call(DOP_WANT, 0); }

#define GRAVITY 3

// A spray of droplets that arc, bounce and skid before settling. Each one
// stamps a decal where it comes to rest, so the pool that remains is the
// shape the spray actually made rather than a square someone drew.
static void gib_spawn(int32_t x, int32_t y, int32_t z,
                      int32_t vx, int32_t vy, int n,
                      uint8_t col, uint8_t stains) {
  for (int k = 0; k < n; k++) {
    int slot = -1; uint8_t worst = 255;
    for (int i = 0; i < MAX_GIB; i++) {
      if (!g_gib[i].life) { slot = i; break; }
      // A full pool recycles the oldest piece - but never money on its
      // way down: a bill that gets replaced by a splinter is a bill lost.
      if (g_gib[i].stains >= GIB_CASH) continue;
      if (g_gib[i].life < worst) { worst = g_gib[i].life; slot = i; }
    }
    if (slot < 0) return;
    Gib *g = &g_gib[slot];
    g->x = x; g->y = y;
    g->z = (int16_t)(z + (int32_t)wr_rand_n(20));
    // Inherit the impact direction, then scatter hard around it.
    g->vx = (int16_t)(vx / 3 + (int32_t)wr_rand_n(23) - 11);
    g->vy = (int16_t)(vy / 3 + (int32_t)wr_rand_n(23) - 11);
    g->vz = (int16_t)(6 + wr_rand_n(13));
    g->life = (uint8_t)(30 + wr_rand_n(26));
    g->size = (uint8_t)(4 + wr_rand_n(6));
    g->col = col;
    g->stains = stains;
  }
}

void gib_burst(int32_t x, int32_t y, int32_t z,
               int32_t vx, int32_t vy, int n) {
  gib_spawn(x, y, z, vx, vy, n, 0xD0 /* ARGB(2,0,0) */, 1);
}

// Splinters, glass and torn metal. Same arc, bounce and skid as blood, but it
// takes the colour of whatever broke and leaves no pool behind.
void gib_burst_debris(int32_t x, int32_t y, int32_t z,
                      int32_t vx, int32_t vy, int n, uint8_t col) {
  gib_spawn(x, y, z, vx, vy, n, col, 0);
}

WR_PART static void gib_update(void) {
  for (int i = 0; i < MAX_GIB; i++) {
    Gib *g = &g_gib[i];
    if (!g->life) continue;

    g->x += g->vx;
    g->y += g->vy;
    g->z = (int16_t)(g->z + g->vz);
    g->vz = (int16_t)(g->vz - GRAVITY);

    if (g->z <= 0) {
      g->z = 0;
      // Only bounce if it is still coming down with some energy, otherwise it
      // jitters on the spot forever instead of settling.
      if (g->vz < -6) {
        g->vz = (int16_t)(-g->vz / 2);
        g->vx = (int16_t)((g->vx * 3) / 4);
        g->vy = (int16_t)((g->vy * 3) / 4);
      } else {
        g->vz = 0; g->vx = 0; g->vy = 0;
      }
    }
    // Walls stop droplets dead.
    if (city_blocked(g->x, g->y, false)) { g->vx = 0; g->vy = 0; }

    if (--g->life == 0) {
      if (g->stains == GIB_BLOOD)      blood_add(g->x, g->y, g->size + 8);
      else if (g->stains == GIB_CASH)  pickup_cash(g->x, g->y, (uint8_t)(g->size * 5));
      else if (g->stains == GIB_BILL)  inside_money(g->x, g->y);
    }
  }
}

void blood_add(int32_t x, int32_t y, uint8_t size) {
  int oldest = 0; uint8_t best = 255;
  for (int i = 0; i < MAX_BLOOD; i++) {
    if (!g_blood[i].life) { oldest = i; break; }
    if (g_blood[i].life < best) { best = g_blood[i].life; oldest = i; }
  }
  g_blood[oldest].x = x + (int32_t)wr_rand_n(24) - 12;
  g_blood[oldest].y = y + (int32_t)wr_rand_n(24) - 12;
  g_blood[oldest].life = 90;
  g_blood[oldest].size = size;
}

// An explosion is created already having done its damage - the Boom entry
// that survives is only the visual.
void boom_add(int32_t x, int32_t y) {
  int slot = 0; uint8_t worst = 255;
  for (int i = 0; i < MAX_BOOM; i++) {
    if (!g_boom[i].life) { slot = i; break; }
    if (g_boom[i].life < worst) { worst = g_boom[i].life; slot = i; }
  }
  g_boom[slot].x = x;
  g_boom[slot].y = y;
  g_boom[slot].life = BOOM_LIFE;
  audio_sfx(SFX_EXPLODE);

  // Scenery in the blast. A grenade in a gun shop that leaves the glass
  // cabinets standing is not an explosion, it is a noise.
  for (int i = 0; i < MAX_PROP; i++) {
    Prop *pr = &g_prop[i];
    if (!pr->alive || pr->loose) continue;
    int32_t pd = wr_dist(pr->x - x, pr->y - y);
    if (pd > 260) continue;
    int32_t kx2 = ((pr->x - x) * 40) / (pd + 1);
    int32_t ky2 = ((pr->y - y) * 40) / (pd + 1);
    prop_hit(i, kx2, ky2, (pd < 120) ? 255 : 120);
  }

  for (int i = 0; i < MAX_ENT; i++) {
    Ent *e = &g_ent[i];
    if (e->type == ENT_NONE || e->state == ST_DEAD) continue;
    int32_t d = wr_dist(e->x - x, e->y - y);
    if (d > 300) continue;
    if (e->type == ENT_HELI && e->z > 100) continue;   // the blast is on the ground
    if (e->type == ENT_CAR || e->type == ENT_COPCAR || e->type == ENT_AMB || e->type == ENT_HELI || ent_is_boat(e->type)) {
      // A car touching the one that went up goes with it (on its own
      // fuse, so there is no recursion); one a car-length away is badly
      // hurt; further out, dented.
      int32_t dmg = (d < 140) ? 255 : (d < 210) ? 90 : 40;
      if (e->hp > dmg) e->hp = (uint8_t)(e->hp - dmg); else vehicle_destroy(i, false);
    } else {
      blood_add(e->x, e->y, 26);
      e->state = ST_DEAD;
      e->hp = 0;
    }
  }

  int32_t pd = wr_dist(g_p.x - x, g_p.y - y);
  if (pd < 260 && !g_p.in_car) {        // in a car, the car took it above
    int32_t dmg = 55 - (pd / 8);
    player_absorb(dmg);
    g_p.hurt_flash = 8;
  }
}

// Set a car alight rather than blowing it up on the spot. A car that has
// just been written off usually catches fire first and goes up a few
// seconds later - which gives you time to get clear, or to shove someone
// else into it. The delay is randomised so it never feels scripted.
void vehicle_ignite(int idx) {
  Ent *e = &g_ent[idx];
  if (e->fire) return;
  e->fire = (uint8_t)(70 + wr_rand_n(150));
  audio_sfx(SFX_SMASH);
}

// Whose fault a wrecked car is. Set by the player's own rounds and blasts
// around their damage to a car; a car written off while it is set is the
// player's crime, and it counts like two bodies.
uint8_t g_blame;

void vehicle_destroy(int idx, bool explode) {
  // Already alight? Then the fuse owns this car and nothing else may end it
  // early. The state does not matter: a burning car may still be driving,
  // because its panicking driver is fleeing with it. Requiring ST_DEAD here
  // let the very next hit fall through to the detonation, which is why a
  // held flamethrower blew cars up on the spot instead of setting them
  // alight. Hastening the fuse is a caller's job; finishing it is not.
  if (g_ent[idx].fire) return;
  if (g_blame && g_ent[idx].state != ST_DEAD) add_heat(2 * HEAT_KILL);   // a car, and whoever was in it

  // Written off is not the same as detonated. A car that has taken its last
  // hit catches fire and burns for a few seconds, which is the warning you
  // need to get out of it - the old behaviour blew up in the same frame the
  // bodywork ran out, with no tell at all. fire_update() sets it off when
  // the timer runs down; anything touching a burning car still goes
  // immediately.
  if (explode) {
    Ent *v = &g_ent[idx];
    if (!v->fire) {
      vehicle_ignite(idx);
      v->state = ST_DEAD;
      v->spd = 0;
      v->vx = v->vy = v->vz = v->spin = 0;    // the driver's scratch, not a throw
      if (g_p.in_car && (int)g_p.in_car - 1 == idx) {
        g_p.in_car = 0;             // thrown clear, on foot, car alight
        g_p.spd = 0;
        g_p.x += fmul(90, fsin(v->ang)); g_p.y -= fmul(90, fcos(v->ang));   // beside it, not in it
      }
      return;
    }
  }
  Ent *e = &g_ent[idx];
  if (e->state == ST_DEAD) return;
  e->hp = 0;
  e->state = ST_DEAD;
  e->spd = 0;
  e->vx = e->vy = e->vz = e->spin = 0;
  // A helicopter that dies in the air takes its pilot with it.
  if (e->type == ENT_HELI && g_p.in_car && (int)g_p.in_car - 1 == idx && e->z > 30)
    player_absorb(255);
  if (g_p.in_car && (int)g_p.in_car - 1 == idx) {
    g_p.in_car = 0;
    g_p.spd = 0;
    g_p.strafe = 0;
    g_p.x += fmul(90, fsin(e->ang)); g_p.y -= fmul(90, fcos(e->ang));
  }
  if (explode) boom_add(e->x, e->y);
}

// Drop something on the ground where a body fell.
void pickup_drop(int32_t x, int32_t y, uint8_t weapon) {
  for (int i = 0; i < MAX_PICKUP; i++) {
    if (g_pickup[i].alive) continue;
    g_pickup[i].x = x; g_pickup[i].y = y;
    g_pickup[i].weapon = weapon;
    g_pickup[i].alive = 1;
    g_pickup[i].world = 0;      // a dropped weapon is never culled by distance
    return;
  }
}

// Civilians never take the last few slots: at four stars the table is
// full of traffic and walkers, and the SWAT team the helicopter has just
// landed to deploy could not find a slot to exist in.
#define RESERVED_SLOTS 5
// The last slot is the helicopter's and nobody else's.
int free_slot(void) {
  for (int i = 0; i < MAX_ENT - 1; i++) if (g_ent[i].type == ENT_NONE) return i;
  return -1;
}
static int free_civil_slot(void) {
  for (int i = 0; i < MAX_ENT - RESERVED_SLOTS; i++) if (g_ent[i].type == ENT_NONE) return i;
  return -1;
}
// The police always get a slot. When the table is full - and at five stars
// it is full of bodies and traffic - the walker, cyclist, unattended car
// or corpse furthest from the player makes way. The player never watches
// it happen: nothing within four tiles is touched.
int free_slot_evict(void) {
  int i = free_slot();
  if (i >= 0) return i;
  int far = -1; int32_t fd = 4 * TILE;
  for (int k = 0; k < MAX_ENT - 1; k++) {
    Ent *e = &g_ent[k];
    bool civil = e->type == ENT_PED ||
                 (e->type == ENT_CAR && !e->occupied);
    if (!civil && e->state != ST_DEAD) continue;
    if (e->occupied) continue;
    // A body goes first, at any distance (the ones you made are at your
    // feet, and at five stars no ambulance comes for them); then the
    // farthest living civilian beyond four tiles.
    int32_t d = wr_dist(e->x - g_p.x, e->y - g_p.y) + (e->state == ST_DEAD ? 100 * TILE : 0);
    if (d > fd) { fd = d; far = k; }
  }
  if (far >= 0) g_ent[far].type = ENT_NONE;
  return far;
}

// Find a spawn point of the right surface type in a ring around the player.
// Spawn ahead of the player, in the half of the world the camera looks
// at. Traffic that actually drives spreads itself thin over the whole
// despawn radius; with the old stalled traffic the cars piled up around
// you and the streets only LOOKED busy. Biasing the ring of spawn points
// forward, and culling anything well behind you, keeps the cars where
// the camera is.
static bool find_spot(bool road, bool hidden, int32_t *ox, int32_t *oy) {
  for (int t = 0; t < 24; t++) {
    uint16_t a = (uint16_t)(g_p.ang + 32768 + (wr_rand() & 0x7FFF) + 16384);   // the front half
    if (!road) a = (uint16_t)(wr_rand() & 0xFFFF);
    int32_t d = SPAWN_MIN + (int32_t)wr_rand_n(SPAWN_MAX - SPAWN_MIN);
    if (road) d -= TILE;
    int32_t x = g_p.x + fmul(d, fcos(a));
    int32_t y = g_p.y + fmul(d, fsin(a));
    if (x < TILE * 2 || y < TILE * 2 || x > CITY_MAX - TILE * 2 || y > CITY_MAX - TILE * 2)
      continue;
    uint8_t c = cell_at_world(x, y);
    if (road ? (c == CELL_ROAD) : (c == CELL_PAVE || c == CELL_PARK)) {
      if (hidden && gfx_in_view(x, y)) continue;     // civilians never pop in on screen
      *ox = x; *oy = y;
      return true;
    }
  }
  return false;
}

#if WR_BOATS
// Boats are moored at the marinas rather than scattered, so the water is not
// randomly full of unattended craft and there is somewhere worth going.
static bool find_mooring(int32_t *ox, int32_t *oy) {
  int32_t md;
  int m = nearest_marina(g_p.x, g_p.y, &md);
  if (md > 22 * TILE) return false;          // too far from any marina
  for (int t = 0; t < 26; t++) {
    uint16_t a = (uint16_t)(wr_rand() & 0xFFFF);
    int32_t d = TILE + (int32_t)wr_rand_n(5 * TILE);
    int32_t x = MARINA_TX[m] * TILE + fmul(d, fcos(a));
    int32_t y = MARINA_TY[m] * TILE + fmul(d, fsin(a));
    if (cell_at_point(x, y) != CELL_WATER) continue;
    if (wr_dist(x - g_p.x, y - g_p.y) < 3 * TILE) continue;
    *ox = x; *oy = y;
    return true;
  }
  return false;
}
#endif

static void spawn(uint8_t type);
void ent_spawn(uint8_t type) { spawn(type); }
WR_PART static void spawn(uint8_t type) {
  bool police = (type == ENT_COP || type == ENT_COPCAR || type == ENT_COPBOAT ||
                 type == ENT_HELI);
  int i = police ? free_slot_evict() : free_civil_slot();
  if (type == ENT_HELI) i = (g_ent[MAX_ENT - 1].type == ENT_NONE) ? MAX_ENT - 1 : -1;
  if (i < 0) return;
  bool road = (type == ENT_CAR || type == ENT_COPCAR || type == ENT_AMB);
  int32_t x, y;
#if WR_BOATS
  if (type == ENT_BOAT) {
    // Civilian craft are moored at a marina.
    if (!find_mooring(&x, &y)) return;
  } else if (type == ENT_COPBOAT) {
    // The marine unit comes to you across open water, wherever you are - it
    // is no use to anyone tied up at a jetty on the far side of the island.
    bool got = false;
    for (int t = 0; t < 30 && !got; t++) {
      uint16_t a = (uint16_t)(wr_rand() & 0xFFFF);
      int32_t d = SPAWN_MIN + (int32_t)wr_rand_n(SPAWN_MAX - SPAWN_MIN);
      int32_t px = g_p.x + fmul(d, fcos(a));
      int32_t py = g_p.y + fmul(d, fsin(a));
      if (cell_at_point(px, py) != CELL_WATER) continue;
      x = px; y = py; got = true;
    }
    if (!got) return;
  } else
#endif
  if (type == ENT_HELI) {
    // Air support needs no ground: it comes in from well ahead, inside the
    // cone the camera looks down, so you watch it arrive.
    uint16_t a = (uint16_t)(g_p.ang + (uint16_t)(wr_rand_n(8000) - 4000));
    x = g_p.x + fmul(8 * TILE, fcos(a)); y = g_p.y + fmul(8 * TILE, fsin(a));
  } else
  if (!find_spot(road, !police, &x, &y)) { TRACE_SPAWN("spawn %d: no spot\n", type); return; }
  TRACE_SPAWN("spawn %d at slot %d\n", type, i);

  Ent *e = &g_ent[i];
  memset(e, 0, sizeof(Ent));
  e->x = x; e->y = y;
  e->type = type;
  e->hp = 100;
  e->occupied = 0;
  // Streets run diagonally, so a vehicle dropped on an axis-aligned heading
  // spends its whole life driving ACROSS the road and turning away from the
  // kerb. Put it on the centreline, pointing down the street.
  if (road) {
    road_snap(&e->x, &e->y, &e->ang);
  } else {
    e->ang = (uint16_t)(wr_rand() & 0xFFFF);
  }

  switch (type) {
    case ENT_PED:
      e->sub = (uint8_t)wr_rand_n(PED_KIND_COUNT);
      domain_call(DOP_SPAWN, (int)(e - g_ent));     // the island may make them something else
      e->state = ST_WALK;
      e->spd = 3 + (int16_t)wr_rand_n(2);
      break;
    case ENT_AMB:
      e->sub = CAR_VAN; e->hp = 140; e->state = ST_DRIVE;
      break;
    case ENT_COP:
      e->sub = PED_MAN;
      e->state = ST_CHASE;
      domain2_call(DOP_UNIT, (int)(e - g_ent));     // the guard makes soldiers of them
      e->spd = 5;
      // The heavier the response, the better armed they turn up.
      e->wep = (g_p.wanted >= 4) ? W_SHOTGUN : W_PISTOL;
      break;
    case ENT_CAR:
      e->sub = (uint8_t)wr_rand_n(CAR_MODEL_COUNT);
      e->state = ST_DRIVE;
      e->spd = 10 + (int16_t)wr_rand_n(6);
      break;
    case ENT_COPCAR:
      e->sub = CAR_SEDAN;
      e->state = ST_CHASE;
      e->spd = 16;
      domain2_call(DOP_UNIT, (int)(e - g_ent));     // ...and tanks of these
      break;
#if WR_BOATS
    case ENT_BOAT: {
      static const int8_t BSPD[BOAT_MODEL_COUNT] = { 26, 18, 11, 24 };
      e->sub = (uint8_t)wr_rand_n(BOAT_MODEL_COUNT - 1);   // never a police hull
      e->state = ST_DRIVE;
      e->spd = BSPD[e->sub];
      e->hp = 100;
      break;
    }
    case ENT_COPBOAT:
      e->sub = BOAT_POLICE;
      e->state = ST_CHASE;
      e->spd = 25;
      e->hp = 120;
      break;
#endif
    case ENT_HELI:
      e->state = ST_CHASE;
      e->spd = 22;              // slower than a car flat out, deliberately
      e->wep = HELI_ROCKETS;
      e->z = HELI_ALT;
      // Comes in from off to one side rather than straight overhead.
      e->x = g_p.x + ((fcos(e->ang) * 9 * TILE) >> 10);
      e->y = g_p.y + ((fsin(e->ang) * 9 * TILE) >> 10);
      break;
  }
  e->timer = (uint8_t)wr_rand_n(255);
}

// ---------------------------------------------------------------------------
// Movement helpers
// ---------------------------------------------------------------------------

static bool try_move(Ent *e, int32_t spd, bool is_car) {
  int32_t nx = e->x + fmul(spd, fcos(e->ang));
  int32_t ny = e->y + fmul(spd, fsin(e->ang));
  if (city_blocked(nx, ny, is_car)) return false;
  e->x = nx; e->y = ny;
  return true;
}

// Turn toward a target angle by at most `rate`, taking the short way round.
void steer(Ent *e, uint16_t target, int32_t rate) {
  int32_t diff = (int32_t)(int16_t)(target - e->ang);
  if (diff > rate) diff = rate;
  if (diff < -rate) diff = -rate;
  e->ang = (uint16_t)(e->ang + diff);
}

uint16_t angle_to(int32_t fx, int32_t fy, int32_t tx, int32_t ty) {
  // Coarse-then-fine search over the sine table. No atan2, no float, and
  // cheap enough at these entity counts.
  int32_t dx = tx - fx, dy = ty - fy;
  if (!dx && !dy) return 0;
  while (wr_abs(dx) > 30000 || wr_abs(dy) > 30000) { dx >>= 1; dy >>= 1; }

  uint16_t base = 0; int32_t bestdot = -0x7FFFFFFF;
  for (int i = 0; i < 32; i++) {
    uint16_t a = (uint16_t)(i * 2048);
    int32_t dot = (dx * fcos(a) + dy * fsin(a)) >> 10;
    if (dot > bestdot) { bestdot = dot; base = a; }
  }
  uint16_t best = base;
  for (int i = -8; i <= 8; i++) {
    uint16_t a = (uint16_t)(base + i * 256);
    int32_t dot = (dx * fcos(a) + dy * fsin(a)) >> 10;
    if (dot > bestdot) { bestdot = dot; best = a; }
  }
  return best;
}

// ---------------------------------------------------------------------------
// Per-type update
// ---------------------------------------------------------------------------

// A body thrown by a car keeps travelling, tumbles, and skids to a stop.
static void ragdoll_update(Ent *e) {
  if (!e->vx && !e->vy && !e->vz && !e->z) return;

  int32_t nx = e->x + e->vx, ny = e->y + e->vy;
  if (city_blocked(nx, ny, false)) { e->vx = (int16_t)(-e->vx / 2); e->vy = (int16_t)(-e->vy / 2); }
  else { e->x = nx; e->y = ny; }

  e->z = (int16_t)(e->z + e->vz);
  e->vz = (int16_t)(e->vz - GRAVITY);
  e->ang = (uint16_t)(e->ang + e->spin);

  if (e->z <= 0) {
    e->z = 0;
    // Only a real downward impact bounces; anything less lets him settle
    // instead of twitching on the tarmac.
    if (e->vz < -8) {
      e->vz = (int16_t)(-e->vz / 2);            // bounce off the tarmac
      e->vx = (int16_t)((e->vx * 5) / 8);
      e->vy = (int16_t)((e->vy * 5) / 8);
      e->spin = (int16_t)((e->spin * 3) / 4);
      gib_burst(e->x, e->y, 8, e->vx, e->vy, 3);
    } else {
      // Friction until it stops sliding.
      e->vz = 0;
      e->vx = (int16_t)((e->vx * 7) / 8);
      e->vy = (int16_t)((e->vy * 7) / 8);
      e->spin = (int16_t)((e->spin * 7) / 8);
      if (wr_abs(e->vx) < 2 && wr_abs(e->vy) < 2) {
        e->vx = 0; e->vy = 0; e->spin = 0; e->z = 0;
      }
    }
  }
}

WR_PART static void update_ped(Ent *e) {
  if (e->state == ST_DEAD) { ragdoll_update(e); return; }
  if ((uint8_t)e->aux == PA_FARE) return;         // waiting for the cab
  if ((uint8_t)e->aux == PA_MILITIA && domain_call(DOP_PERSON, (int)(e - g_ent))) return;

  int32_t d = wr_dist(e->x - g_p.x, e->y - g_p.y);

  // Panic if the player is armed and close, or has any wanted level.
  if (d < 3 * TILE && (g_p.weapon != W_FIST || g_p.wanted > 0 || g_p.in_car))
    e->state = ST_FLEE;
  else if (e->state == ST_FLEE && d > 6 * TILE)
    e->state = ST_WALK;

  if (e->state == ST_FLEE) {
    uint16_t away = (uint16_t)(angle_to(g_p.x, g_p.y, e->x, e->y));
    steer(e, away, 3000);
    if (!try_move(e, 7, false)) e->ang += 8192;
    if ((e->timer & 31) == 0) audio_sfx(e->sub == PED_WOMAN ? SFX_SCREAM_W : SFX_SCREAM_M);
    e->phase += 22;
  } else {
    if (!try_move(e, e->spd, false)) e->ang += 16384;   // bounce off, quarter turn
    if ((e->timer & 63) == 0) e->ang += (uint16_t)(wr_rand_n(3) - 1) * 16384;
    e->phase += (uint8_t)(3 + e->spd * 2);
  }
  e->timer++;
}

// Cruising speed for a model. Traffic accelerates back up to this after any
// knock - without it, a single fender-bender left a car permanently at zero
// and the streets slowly filled with dead vehicles nobody could explain.
static inline int32_t car_cruise(const Ent *e) {
  static const int8_t SPD[CAR_MODEL_COUNT] = { 13, 12, 10, 17, 9, 12, 16, 9, 14 };
  return SPD[e->sub % CAR_MODEL_COUNT];
}

// ---------------------------------------------------------------------------
// The driver.
//
// One controller for everything with wheels: traffic, cyclists, the
// ambulance and the police. It stores no route and knows no graph. Each
// frame it asks road_at() what road it is on, holds the right-hand lane by
// steering at a point a little way ahead in that lane, and when a crossing
// comes up it picks an exit once, aims at a point in the exit lane, and
// lets the same steering carry it round the corner. A road that simply
// ends is a crossing with nowhere to go straight, and a car with no exits
// at all aims back the way it came - which the rate-limited steering turns
// into a U-turn rather than a pivot.
//
// Seekers (police, ambulance) use the same code with one difference: at a
// crossing they take the exit that ends nearest their target.
// ---------------------------------------------------------------------------


#define LOOK       280          // lane-keeping lookahead
#define DECIDE     (TILE * 2)   // how far ahead a crossing is noticed
#define EXIT_D     (ROAD_HALF + 200)


// Does another carriageway cross mine at this point? If so, copy it out
// and return the heading along it that points to my left.
static bool crossing_at(int32_t x, int32_t y, uint16_t hd, uint8_t mykind,
                        Road *cr, uint16_t *cross) {
  Road r[4];
  int n = road_here(x, y, r);
  for (int i = 0; i < n; i++) {
    if (wr_abs(r[i].lat) > 70 || r[i].kind == mykind) continue;
    *cr = r[i];
    int32_t d = (int16_t)(uint16_t)(r[i].head - hd);
    *cross = (d > 0) ? r[i].head : (uint16_t)(r[i].head + 32768);
    return true;
  }
  return false;
}

static bool road_on(const Road *rd, int32_t x, int32_t y, uint16_t hd, int32_t d) {
  int32_t px, py;
  road_point(rd, x, y, hd, d, 0, &px, &py);
  return cell_at_point(px, py) == CELL_ROAD;
}

// Steer toward a world point at a bounded rate.
static void aim_at(Ent *e, int32_t ax, int32_t ay, int32_t rate) {
  steer(e, angle_to(e->x, e->y, ax, ay), rate);
}


void drive_along(Ent *e, bool seek, int32_t tx, int32_t ty,
                        int32_t lane, int32_t limit, bool is_car) {
  Road rd;
  int32_t c = fcos(e->ang), s = fsin(e->ang);
  bool on_road = true;
#ifdef WR_TRACE
  int32_t limit0 = limit; int why = 0;  // what capped the speed
#define WHY(v) (why = (v))
#else
#define WHY(v) ((void)0)
#endif

  if (!road_at(e->x, e->y, e->ang, &rd)) {
    // Off the tarmac: head for the nearest centreline and rejoin. On the
    // beach the nearest street centreline is the extension of a street
    // that has already ended, so head inland instead - the ring is in the
    // way and the ring is road.
    int32_t rx = e->x, ry = e->y;
    if (cell_at_point(rx, ry) == CELL_SAND || cell_at_point(rx, ry) == CELL_WATER) {
      int32_t d1 = wr_dist(rx - ISLAND_CX * TILE, ry - ISLAND_CY * TILE);
      int32_t d2 = wr_dist(rx - ISLAND_BX * TILE, ry - ISLAND_BY * TILE);
      rx = (d1 < d2 ? ISLAND_CX : ISLAND_BX) * TILE;
      ry = (d1 < d2 ? ISLAND_CY : ISLAND_BY) * TILE;
    } else {
      road_snap(&rx, &ry, NULL);
    }
    aim_at(e, rx, ry, 1500);
    e->wep = TURN_NONE;
    on_road = false;
    if (limit > 7) limit = 7;
  } else {
    int32_t dh = (int16_t)(uint16_t)(e->ang - rd.head);
    int fwd = (wr_abs(dh) <= 16384) ? 1 : -1;
    uint16_t hd = fwd > 0 ? rd.head : (uint16_t)(rd.head + 32768);
    int32_t hc = fcos(hd), hs = fsin(hd);
    // My lateral position relative to `hd`: lat is measured left of the
    // canonical heading, so flip it when I am driving the other way. The
    // right-hand lane is then simply -lane, whichever way I am going.
    int32_t want = -lane;

    // A crossing ahead? Decide once, and remember it until it is done.
    // Three probes half a tile apart cover 1.5 to 2.5 tiles ahead without
    // gaps, so a crossing cannot fall between the look and the "road ends"
    // test - which is how a street meeting the ring read as a dead end.
    if (e->wep == TURN_NONE) {
      int32_t jx = 0, jy = 0;
      uint16_t cross = 0;
      Road cr;
      bool have = false;
      for (int k = -1; k <= 1 && !have; k++) {
        road_point(&rd, e->x, e->y, hd, DECIDE + k * (TILE / 2), 0, &jx, &jy);
        have = crossing_at(jx, jy, hd, rd.kind, &cr, &cross);
      }
      bool ends = !road_on(&rd, e->x, e->y, hd, DECIDE + TILE / 2);
      if (!have) road_point(&rd, e->x, e->y, hd, DECIDE, 0, &jx, &jy);
      if (have || ends) {
        if (!have) cross = (uint16_t)(hd + 16384);
        uint16_t opts[3] = { hd, cross, (uint16_t)(cross + 32768) };
        bool ok[3];
        ok[0] = road_on(&rd, jx, jy, hd, TILE + TILE / 2);
        ok[1] = have && road_on(&cr, jx, jy, opts[1], TILE + TILE / 4);
        ok[2] = have && road_on(&cr, jx, jy, opts[2], TILE + TILE / 4);
        int pick = -1;
        if (seek) {
          int32_t best = 0x7FFFFFFF;
          for (int k = 0; k < 3; k++) {
            if (!ok[k]) continue;
            int32_t px = jx + fmul(2 * TILE, fcos(opts[k]));
            int32_t py = jy + fmul(2 * TILE, fsin(opts[k]));
            int32_t d = wr_dist(px - tx, py - ty);
            if (d < best) { best = d; pick = k; }
          }
        } else {
          // Mostly straight on, sometimes a turn, so traffic wanders the
          // whole grid rather than orbiting one block.
          int r = (int)wr_rand_n(10);
          int pref = (r < 6) ? 0 : (r < 8) ? 1 : 2;
          for (int k = 0; k < 3 && pick < 0; k++)
            if (ok[(pref + k) % 3]) pick = (pref + k) % 3;
        }
        e->wep = (pick < 0) ? TURN_BACK : (uint8_t)(TURN_STRAIGHT + pick);
        e->vx = (int16_t)(jx >> 2); e->vy = (int16_t)(jy >> 2);
        e->vz = (int16_t)(cross >> 1);
        TRACE("  decide f=%lu kind=%d hd=%u have=%d ends=%d cross=%u ok=%d%d%d pick=%d J=%d,%d\n",
              (unsigned long)g_frame, rd.kind, hd, have, ends, cross, ok[0], ok[1], ok[2], pick, (int)jx, (int)jy);
      }
    }

    int32_t canon = fwd > 0 ? want : -want;         // my lane, canonical frame
    if (e->wep == TURN_NONE || e->wep == TURN_STRAIGHT) {
      // Lane keeping: aim at a point LOOK ahead, sitting in my lane.
      int32_t ax, ay;
      road_point(&rd, e->x, e->y, hd, LOOK, canon, &ax, &ay);
      aim_at(e, ax, ay, 1400);
      // Past the crossing: forget it.
      if (e->wep == TURN_STRAIGHT) {
        int32_t jx = (int32_t)e->vx * 4, jy = (int32_t)e->vy * 4;
        if (((e->x - jx) * hc + (e->y - jy) * hs) > (ROAD_HALF << 10)) e->wep = TURN_NONE;
      }
    } else {
      int32_t jx = (int32_t)e->vx * 4, jy = (int32_t)e->vy * 4;
      uint16_t ex = (e->wep == TURN_LEFT)  ? (uint16_t)((uint16_t)e->vz << 1)
                  : (e->wep == TURN_RIGHT) ? (uint16_t)(((uint16_t)e->vz << 1) + 32768)
                  : (uint16_t)(hd + 32768);
      int32_t ec = fcos(ex), es = fsin(ex);
      int32_t along = ((e->x - jx) * hc + (e->y - jy) * hs) >> 10;
      if (e->wep == TURN_BACK) {
        // U-turn: turn toward the reversed road heading, left, through the
        // middle of the road. At walking pace the arc is a car-length
        // across, so it fits inside the carriageway; lane keeping then
        // settles it into the opposite lane.
        steer(e, (uint16_t)(ex + 4000), 1500);
        if (wr_abs((int16_t)(uint16_t)(e->ang - ex)) < 3000) e->wep = TURN_NONE;
        if (limit > 6) limit = 6;
      } else {
        // Hold the lane up to the box, then aim into the exit lane.
        if (along < -(ROAD_HALF + 40)) {
          int32_t ax, ay;
          road_point(&rd, e->x, e->y, hd, LOOK, canon, &ax, &ay);
          aim_at(e, ax, ay, 1400);
        } else {
          int32_t ax = jx + fmul(EXIT_D, ec) + fmul(lane, es);
          int32_t ay = jy + fmul(EXIT_D, es) - fmul(lane, ec);
          aim_at(e, ax, ay, 1600);
        }
        if (wr_abs((int16_t)(uint16_t)(e->ang - ex)) < 2500 &&
            ((e->x - jx) * ec + (e->y - jy) * es) > 0) e->wep = TURN_NONE;
        if (along > -(TILE + ROAD_HALF) && limit > 8) limit = 8;
      }
    }

    // Traffic lights on the grid. Family from the road, stop line just
    // short of the box; a car already in the box clears it.
    if (rd.kind <= RD_DIAG_V && !traffic_green(rd.kind)) {
      int32_t cross = (rd.kind == RD_DIAG_U) ? (e->x - e->y) : (e->x + e->y);
      int32_t rate  = (rd.kind == RD_DIAG_U) ? (hc - hs) : (hc + hs);
      if (rate) {
        int32_t m2 = cross % DIAG_PERIOD;
        if (m2 < 0) m2 += DIAG_PERIOD;
        int32_t remc = (rate > 0) ? (DIAG_PERIOD - m2) : m2;
        int32_t dist = wr_muldiv(remc, 1024, rate > 0 ? rate : -rate);
        dist -= ROAD_HALF + 130;
        if (dist >= 0 && dist < 640) { limit = wr_clamp(dist / 42, 0, limit); WHY(1); }
      }
    }
  }

  // Car following: match the vehicle ahead in my lane and stop short of
  // its bumper. Oncoming traffic is in the other lane and simply passes; a
  // wreck or a stopped car is an obstacle whichever way it faces. So is
  // the player on foot: slot -1 below stands for him, so that a walk
  // across the road is a walk, not a hit-and-run. (Traffic still does not
  // brake for anyone else; that is deliberate, and the police do not
  // brake for you.)
  // Off the tarmac there are no lanes and nothing to queue behind: two
  // cars shoved onto a pavement by the same crash used to wait for each
  // other until one of them was recycled.
  for (int j = -1; j < MAX_ENT && on_road; j++) {
    Ent *o = (j < 0) ? NULL : &g_ent[j];
    int32_t ox, oy, ospd; uint16_t oang; bool dead;
    if (j < 0) {
      if (g_p.in_car || g_p.dead || seek) continue;
      ox = g_p.x; oy = g_p.y; ospd = 0; oang = e->ang; dead = true;
    } else {
      if (o == e || o->state == ST_PARK) continue;
      if (o->type != ENT_CAR && o->type != ENT_COPCAR && o->type != ENT_AMB) continue;
      ox = o->x; oy = o->y; ospd = o->spd; oang = o->ang; dead = (o->state == ST_DEAD);
    }
    int32_t rx = ox - e->x, ry = oy - e->y;
    int32_t ahead = (rx * c + ry * s) >> 10;
    if (ahead <= 40 || ahead > 3 * TILE) continue;
    int32_t lat2 = (ry * c - rx * s) >> 10;
    if (wr_abs(lat2) > 110) continue;
    // Oncoming traffic is the other lane's business, unless it is a
    // wreck. Treating a stopped oncoming car as an obstacle is how two
    // cars nose to nose waited for each other forever.
    int32_t off = (int16_t)(uint16_t)(oang - e->ang);
    if (wr_abs(off) > 16384 && !dead) continue;
    bool obstacle = dead || (ospd <= 2);
    int32_t gap = ahead - 170;
    int32_t cap = (gap <= 60) ? 0
                : wr_clamp((obstacle ? 0 : ospd) + (gap - 60) / 40, 0, limit);
    if (cap < limit) { limit = cap; WHY(2 + j); }
  }

  // Stuck is not the same as stopped. A red light is at most a light cycle;
  // anything stationary longer than that turns round, and anything still
  // stationary long after that goes back in the pool.
  // `spin` is the stall clock: only a wreck tumbles, so on a live vehicle
  // it is free. (`anim` is not: ent_update() counts it down every frame.)
  if (e->spd <= 1) {
    if (e->spin < 600) e->spin++;
    if (e->spin == LIGHT_CYCLE + 60 && e->wep != TURN_BACK) {
      e->wep = TURN_BACK;
      e->vx = (int16_t)(e->x >> 2); e->vy = (int16_t)(e->y >> 2);
      e->vz = (int16_t)((uint16_t)(e->ang + 16384) >> 1);
    }
    if (e->spin >= 500 && !e->occupied && wr_dist(e->x - g_p.x, e->y - g_p.y) > 4 * TILE) {
      e->type = ENT_NONE; return;
    }
  } else if (e->spin) e->spin = 0;

  if (e->spd < limit) e->spd++;
  else if (e->spd > limit) e->spd = (int16_t)(e->spd - ((e->spd - limit > 3) ? 3 : 1));

  bool moved = try_move(e, e->spd, is_car);
  if (!moved) e->spd = (int16_t)((e->spd * 3) / 4);
  TRACE("f=%lu pos=%d,%d ang=%u spd=%d limit=%d/%d why=%d road=%d lat=%d wep=%d stall=%d moved=%d cell=%d\n",
        (unsigned long)g_frame, (int)e->x, (int)e->y, e->ang, e->spd, (int)limit, (int)limit0, why,
        road_at(e->x, e->y, e->ang, &rd) ? rd.kind : -1, (int)rd.lat, e->wep, e->spin, moved,
        cell_at_point(e->x, e->y));
  e->phase += (uint8_t)(2 + e->spd);
  e->timer++;
}

WR_PART static void update_car(Ent *e) {
  if (e->state == ST_DEAD) return;
  if (e->state == ST_FLEE && e->sub == CAR_BIKE) {
    // A riderless motorcycle: slides where it was flung, then lies there.
    if (e->spd > 0) {
      if (!try_move(e, e->spd, true)) e->spd = (int16_t)(e->spd / 2);
      if (g_frame & 1) e->spd--;
      e->phase += (uint8_t)e->spd;
    }
    return;
  }
  if (e->state == ST_PARK) {
    // Pinned where it was left: a parked car in a busy bay would otherwise
    // be shoved out of it over time by collision separation.
    if (!e->vx && !e->vy) { e->vx = (int16_t)(e->x >> 6); e->vy = (int16_t)(e->y >> 6); }
    int32_t hx = (int32_t)e->vx * 64, hy = (int32_t)e->vy * 64;
    if (wr_dist(e->x - hx, e->y - hy) > 96) { e->x = hx; e->y = hy; }
    e->spd = 0;
    return;
  }
  drive_along(e, false, 0, 0, LANE, car_cruise(e), true);
}

// Where the police think you are. While you are in the open this is simply
// where you are; the moment you are under cover it stops updating, so they
// converge on the spot they last had eyes on and mill about there instead
// of walking straight to a man they cannot possibly see. This is what makes
// a parking garage a tactic rather than a decoration.
int32_t s_seen_x, s_seen_y;

bool player_under_cover(void) {
  int li = lot_at(g_p.x, g_p.y);
  return li >= 0 && g_lot[li].kind == LOT_GARAGE;
}

WR_PART static void seen_update(void) {
  if (!player_under_cover()) { s_seen_x = g_p.x; s_seen_y = g_p.y; }
}

// A settled corpse: dead, on the ground, and done sliding.

// The ambulance. It ignores the lights - it is allowed to - drives to the
// nearest settled body, sits over it for a moment with the lights going,
// and the body is gone when it pulls away. `wep` holds its target slot + 1,
// which is a borrowed field, but the alternative was growing every entity
// by two bytes for one vehicle.

// A cyclist: faster than a walker, keeps to the road, and comes off the
// bike hard. He is a person for every other purpose - shooting one, running
// one down and the ragdoll that follows all go through the same code paths
// as any pedestrian.

// Burning cars, and the thing that finally sets them off.
WR_PART static void fire_update(void) {
  for (int i = 0; i < MAX_ENT; i++) {
    Ent *e = &g_ent[i];
    if (!e->fire) continue;

    bool person = (e->type == ENT_PED || e->type == ENT_COP);
    if (person) {
      // Alight. He runs - badly, veering, not toward anything - and the
      // burn is what kills him, a little at a time, so there is a few
      // seconds of him blundering about before he goes down. What is left
      // stays charred: `fire` is held at 1 after death as the marker, which
      // costs no extra field on an entity.
      if (e->state == ST_DEAD) { e->fire = 1; continue; }
      e->fire--;
      if ((g_frame & 3) == 0)
        gib_burst_debris(e->x, e->y, 30, 0, 0, 1,
                         (g_frame & 8) ? ARGB(3,2,0) : ARGB(3,0,0));
      e->state = ST_FLEE;
      e->spd = 8;
      e->ang += (uint16_t)(wr_rand_n(5000) - 2500);
      e->phase += 12;
      try_move(e, e->spd, false);
      if ((g_frame & 7) == 0) {
        if (e->hp > 7) e->hp -= 7;
        else {
          // kill_entity() lives in wr_game.c, so do it directly. No heat for
          // a fire death, which is a small inaccuracy worth the simplicity.
          // And no blood: a man who burns to death leaves char, not a pool.
          e->hp = 0;
          e->state = ST_DEAD;
          e->spd = 0;
          e->fire = 1;                 // stays charred
        }
      }
      continue;
    }

    // A wreck thrown by a blast: gravity, a couple of bounces, and a tumble
    // all the way down. Cars have the same z/vx/vy/spin fields ragdolls use;
    // nothing had ever put them in the air before.
    if (e->state == ST_DEAD && (e->z > 0 || e->vz)) {
      e->z = (int16_t)(e->z + e->vz);
      e->vz = (int16_t)(e->vz - 3);
      int32_t nx2 = e->x + e->vx, ny2 = e->y + e->vy;
      if (!city_blocked(nx2, ny2, true) || e->z > 24) { e->x = nx2; e->y = ny2; }
      else { e->vx = (int16_t)(-e->vx / 2); e->vy = (int16_t)(-e->vy / 2); }
      e->ang = (uint16_t)(e->ang + e->spin);
      if (e->z <= 0) {
        e->z = 0;
        if (e->vz < -5) {
          // Bounce, shedding speed, spin and a shower of parts each time.
          e->vz = (int16_t)((-e->vz * 2) / 5);
          e->vx = (int16_t)((e->vx * 3) / 5);
          e->vy = (int16_t)((e->vy * 3) / 5);
          e->spin = (int16_t)((e->spin * 2) / 3);
          gib_burst_debris(e->x, e->y, 20, e->vx, e->vy, 4, ARGB(2,2,2));
          audio_sfx(SFX_SMASH);
        } else {
          e->vz = 0; e->vx = 0; e->vy = 0; e->spin = 0;
        }
      }
    }

    e->fire--;
    // Flame and smoke off the bodywork while it burns.
    if ((g_frame & 3) == 0)
      gib_burst_debris(e->x + (int32_t)wr_rand_n(40) - 20,
                       e->y + (int32_t)wr_rand_n(40) - 20, 40,
                       0, 0, 1,
                       (g_frame & 8) ? ARGB(3,2,0) : ARGB(3,1,0));
    if (e->fire == 0) {
      // Wreckage. A car coming apart should throw panels and glass in every
      // direction, not just flash and sit there.
      for (int p = 0; p < 5; p++) {
        int32_t sx2 = (int32_t)wr_rand_n(36) - 18;
        int32_t sy2 = (int32_t)wr_rand_n(36) - 18;
        gib_burst_debris(e->x, e->y, 28 + p * 6, sx2, sy2, 3,
                         (p & 1) ? ARGB(2,2,2) : ARGB(1,1,1));
      }
      gib_burst_debris(e->x, e->y, 40, 0, 0, 6, ARGB(1,2,2));   // glass
      boom_add(e->x, e->y);
      vehicle_destroy(i, false);
      if (wr_dist(e->x - g_p.x, e->y - g_p.y) < 3 * TILE) wr_vibe(VIBE_HEAVY);
    }
  }
}

// Is there clear ground straight between two points? A handful of samples,
// which is all a 200-pixel screen ever justifies.
static bool line_clear(int32_t ax, int32_t ay, int32_t bx, int32_t by) {
  int32_t dx = bx - ax, dy = by - ay;
  int32_t d = wr_dist(dx, dy);
  if (d < 60) return true;
  int steps = (int)(d / 90);
  if (steps > 10) steps = 10;
  for (int i = 1; i <= steps; i++) {
    int32_t px = ax + wr_muldiv(dx, i, steps);
    int32_t py = ay + wr_muldiv(dy, i, steps);
    if (city_blocked(px, py, false)) return false;
  }
  return true;
}

// Route a man on foot around a block instead of into it.
//
// This needs no navigation graph and stores nothing, because the street
// layout is not arbitrary - it is two families of parallel lines at right
// angles. Work in band coordinates (u = x+y, v = x-y) and the whole problem
// collapses: every street is a line of constant u or constant v, moving
// along a u-street changes only your v, and vice versa. So the route from
// anywhere to anywhere is the Manhattan one - get onto the nearest
// centreline of the family that closes the bigger gap, follow it until that
// coordinate matches the target, then switch families. Two legs, computed
// in a dozen lines of arithmetic, no search and no memory.
static void cop_waypoint(const Ent *e, int32_t tx, int32_t ty,
                         int32_t *wx, int32_t *wy) {
  int32_t mu = e->x + e->y, mv = e->x - e->y;
  int32_t tu = tx + ty,     tv = tx - ty;

  // Snap to the centreline of whichever family we are nearest.
  int32_t cu = ((mu + DIAG_PERIOD / 2) / DIAG_PERIOD) * DIAG_PERIOD;
  int32_t cv = ((mv + DIAG_PERIOD / 2) / DIAG_PERIOD) * DIAG_PERIOD;

  int32_t du = tu - mu, dv = tv - mv;
  int32_t au = du < 0 ? -du : du, av = dv < 0 ? -dv : dv;

  int32_t gu, gv;
  if (av >= au) {
    // Close the v gap: ride a constant-u street, so hold u on its
    // centreline and let v run toward the target.
    gu = cu;
    gv = mv + (dv > 0 ? DIAG_PERIOD : -DIAG_PERIOD);
    if ((dv > 0 && gv > tv) || (dv < 0 && gv < tv)) gv = tv;
  } else {
    gv = cv;
    gu = mu + (du > 0 ? DIAG_PERIOD : -DIAG_PERIOD);
    if ((du > 0 && gu > tu) || (du < 0 && gu < tu)) gu = tu;
  }
  *wx = (gu + gv) / 2;
  *wy = (gu - gv) / 2;
}

// Fill the parking bays with real cars.
//
// The garages were drawn with decorative car-shaped boxes, which is a poor
// trade: they look like cars, so of course you try to steal one. These are
// ordinary vehicles with ST_PARK set - same models, same bodywork, same
// collisions, same explosions - so the obvious thing you want to do to a
// parked car works. They are capped well below the entity budget and left
// out of the traffic headcount, so filling a car park never starves the
// streets of moving traffic.
// Parked cars share the entity table with traffic, police and pedestrians.
// Raising the garage rate from two per map to a dozen pushed this cap's worth
// of slots into car parks near the player, and the shortfall came out of the
// things that matter: traffic stopped flowing and SWAT stopped deploying,
// because dispatch could not find free slots. Six is enough to make a garage
// look occupied and leaves the table room to breathe.
#define MAX_PARKED 10

// Cars in the garage bays: something to swap into. Refilled slowly, only
// when the table has room, never where something already stands.
WR_PART static void parked_refresh(void) {
  int used = 0, parked = 0;
  for (int i = 0; i < MAX_ENT; i++) {
    if (g_ent[i].type != ENT_NONE) used++;
    if (g_ent[i].type == ENT_CAR && g_ent[i].state == ST_PARK) parked++;
  }
  if (used > MAX_ENT - 6) return;

  // Nearest garage first, so the one you can see is the one that is
  // stocked; every garage in the cache gets at least two cars before any
  // gets a third.
  for (int pass = 0; pass < 2 && parked < MAX_PARKED; pass++)
  for (int li = 0; li < g_nlot && parked < MAX_PARKED; li++) {
    if (g_lot[li].kind != LOT_GARAGE || lot_is_airport(&g_lot[li])) continue;   // a terminal is not a car park
    int32_t slot[GARAGE_SLOTS][3];
    int ns = city_garage_slots(&g_lot[li], slot, GARAGE_SLOTS);
    int here = 0;
    for (int i = 0; i < MAX_ENT; i++)
      if (g_ent[i].type == ENT_CAR && g_ent[i].state == ST_PARK && lot_at(g_ent[i].x, g_ent[i].y) == li) here++;
    for (int s = 0; s < ns && parked < MAX_PARKED && (pass || here < 2); s++, here++) {
      bool taken = false;
      for (int i = 0; i < MAX_ENT && !taken; i++)
        if (g_ent[i].type != ENT_NONE &&
            wr_dist(g_ent[i].x - slot[s][0], g_ent[i].y - slot[s][1]) < 90) taken = true;
      if (taken || wr_dist(slot[s][0] - g_p.x, slot[s][1] - g_p.y) > 12 * TILE) continue;
      int fs = free_civil_slot();
      if (fs < 0) return;
      Ent *e = &g_ent[fs];
      memset(e, 0, sizeof(Ent));
      e->type = ENT_CAR;
      e->sub = (uint8_t)wr_rand_n(CAR_BIKE);                // cars, not bikes or buses
      e->hp = 100;
      e->state = ST_PARK;
      e->x = slot[s][0]; e->y = slot[s][1];
      e->ang = (uint16_t)slot[s][2];
      parked++;
    }
  }
}

WR_PART static void update_cop_foot(Ent *e) {
  // Pursuit reads from the last-seen point, not from the player.
  const int32_t px = s_seen_x, py = s_seen_y;
  if (e->state == ST_DEAD) { ragdoll_update(e); return; }

  int32_t d = wr_dist(e->x - px, e->y - py);
  uint16_t toward = angle_to(e->x, e->y, px, py);
  steer(e, toward, 2600);

  // Round the block, not through it. If nothing is in the way we walk
  // straight at him; if something is, we head for the next street corner on
  // the Manhattan route instead and re-evaluate when we get there.
  // Round the block, not through it. If nothing is in the way we walk
  // straight at him; if something is, we aim for the next corner on the
  // Manhattan route and re-evaluate on arrival. The blind probe below still
  // runs on top of this: the route says which way to go, the probe handles
  // the doorway, the parked car and the lamp post it cannot know about.
  if (d > 100 && !line_clear(e->x, e->y, px, py)) {
    int32_t wx, wy;
    cop_waypoint(e, px, py, &wx, &wy);
    toward = angle_to(e->x, e->y, wx, wy);
    steer(e, toward, 2600);
  }

  if (d > 100) {
    // Steer round the building instead of into it. The old behaviour was to
    // aim straight at the target and, on a blocked step, swing 45 degrees
    // and try again next frame - which walks a man into a wall, slides him
    // along it, and traps him completely in a dead end.
    //
    // This probes outward from the direct bearing in widening pairs and
    // takes the first heading with clear ground a stride ahead, so he peels
    // around a corner the way a person does. The chosen detour is held for
    // a couple of dozen frames (`door` is free on a person) rather than
    // re-decided every frame, because re-deciding at a corner makes him
    // jitter on the spot between two equally blocked options.
    if (e->door > 0) {
      e->door--;
      e->ang = (uint16_t)(toward + (int16_t)((e->anim & 0x80) ? 12000 : -12000));
    }
    if (!try_move(e, e->spd, false)) {
      static const int32_t PROBE[6] = { 5461, -5461, 12000, -12000, 20000, -20000 };
      bool got = false;
      for (int i = 0; i < 6 && !got; i++) {
        uint16_t cand = (uint16_t)(toward + PROBE[i]);
        int32_t nx = e->x + fmul(e->spd * 6 + 90, fcos(cand));
        int32_t ny = e->y + fmul(e->spd * 6 + 90, fsin(cand));
        if (city_blocked(nx, ny, false)) continue;
        e->ang = cand;
        // Remember which way we peeled, and commit to it briefly.
        if (PROBE[i] > 0) e->anim |= 0x80; else e->anim &= 0x7F;
        e->door = 22;
        got = true;
      }
      // Boxed in: turn out over a few frames rather than pivoting. A man
      // spinning on the spot is the tell that an AI has given up.
      if (!got) steer(e, (uint16_t)(e->ang + 8192), 2600);
      try_move(e, e->spd, false);
    }
    e->phase += (uint8_t)(3 + e->spd * 2);
  } else if (e->timer % 25 == 0 && (g_p.wanted || g_p.hostile)) {   // no heat on you: no nightstick
    e->anim = 7;
    // Nightstick range.
    player_absorb(6);
    g_p.hurt_flash = 4;
    audio_sfx(SFX_PUNCH);
  }

  // Armed response from three stars up.
  // Was 9 frames between rounds for a machine gun - four times a service
  // pistol, which is what made SWAT lethal on sight. 18 still reads as
  // automatic fire without deleting you in a second.
  int32_t rate = (e->wep == W_MG) ? 18 : 30;
  if (g_p.hostile && d < 6 * TILE && (e->timer % rate) == 0) {
    e->anim = 6;
    cop_shoot(e->x, e->y, toward, 40, 16, (e->wep == W_MG) ? 5 : (e->wep == W_SHOTGUN) ? 14 : 10,
              1, e->wep == W_SHOTGUN ? SFX_SHOTGUN : SFX_PISTOL);
  }
  e->timer++;
}

WR_PART static void update_cop_car(Ent *e) {
  if (e->state == ST_PARK) { e->spd = 0; return; }     // left at the kerb: stays
  const int32_t px = s_seen_x, py = s_seen_y;
  int32_t d = wr_dist(e->x - px, e->y - py);

  // Until you have used a gun, a patrol car pulls up two tiles short and
  // an officer gets out to make the arrest on foot. It waits there while
  // you are near and picks the chase up again if you run.
  // Pull up short and an officer gets out - to make the arrest below
  // hostile, to shoot from behind the door above it. Either way the car
  // waits while you are near and picks the chase up if you run.
  // Which cars ram: one in three, by slot. The rest pull up and put an
  // officer on the street with a shotgun, which is a fight you can have;
  // three cars taking turns to run you down was not.
  bool rammer = ((int)(e - g_ent) % 3) == 0;
  bool near = d < 2 * TILE + TILE / 2;
  if (near && (!g_p.hostile || !rammer || e->door)) {
    if (e->spd > 0) e->spd = (int16_t)(e->spd - (e->spd > 3 ? 3 : e->spd));
    if (e->spd) try_move(e, e->spd, true);
    if (!e->aux) {
      int s = free_slot_evict();
      if (s >= 0) {
        Ent *c = &g_ent[s];
        memset(c, 0, sizeof(Ent));
        c->type = ENT_COP; c->sub = PED_MAN; c->hp = 100; c->wep = g_p.hostile ? W_SHOTGUN : W_PISTOL;
        c->state = ST_CHASE; c->spd = 6;
        c->x = e->x - fmul(60, fsin(e->ang)); c->y = e->y + fmul(60, fcos(e->ang));
        c->ang = angle_to(c->x, c->y, px, py);
        e->aux = 1;
        audio_sfx(SFX_DOOR);
      }
    }
    e->timer++;
    return;
  }
  if (d > 4 * TILE) e->aux = 0;                         // he can get out again next time
  if (!g_p.hostile) { drive_along(e, true, px, py, LANE, 18, true); return; }

  // Close and in sight: leave the lane and go straight for him - on a
  // wide turn, not a pivot, and not again for a couple of seconds after
  // a hit (`door` counts that down). Otherwise follow the roads toward
  // where he was last seen, like everyone else.
  if (d < 3 * TILE && line_clear(e->x, e->y, px, py) && !e->door) {
    aim_at(e, px, py, 1100);
    if (e->spd < 16) e->spd++;
    try_move(e, e->spd, true);
    e->phase += (uint8_t)(2 + e->spd);
    e->wep = TURN_NONE;
  } else {
    drive_along(e, true, px, py, LANE, 18, true);
    if (e->type == ENT_NONE) return;
  }

  // Ramming. Against a driver it is the bodywork that takes it, and a
  // wrecked car dumps you onto the street where they can reach you. A
  // helicopter overhead is out of reach of a bumper.
  bool aloft = g_p.in_car && g_ent[g_p.in_car - 1].type == ENT_HELI &&
               g_ent[g_p.in_car - 1].z > 40;
  if (d < 120 && (e->timer % 10) == 0 && !aloft) {
    e->door = 250;
    if (g_p.in_car) {
      Ent *pc = &g_ent[g_p.in_car - 1];
      uint8_t dmg = (uint8_t)wr_clamp(4 + wr_abs(e->spd) / 4, 0, 20);
      if (pc->hp > dmg) pc->hp -= dmg;
      else vehicle_destroy(g_p.in_car - 1, true);
      if (!g_p.in_car) player_absorb(3);
      g_p.spd = (int16_t)((g_p.spd * 3) / 4);
      audio_sfx(SFX_SMASH);
    } else if (wr_abs(e->spd) < 8) {
      // Nudging you at a crawl: a shove. At speed, the foot tenant's
      // knockdown takes over - you go over the bonnet, not through the car.
      player_absorb(14);
      audio_sfx(SFX_CRASH);
    }
    g_p.hurt_flash = 5;
  }
  e->timer++;
}

// Boats potter about the bay. Open water in every direction, so all they have
// to do is turn away from the shore.
#if WR_BOATS
// Boats. A civilian potters about the bay, turning away from the shore;
// the marine unit steers at you, rams, and shoots from three stars.
WR_PART static void update_boat(Ent *e, bool chase) {
  int32_t la = 150;
  uint16_t toward = angle_to(e->x, e->y, g_p.x, g_p.y);
  if (chase) steer(e, toward, 3200);
  else if ((e->timer & 127) == 0) e->ang += (uint16_t)(wr_rand_n(6000) - 3000);

  if (water_blocked(e->x + fmul(la, fcos(e->ang)), e->y + fmul(la, fsin(e->ang)))) {
    // Land ahead: come round toward whichever side is open.
    uint16_t left = (uint16_t)(e->ang + 12000), right = (uint16_t)(e->ang - 12000);
    bool lok = !water_blocked(e->x + fmul(la, fcos(left)),  e->y + fmul(la, fsin(left)));
    bool rok = !water_blocked(e->x + fmul(la, fcos(right)), e->y + fmul(la, fsin(right)));
    uint16_t want2 = (lok && rok) ? (wr_rand_n(2) ? left : right)
                   : lok ? left : rok ? right : (uint16_t)(e->ang + 32768);
    steer(e, want2, 1800);
    if (!chase) e->spd = (int16_t)((e->spd * 3) / 4);
  } else {
    e->x += fmul(e->spd, fcos(e->ang));
    e->y += fmul(e->spd, fsin(e->ang));
  }
  e->phase += (uint8_t)(3 + e->spd / 2);
  e->timer++;
  if (!chase) return;

  int32_t d = wr_dist(e->x - g_p.x, e->y - g_p.y);
  if (d < 130 && (e->timer % 10) == 0) {
    if (g_p.in_car) {
      Ent *pv = &g_ent[g_p.in_car - 1];
      if (pv->hp > 10) pv->hp -= 10; else vehicle_destroy(g_p.in_car - 1, true);
    }
    player_absorb(6);
    g_p.hurt_flash = 5;
    audio_sfx(SFX_SMASH);
  }
  if (g_p.wanted >= 3 && d < 7 * TILE && (e->timer % 52) == 0)
    cop_shoot(e->x, e->y, toward, 42, 18, 14, 1, SFX_PISTOL);
}
#endif

static void crash_velocity(int idx, int32_t vx, int32_t vy);
void crash_damage(int idx, uint8_t dmg);
static void crash_push(int idx, int32_t px, int32_t py);


// Car crashes as physics rather than dice: momentum is exchanged along
// the line between the two cars, damage comes from the closing speed, one
// contact is one damage event, and every real hit throws debris.
// The three things a crash does to one car, so they are written once and
// applied to both. A wreck takes none of them: it is scenery.
static void crash_velocity(int idx, int32_t vx, int32_t vy) {
  Ent *e = &g_ent[idx];
  if (e->state == ST_DEAD) return;
  bool pl = g_p.in_car && (int)g_p.in_car - 1 == idx;
  e->spd = (int16_t)((vx * fcos(e->ang) + vy * fsin(e->ang)) >> 10);
  int32_t t = (vx * -fsin(e->ang) + vy * fcos(e->ang)) >> 10;
  int32_t lim = pl ? 700 : 5000;
  e->ang += (uint16_t)wr_clamp(t * 140, -lim, lim);
  if (pl) { g_p.spd = e->spd; g_p.ang = e->ang; }
}

void crash_damage(int idx, uint8_t dmg) {
  Ent *e = &g_ent[idx];
  if (e->state == ST_DEAD) return;
  if (g_p.in_car && (int)g_p.in_car - 1 == idx) dmg /= 5;   // yours lasts
  if (e->fire) e->fire = 1;                     // a knock sets off a burning car
  if (e->hp > dmg) e->hp -= dmg; else vehicle_destroy(idx, true);
}
static void crash_push(int idx, int32_t px, int32_t py) {
  Ent *e = &g_ent[idx];
  if (e->state != ST_DEAD && !city_blocked(e->x + px, e->y + py, true)) { e->x += px; e->y += py; }
  if (g_p.in_car && (int)g_p.in_car - 1 == idx) { g_p.x = e->x; g_p.y = e->y; }
}

WR_PART static void car_collisions(void) {
  for (int i = 0; i < MAX_ENT; i++)
    if (g_ent[i].hit_cd) g_ent[i].hit_cd--;

  for (int i = 0; i < MAX_ENT; i++) {
    Ent *a = &g_ent[i];
    if (a->type != ENT_CAR && a->type != ENT_COPCAR && a->type != ENT_AMB) continue;
    for (int j = i + 1; j < MAX_ENT; j++) {
      Ent *b = &g_ent[j];
      if (b->type != ENT_CAR && b->type != ENT_COPCAR && b->type != ENT_AMB) continue;
      bool a_dead = a->state == ST_DEAD, b_dead = b->state == ST_DEAD;
      if (a_dead && b_dead) continue;

      int32_t dx = b->x - a->x, dy = b->y - a->y;
      int32_t d = wr_dist(dx, dy);
      if (d > 172 || d == 0) continue;

      int32_t vax = (fcos(a->ang) * a->spd) >> 10, vay = (fsin(a->ang) * a->spd) >> 10;
      int32_t vbx = (fcos(b->ang) * b->spd) >> 10, vby = (fsin(b->ang) * b->spd) >> 10;
      int32_t nx = (dx * 1024) / d, ny = (dy * 1024) / d;
      int32_t closing = ((vax - vbx) * nx + (vay - vby) * ny) >> 10;

      // A motorcycle knocked over by a car: only when the car is yours or
      // a patrol car on the chase. Left to itself, traffic nudging a bike
      // in a queue was throwing riders across the city.
      bool a_bike = a->type == ENT_CAR && a->sub == CAR_BIKE && !a_dead;
      bool b_bike = b->type == ENT_CAR && b->sub == CAR_BIKE && !b_dead;
      Ent *other = a_bike ? b : a;
      bool violent = other->occupied || other->type == ENT_COPCAR;
      if (a_bike != b_bike && violent && closing > 4 && !a->hit_cd && !b->hit_cd) {
        a->hit_cd = b->hit_cd = 20;
        // The rider comes off: the scenery tenant's physics (bikehit.inc).
        domain_call(DOP_BIKE_HIT, (int)(((uint32_t)(closing & 0xFF) << 16) | ((uint32_t)(a_bike ? j : i) << 8) | (uint32_t)(a_bike ? i : j)));
        continue;
      }

      if (closing > 0) {
        // Momentum goes mostly to whichever car is NOT yours: a hit at speed
        // should throw the other car about, not stop you dead.
        int32_t j2 = (closing * 11) / 8;
        bool pa = g_p.in_car && (int)g_p.in_car - 1 == i, pb = g_p.in_car && (int)g_p.in_car - 1 == j;
        int32_t ja = a_dead ? 0 : (b_dead ? j2 : pa ? j2 / 4 : pb ? j2 : j2 / 2);
        int32_t jb = b_dead ? 0 : (a_dead ? j2 : pb ? j2 / 4 : pa ? j2 : j2 / 2);
        crash_velocity(i, vax - ((ja * nx) >> 10), vay - ((ja * ny) >> 10));
        crash_velocity(j, vbx + ((jb * nx) >> 10), vby + ((jb * ny) >> 10));

        if (closing > 9 && !a->hit_cd && !b->hit_cd) {
          uint8_t dmg = (uint8_t)wr_clamp((closing - 9) * 3, 0, 46);
          a->hit_cd = b->hit_cd = 14;
          // A tank crushes what it meets and does not feel it.
          bool ta = a->sub == CAR_TANK && a->type == ENT_COPCAR, tb = b->sub == CAR_TANK && b->type == ENT_COPCAR;
          crash_damage(i, ta ? 0 : tb ? 90 : dmg);
          crash_damage(j, tb ? 0 : ta ? 90 : dmg);
          if (g_p.in_car && ((int)g_p.in_car - 1 == i || (int)g_p.in_car - 1 == j))
            wr_vibe(closing > 16 ? VIBE_HEAVY : VIBE_MED);
          int32_t mx2 = a->x + dx / 2, my2 = a->y + dy / 2;
          int32_t tvx = ((-ny) * closing) >> 11, tvy = (nx * closing) >> 11;
          int nglass = (int)wr_clamp(closing, 6, 22);
          gib_burst_debris(mx2, my2, 20,  tvx,  tvy, nglass, ARGB(2,2,2));
          gib_burst_debris(mx2, my2, 16, -tvx, -tvy, nglass, ARGB(1,2,2));
          gib_burst_debris(mx2, my2, 30, tvy / 2, -tvx / 2, nglass / 2 + 3, ARGB(1,1,1));
          audio_sfx(closing > 14 ? SFX_SMASH : SFX_CRASH);
        }
      }

      // Separate them. A wreck does not move, so the live car takes the
      // whole shove.
      int32_t push = (180 - d) / 2 + 6;
      if (a_dead || b_dead) push *= 2;
      crash_push(i, -(dx * push) / d, -(dy * push) / d);
      crash_push(j,  (dx * push) / d,  (dy * push) / d);
    }
  }
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void ent_reset(void) {
  memset(g_ent, 0, sizeof(g_ent));
  memset(g_boom, 0, sizeof(g_boom));
  memset(g_gib, 0, sizeof(g_gib));
  memset(g_bullet, 0, sizeof(g_bullet));
  memset(g_blood, 0, sizeof(g_blood));
}

void ent_update(void) {
  int peds = 0, cars = 0, cops = 0, copcars = 0, heli = 0;
  int boats = 0, copboats = 0;
  (void)boats; (void)copboats;
  s_shooters = 0;                     // reset the firing budget each frame
  if (s_cop_fire_cd) s_cop_fire_cd--;
  g_heli_up = 0;

  for (int i = 0; i < MAX_ENT; i++) {
    Ent *e = &g_ent[i];
    if (e->type == ENT_NONE) continue;

    // The car the player is driving is moved by the player code.
    if (g_p.in_car && (int)g_p.in_car - 1 == i) { cars++; continue; }

    int32_t d = wr_dist(e->x - g_p.x, e->y - g_p.y);
    // An ambulance on a call is exempt, like the helicopter. The cull measures
    // distance from the PLAYER, but an ambulance is not driving toward the
    // player - it is driving toward a body that may be anywhere. It was being
    // deleted en route every time the call was further out than the cull
    // radius, which is why three separate changes inside update_amb() moved
    // its closest approach by exactly zero: the vehicle never lived long
    // enough for any of them to matter.
    bool flying = (e->type == ENT_HELI && e->state != ST_PARK);
    // Cull far away - or half as far if it is behind the camera.
    int32_t rel = (int16_t)(uint16_t)(angle_to(g_p.x, g_p.y, e->x, e->y) - g_p.ang);
    int32_t reach = (wr_abs(rel) > 24576 && e->type == ENT_CAR && e->state == ST_DRIVE) ? DESPAWN / 2 : DESPAWN;
    if (d > reach && !flying && !e->occupied) { e->type = ENT_NONE; continue; }

    // Indoors, people follow the house's script, not the street's.
    if (g_p.inside && e->aux && inside_person(e)) {
      if (e->type == ENT_COP) cops++;
      continue;
    }
    switch (e->type) {
      case ENT_PED:    peds++;    update_ped(e);      break;
      case ENT_CAR:
        // A parked car is not traffic; counting it as such would let a full
        // car park stop dispatch sending any moving cars at all.
        if (e->state != ST_PARK) cars++;
        update_car(e);
        break;
      case ENT_COP:    if (e->state != ST_DEAD) cops++;   update_cop_foot(e); break;   // the dead are not on duty
      case ENT_HELI:
        // Air support stands down when the heat is off - unless it is on
        // the ground or in your hands, in which case it is just a vehicle.
        if (!want_heli() && e->state == ST_CHASE) { e->type = ENT_NONE; break; }
        if (e->state != ST_PARK && !e->occupied) { heli++; g_heli_up = 1; }
        domain2_call(DOP_ENT, i);
        break;
      case ENT_AMB:
        // Its tenant is on the THREAT bench at zero to three stars; with no
        // tenant (a war zone) it goes home.
        if (e->state != ST_DEAD && !domain2_call(DOP_AMB, i)) e->type = ENT_NONE;
        break;
      case ENT_COPCAR:
        if (e->state == ST_DEAD) break;              // a wreck does not chase
        copcars++;
        if (e->sub == CAR_TANK) { if (!e->occupied) domain2_call(DOP_ENT, i); break; }   // the guard's
        update_cop_car(e);  break;
#if WR_BOATS
      case ENT_BOAT:   boats++;   update_boat(e, false); break;
      case ENT_COPBOAT:
        // Only worth having while you are actually on the water.
        if (!want_copboats()) { e->type = ENT_NONE; break; }
        copboats++; update_boat(e, true);
        break;
#endif
    }
  }

  // Top up the population a little each frame rather than all at once.
  // Twice the city. The frame budget took the last round of detail without
  // complaint, so the streets carry a real crowd now.
  // Double the original nine walkers and six cars, with cyclists on top -
  // which is about what the streets can carry before a getaway turns into
  // an unavoidable massacre and the heat can never fall.
  // The crowd thins as the heat rises - partly because that is what a
  // crowd does when the shooting starts, mostly because the police need
  // the slots. 55 entities wanted at five stars into a 48-slot table meant
  // the helicopter could never respawn.
  if (!g_p.inside && island_at(g_p.x, g_p.y) != ISL_SECRET) {   // indoors, the room is furnished by wr_inside.c; the island by its residents
    if (peds < 12 - 2 * g_p.wanted) spawn(ENT_PED);
    if (cars < 15 - g_p.wanted) spawn(ENT_CAR);
    if (cops < want_cops()) spawn(ENT_COP);
    if (copcars < want_copcars()) spawn(ENT_COPCAR);
    if (heli < want_heli()) spawn(ENT_HELI);
#if WR_BOATS
    if (boats < 4) spawn(ENT_BOAT);
    if (copboats < want_copboats()) spawn(ENT_COPBOAT);
#endif
    if ((g_frame & 31) == 0) parked_refresh();
  }
  seen_update();
  car_collisions();
  fire_update();
  gib_update();

  for (int i = 0; i < MAX_BLOOD; i++) if (g_blood[i].life) g_blood[i].life--;
  for (int i = 0; i < MAX_BOOM;  i++) if (g_boom[i].life)  g_boom[i].life--;

  // Attack poses and car doors both just count back down to rest.
  for (int i = 0; i < MAX_ENT; i++) {
    if (g_ent[i].anim) g_ent[i].anim--;
    if (g_ent[i].door) g_ent[i].door = (uint8_t)(g_ent[i].door > 4 ? g_ent[i].door - 4 : 0);
  }
}

// Nearest live entity of a given type within `range`. Returns -1 for none.
int ent_nearest(uint8_t type, int32_t range) {
  int best = -1; int32_t bd = range;
  for (int i = 0; i < MAX_ENT; i++) {
    if (g_ent[i].type != type) continue;
    if (g_ent[i].state == ST_DEAD) continue;
    int32_t d = wr_dist(g_ent[i].x - g_p.x, g_ent[i].y - g_p.y);
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}

bool ent_any_cop_near(int32_t range, bool include_air) {
  for (int i = 0; i < MAX_ENT; i++) {
    // Boats and the helicopter count too. They used not to, which made the
    // marine unit self-destructing: it would be dispatched, nothing would
    // reset the evasion timer because no LAND unit was nearby, and a second
    // later dispatch decided you had got away and deleted the boat that was
    // ramming you.
    if (g_ent[i].type != ENT_COP && g_ent[i].type != ENT_COPCAR &&
        g_ent[i].type != ENT_COPBOAT && g_ent[i].type != ENT_HELI) continue;
    if (g_ent[i].type == ENT_HELI &&
        (!include_air || g_ent[i].occupied || g_ent[i].state == ST_PARK)) continue;
    if (g_ent[i].state == ST_DEAD) continue;
    // An aircraft trailing half a mile back is not keeping eyes on you.
    // Using the full ground sight range for it meant that once the
    // helicopter was up, the evasion timer never started at all.
    int32_t r = (g_ent[i].type == ENT_HELI) ? (4 * TILE) : range;
    if (wr_dist(g_ent[i].x - g_p.x, g_ent[i].y - g_p.y) < r) return true;
  }
  return false;
}
