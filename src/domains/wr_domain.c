#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"

// ---------------------------------------------------------------------------
// The loader.
//
// Blob format, from tools/mkdomain.py:
//
//     u16 code_len, u16 nreloc, [ code + tables ], [ u16 offsets ]
//
// The domain is linked at base 0, so every address inside it is really an
// offset. Loading is: copy into the arena, then add the arena's address to
// the 32-bit word at each listed offset. Branches inside the blob are
// PC-relative and need nothing; calls into the host go through HostCtx and
// need nothing either.
//
// The arena is sized to the largest domain. It is the whole cost of the
// scheme: one arena, however many domains.
// ---------------------------------------------------------------------------

// Generated at build time from the blobs that were actually produced - see
// the wscript. It holds the largest domain and does not grow as domains are
// added, which is the entire point. Hard-coding it was a mistake: a compiler
// that emits 28 more bytes than the one you sized against makes the loader
// refuse the blob, and the failure is silent.
#if WR_DOMAINS
#include "wr_arena.h"
#else
#define ARENA_BYTES  8
#define ARENA2_BYTES 8
#define ARENA3_BYTES 8
#endif

#if WR_MONOLITH
int dom_air_entry(const HostCtx *h, int op, int arg);
int dom_taxi_entry(const HostCtx *h, int op, int arg);
int dom_arrest_entry(const HostCtx *h, int op, int arg);
int dom_guard_entry(const HostCtx *h, int op, int arg);
int dom_secret_entry(const HostCtx *h, int op, int arg);
int dom_home_entry(const HostCtx *h, int op, int arg);
int dom_island_entry(const HostCtx *h, int op, int arg);
int dom_h_melee_entry(const HostCtx *h, int op, int arg);
int dom_h_guns_entry(const HostCtx *h, int op, int arg);
int dom_h_flame_entry(const HostCtx *h, int op, int arg);
int dom_h_rocket_entry(const HostCtx *h, int op, int arg);
int dom_h_car_entry(const HostCtx *h, int op, int arg);
int dom_h_board_entry(const HostCtx *h, int op, int arg);
int dom_h_grenade_entry(const HostCtx *h, int op, int arg);
int dom_h_fly_entry(const HostCtx *h, int op, int arg);
int dom_h_dead_entry(const HostCtx *h, int op, int arg);
// Statically linked build: the domains are ordinary functions compiled into
// the image. Used by the host test harness, and as the fallback if a build
// ever needs to ship without resources.
int dom_city_entry(const HostCtx *h, int op, int arg);
int dom_ghetto_entry(const HostCtx *h, int op, int arg);
int dom_coast_entry(const HostCtx *h, int op, int arg);
int dom_club_entry(const HostCtx *h, int op, int arg);
int dom_store_entry(const HostCtx *h, int op, int arg);
int dom_diner_entry(const HostCtx *h, int op, int arg);
int dom_airport_entry(const HostCtx *h, int op, int arg);
int dom_airside_entry(const HostCtx *h, int op, int arg);
int dom_menu_entry(const HostCtx *h, int op, int arg);
#else
static uint8_t s_arena[ARENA_BYTES] __attribute__((aligned(8)));
static uint8_t s_arena2[ARENA2_BYTES] __attribute__((aligned(8)));
static uint8_t s_arena3[ARENA3_BYTES] __attribute__((aligned(8)));
static domain_fn s_entry, s_entry2, s_entry3;
#endif

static uint8_t s_current, s_current2, s_current3;
static uint8_t s_depth;             // calls into the PLACE tenant on the stack

// --- the context -----------------------------------------------------------
// Thin wrappers where the host's version is inline or has a different shape.
static int32_t  h_fsin(uint16_t a) { return fsin(a); }
static int32_t  h_fcos(uint16_t a) { return fcos(a); }
static int32_t  h_fmul(int32_t a, int32_t b) { return fmul(a, b); }
static int32_t  h_dist(int32_t dx, int32_t dy) { return wr_dist(dx, dy); }
static uint32_t h_rand_n(uint32_t n) { return wr_rand_n(n); }
static void     h_absorb(int32_t dmg) { player_absorb((uint8_t)dmg); }
static int32_t  h_clamp(int32_t v, int32_t lo, int32_t hi) { return wr_clamp(v, lo, hi); }
static int32_t  h_abs(int32_t v) { return wr_abs(v); }
extern int32_t s_eye_x, s_eye_y, s_psx, s_psy, s_pdepth, s_seen_x, s_seen_y;
void hud_text(GContext *ctx, const char *s, const char *font, int x, int y, int w, int h, GColor col);
void hud_marker(GContext *ctx, int32_t x, int32_t y, int32_t z, GColor col);
int free_slot(void); int free_slot_evict(void);
void drive_along(Ent *e, bool seek, int32_t tx, int32_t ty, int32_t lane, int32_t cruise, bool police);
void ent_spawn(uint8_t type);
void fire_bullet_kind(uint16_t ang, uint8_t dmg, uint8_t kind);
void fire_bullet(uint16_t ang, uint8_t dmg);
void damage_entity(Ent *e, uint8_t dmg);
void kill_entity(Ent *e);
void steer(Ent *e, uint16_t target, int32_t rate);
void cop_shoot(int32_t x, int32_t y, uint16_t aim, int32_t spd, uint8_t life,
               uint8_t dmg, uint8_t flags, uint8_t sfx);

static const HostCtx s_ctx = {
  &g_p, &g_in, g_ent, g_prop, g_lot, g_gib, g_pickup, &g_nlot, &g_frame,
  h_fsin, h_fcos, h_fmul, h_dist, angle_to, h_rand_n,
  cell_at_point, audio_sfx, game_caption, pickup_drop, gib_burst_debris,
  money_burst, h_absorb, PROP_HP,
  gfx_part, gfx_taper, gfx_world_poly,
  gfx_clip_world_poly, gfx_fill_poly, gfx_all_ok, gfx_area2, gfx_project, gfx_parts,
  gfx_ground_band, gfx_shade, gfx_depth_at, gfx_set_stipple, wr_sqrt, wr_muldiv,
  h_clamp, h_abs, &s_eye_x, &s_eye_y, &s_psx, &s_psy, &s_pdepth,
  bridge_find, ent_fire, ent_nearest_of, island_at,
  cop_shoot, free_slot, free_slot_evict, steer, vehicle_destroy, money_on_death, gib_burst,
  add_heat, wr_vibe, city_blocked, gfx_ground_quad, &s_seen_x, &s_seen_y, &g_heli_up,
  g_bullet, road_snap, wr_rand, ISLANDS,
  &g_menu, &g_taxi, graphics_context_set_fill_color, graphics_context_set_stroke_color,
  graphics_fill_rect, graphics_draw_round_rect, graphics_fill_circle, hud_text, hud_marker, gfx_utoa,
  audio_set_volume, audio_set_enabled, graphics_draw_line, g_venue, &g_nvenue, BRIDGES, lot_is_airport, &g_park_col, &g_lot_col, car_half_dims, city_garage_slots, crash_damage, &g_blame, city_set_interior, ent_reset, props_refresh, pickups_refresh, city_refresh, cell_at, drive_along, blood_add, ent_spawn, model_car,
  fire_bullet_kind, fire_bullet, damage_entity, prop_hit,
  kill_entity, cell_at_world, lot_at, ent_nearest, water_blocked, domain_call, domain2_call,
  bridge_in_gap, city_shop_door, inside_leave, inside_violence, memset,
};

// ---------------------------------------------------------------------------

uint8_t domain_current(void) { return s_current; }
#ifdef WR_PROFILE
uint8_t domain2_current(void) { return s_current2; }   // the harness only
#endif

void domain_drop(void) {
  s_current = DOM_NONE;
#if !WR_MONOLITH
  s_entry = NULL;
#endif
}

// How many loads have failed, and the last size that would not fit. Nothing
// reads these in a shipped build; they exist so that "the buildings are
// gone" can be diagnosed in one look at the debugger instead of a day.
uint8_t g_domain_fail;
uint16_t g_domain_toobig;

static bool load_into(uint8_t which, uint8_t *arena, uint16_t cap, domain_fn *entry) {
#if WR_MONOLITH
  (void)arena; (void)cap; (void)entry; (void)which;
  return true;
#else
  *entry = NULL;
  static const uint8_t RES[DOM_COUNT] = { 0, RESOURCE_ID_DOM_CITY, RESOURCE_ID_DOM_GHETTO,
    RESOURCE_ID_DOM_COAST, RESOURCE_ID_DOM_CLUB, RESOURCE_ID_DOM_STORE, RESOURCE_ID_DOM_DINER,
    RESOURCE_ID_DOM_AIRPORT, RESOURCE_ID_DOM_AIRSIDE, RESOURCE_ID_DOM_MENU, RESOURCE_ID_DOM_AIR, RESOURCE_ID_DOM_TAXI,
    RESOURCE_ID_DOM_ARREST, RESOURCE_ID_DOM_GUARD, RESOURCE_ID_DOM_SECRET, RESOURCE_ID_DOM_HOME, RESOURCE_ID_DOM_ISLAND,
    RESOURCE_ID_DOM_H_MELEE, RESOURCE_ID_DOM_H_GUNS, RESOURCE_ID_DOM_H_FLAME, RESOURCE_ID_DOM_H_ROCKET,
    RESOURCE_ID_DOM_H_CAR, RESOURCE_ID_DOM_H_BOARD, RESOURCE_ID_DOM_H_GRENADE, RESOURCE_ID_DOM_H_FLY, RESOURCE_ID_DOM_H_DEAD };
  ResHandle h = resource_get_handle(RES[which]);
  size_t sz = resource_size(h);
  if (sz < 4) { g_domain_fail++; return false; }
  uint8_t head[4];
  if (resource_load_byte_range(h, 0, head, 4) != 4) { g_domain_fail++; return false; }
  uint16_t code_len = (uint16_t)(head[0] | (head[1] << 8));
  uint16_t nreloc   = (uint16_t)(head[2] | (head[3] << 8));
  if (code_len > cap) { g_domain_fail++; g_domain_toobig = code_len; return false; }
  if (sz != (size_t)(4 + code_len + nreloc * 2)) { g_domain_fail++; return false; }
  if (resource_load_byte_range(h, 4, arena, code_len) != code_len) { g_domain_fail++; return false; }
  uint32_t base = (uint32_t)(uintptr_t)arena;
  for (uint16_t i = 0; i < nreloc; i++) {
    uint8_t ob[2];
    if (resource_load_byte_range(h, 4 + code_len + i * 2, ob, 2) != 2) { g_domain_fail++; return false; }
    uint16_t off = (uint16_t)(ob[0] | (ob[1] << 8));
    if ((uint32_t)off + 4 > code_len) { g_domain_fail++; return false; }
    *(uint32_t *)(void *)(arena + off) += base;
  }
  *entry = (domain_fn)(void *)((uintptr_t)arena | 1u);     // Thumb bit
  return true;
#endif
}

#ifdef WR_SWAP_HOOK
// Harness only: called whenever the PLACE bench changes tenant while a call
// into the old tenant is still on the stack - the monolith cannot see that,
// and on hardware it is a crash.
void wr_swap_hook(uint8_t from, uint8_t to, int depth);
#endif

bool domain_use(uint8_t which) {
  if (which == s_current) return true;
#ifdef WR_SWAP_HOOK
  wr_swap_hook(s_current, which, s_depth);
#endif
  if (which == DOM_NONE) { domain_drop(); return true; }
  s_current = DOM_NONE;
#if WR_MONOLITH
  s_current = which; return true;
#else
  if (!load_into(which, s_arena, ARENA_BYTES, &s_entry)) return false;
  s_current = which; return true;
#endif
}

static bool domain2_use(uint8_t which) {
  if (which == s_current2) return true;
  s_current2 = DOM_NONE;
  if (which == DOM_NONE) {
#if !WR_MONOLITH
    s_entry2 = NULL;
#endif
    return true;
  }
#if WR_MONOLITH
  s_current2 = which;
#else
  if (!load_into(which, s_arena2, ARENA2_BYTES, &s_entry2)) return false;
  s_current2 = which;
#endif
  domain2_call(DOP_ENTER, 0);     // a new tenant looks at what is already on the street
  return true;
}

int domain2_call(int op, int arg) {
  if (!s_current2) return 0;
#if WR_MONOLITH
  return (s_current2 == DOM_AIR) ? dom_air_entry(&s_ctx, op, arg)
       : (s_current2 == DOM_ARREST) ? dom_arrest_entry(&s_ctx, op, arg)
       : (s_current2 == DOM_GUARD) ? dom_guard_entry(&s_ctx, op, arg)
       : (s_current2 == DOM_ISLAND) ? dom_island_entry(&s_ctx, op, arg) : dom_taxi_entry(&s_ctx, op, arg);
#else
  return s_entry2 ? s_entry2(&s_ctx, op, arg) : 0;
#endif
}

// Air support is resident from four stars, and stays resident while any
// aircraft exists - including the one you stole, which is a vehicle in the
// kernel's eyes but is flown, drawn and armed by this domain. Nothing may be
// dropped while an entity of its owns is alive.
// The taxi takes the same bench when there is no heat at all: a fare needs
// zero stars. Between one and three stars the bench is empty.
// Escalation. The force, then air support at four stars, then - at five,
// once you have killed enough officers that they have stopped winning -
// the national guard. The tier latches until the heat is gone. Air support
// stands down for the guard: while a helicopter exists the bench stays the
// air's (it may be the one you are flying), and the guard's DOP_WANT of
// zero sends the police's own home. A tank on the street keeps the guard
// resident - it may be the one you are driving.
void domain_threat(void) {
  bool any = g_p.in_car && g_ent[g_p.in_car - 1].type == ENT_HELI;
  for (int i = 0; i < MAX_ENT && !any; i++) if (g_ent[i].type == ENT_HELI) any = true;
  bool tank = false;
  for (int i = 0; i < MAX_ENT && !tank; i++) if (g_ent[i].type == ENT_COPCAR && g_ent[i].sub == CAR_TANK && g_ent[i].state != ST_DEAD) tank = true;
  if (g_p.wanted >= 5 && g_p.copkills >= 6) g_p.tier = 2;
  if (g_p.wanted == 0 && !tank) g_p.tier = 0;
  uint8_t want = island_at(g_p.x, g_p.y) == ISL_SECRET ? DOM_ISLAND      // no police there: the residents
               : ((g_p.tier == 2 && !any) || tank) ? DOM_GUARD
               : (g_p.wanted >= 4 || any) ? DOM_AIR
               : (g_p.wanted == 0 && !g_p.inside) ? DOM_TAXI : DOM_ARREST;
  domain2_use(want);
}

// A domain must never unload itself: inside_exit() would load the scenery
// into the arena the caller's code is still executing from. So a room asks
// (g_in.leave) and the kernel leaves it here, after the call has returned
// and nothing is running in the arena. The club's bouncers crashed the
// watch this way, and the airport would have.
static void after_call(void) {
  if (s_depth) return;
  if (g_in.leave) { g_in.leave = 0; inside_exit_now(); return; }
  if (g_in.after == 3) {                  // the airport's security gate: airside is the other tenant
    g_in.after = 0;
    if (domain_use(DOM_AIRSIDE)) domain_call(DOP_ENTER, 0);
  }
}

int domain_call(int op, int arg) {
  if (!s_current) return 0;
  int r;
  s_depth++;
#if WR_MONOLITH
  r = (s_current == DOM_CITY)   ? dom_city_entry(&s_ctx, op, arg)
    : (s_current == DOM_GHETTO) ? dom_ghetto_entry(&s_ctx, op, arg)
    : (s_current == DOM_COAST)  ? dom_coast_entry(&s_ctx, op, arg)
    : (s_current == DOM_CLUB)   ? dom_club_entry(&s_ctx, op, arg)
    : (s_current == DOM_DINER)  ? dom_diner_entry(&s_ctx, op, arg)
    : (s_current == DOM_AIRPORT) ? dom_airport_entry(&s_ctx, op, arg)
    : (s_current == DOM_AIRSIDE) ? dom_airside_entry(&s_ctx, op, arg)
    : (s_current == DOM_SECRET) ? dom_secret_entry(&s_ctx, op, arg)
    : (s_current == DOM_HOME) ? dom_home_entry(&s_ctx, op, arg)
    : (s_current == DOM_MENU)   ? dom_menu_entry(&s_ctx, op, arg)
                                : dom_store_entry(&s_ctx, op, arg);
#else
  r = s_entry ? s_entry(&s_ctx, op, arg) : 0;
#endif
  s_depth--;
  after_call();
  return r;
}

// Which island's look.
void domain_scenery(void) {
  if (g_p.inside) return;
  static const uint8_t SCENE[N_ISLAND] = { DOM_CITY, DOM_COAST, DOM_GHETTO, DOM_SECRET };
  uint8_t want = SCENE[island_at(g_p.x, g_p.y)];
  if (want != s_current) {
    g_park_col = ARGB(0,2,0); g_lot_col = ARGB(2,2,1);   // the defaults; the scenery may change them
    if (domain_use(want)) domain_call(DOP_ENTER, 0);
  }
}

// --- the hands ---------------------------------------------------------------
static bool domain3_use(uint8_t which) {
  if (which == s_current3) return true;
  s_current3 = DOM_NONE;
  if (which == DOM_NONE) {
#if !WR_MONOLITH
    s_entry3 = NULL;
#endif
    return true;
  }
#if WR_MONOLITH
  s_current3 = which; return true;
#else
  if (!load_into(which, s_arena3, ARENA3_BYTES, &s_entry3)) return false;
  s_current3 = which; return true;
#endif
}

int domain3_call(int op, int arg) {
  if (!s_current3) return 0;
#if WR_MONOLITH
  switch (s_current3) {
    case DOM_H_MELEE:  return dom_h_melee_entry(&s_ctx, op, arg);
    case DOM_H_GUNS:   return dom_h_guns_entry(&s_ctx, op, arg);
    case DOM_H_FLAME:  return dom_h_flame_entry(&s_ctx, op, arg);
    case DOM_H_CAR:    return dom_h_car_entry(&s_ctx, op, arg);
    case DOM_H_BOARD:  return dom_h_board_entry(&s_ctx, op, arg);
    case DOM_H_GRENADE: return dom_h_grenade_entry(&s_ctx, op, arg);
    case DOM_H_FLY:    return dom_h_fly_entry(&s_ctx, op, arg);
    case DOM_H_DEAD:   return dom_h_dead_entry(&s_ctx, op, arg);
    default:           return dom_h_rocket_entry(&s_ctx, op, arg);
  }
#else
  return s_entry3 ? s_entry3(&s_ctx, op, arg) : 0;
#endif
}

// The weapon in your hands decides the tenant. Money is the room's own
// business, and in a vehicle your hands are on the wheel: nothing.
void domain_hands(void) {
  uint8_t w = g_p.weapon, want;
  bool car = g_p.in_car && (g_ent[g_p.in_car - 1].type == ENT_CAR || g_ent[g_p.in_car - 1].type == ENT_COPCAR || g_ent[g_p.in_car - 1].type == ENT_AMB);
  if (g_p.dead) want = DOM_H_DEAD;                 // the cinematic, and the way back
  else if (car) want = DOM_H_CAR;
  else if (g_p.in_car) want = DOM_H_FLY;           // flying
  else if (w == W_CASH) want = DOM_H_MELEE;        // indoors you still walk
  else if (w == W_FLAME)  want = DOM_H_FLAME;
  else if (w == W_ROCKET) want = DOM_H_ROCKET;
  else if (w == W_GRENADE) want = DOM_H_GRENADE;
  else if (w >= W_PISTOL) want = DOM_H_GUNS;
  else                    want = DOM_H_MELEE;
  domain3_use(want);
}

// The hijacked airliner, made by the air domain once the airport room is
// gone. The domain is loaded for it here, since no aircraft exists yet to
// keep it resident.
void domain_air_spawn_plane(void) {
  if (domain3_use(DOM_H_FLY)) domain3_call(DOP_SPAWN, 0);   // the flying tenant makes it, and then flies it
}

void domain_board(void) {
  if (domain3_use(DOM_H_BOARD)) domain3_call(DOP_VEHICLE, 0);
  domain_hands();                       // in the car now, or back to the weapon
}
