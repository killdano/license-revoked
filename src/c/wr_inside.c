#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"

// ---------------------------------------------------------------------------
// Money, and the two places to spend it.
//
// Cash comes off bodies the way blood does - a burst of bills that arc,
// bounce, skid and come to rest as pickups around the corpse rather than
// on it. Indoors, the money gun throws it back out ten dollars at a time.
//
// Both interiors are the same machine: a room built from four wall lots
// and a handful of props, a door in the front wall, the outside world
// parked while you are in, and a set of house rules applied every frame.
// The gun store sells guns to anyone who pays and calls the police on
// anyone who does not. The club sells a show, one bill at a time, and the
// door staff deal with anyone who forgets where they are.
// ---------------------------------------------------------------------------

Inside g_in;

#define BOUNCER_HP 150
#define STAGE_H    30

static inline int32_t room_x1(void) { return ROOM_OX + g_in.w; }
static inline int32_t room_y1(void) { return ROOM_OY + g_in.h; }
// Fixed spots in the club.
static inline int32_t pole_x(void)  { return ROOM_OX + g_in.w / 2; }
static inline int32_t pole_y(void)  { return ROOM_OY + TILE / 2 + 20; }
static inline int32_t chair_x(void) { return ROOM_OX + g_in.w / 2; }
static inline int32_t chair_y(void) { return ROOM_OY + TILE + TILE / 2 + 60; }
static inline int32_t door_x(void)  { return ROOM_OX + g_in.w / 2; }

// ---------------------------------------------------------------------------
// Cash
// ---------------------------------------------------------------------------


void money_burst(int32_t x, int32_t y, int n) {
  for (int k = 0; k < n; k++) {
    // Reuse the debris slot search; then re-tag the newest one as money.
    gib_burst_debris(x, y, 46, 0, 0, 1, (k & 1) ? ARGB(0,3,0) : ARGB(1,3,1));
    Gib *g = NULL;
    for (int i = 0; i < MAX_GIB; i++)
      if (g_gib[i].life && g_gib[i].x == x && g_gib[i].y == y && !g_gib[i].stains) g = &g_gib[i];
    if (!g) return;
    g->stains = GIB_CASH;
    g->size = (uint8_t)(2 + wr_rand_n(6));            // $10..$35
  }
}

// What a body is worth. Most people carry nothing; a few carry a lot.
void money_on_death(const Ent *e) {
  uint32_t r = wr_rand_n(100);
  int n = (e->type == ENT_COP) ? 2 + (int)wr_rand_n(2)
        : (r < 50) ? 0 : (r < 85) ? 1 + (int)wr_rand_n(2) : 4 + (int)wr_rand_n(3);
  money_burst(e->x, e->y, n);
}

// A bill has landed. A free slot takes it; failing that it joins the
// nearest pile of cash, and failing that it takes the place of a bat or a
// knife. Money is never lost to a full pool.
void pickup_cash(int32_t x, int32_t y, uint8_t value) {
  int slot = -1, pile = -1, junk = -1; int32_t pd = 2 * TILE;
  for (int i = 0; i < MAX_PICKUP; i++) {
    Pickup *p = &g_pickup[i];
    if (!p->alive) { if (slot < 0) slot = i; continue; }
    if (p->weapon == PICKUP_CASH) {
      int32_t d = wr_dist(p->x - x, p->y - y);
      if (d < pd) { pd = d; pile = i; }
    } else if (p->weapon == W_BAT) junk = i;
  }
  if (slot < 0) slot = junk;                     // a bat's slot before a distant pile
  if (slot < 0 && pile >= 0) {
    g_pickup[pile].world = (uint8_t)wr_clamp(g_pickup[pile].world + value, 0, 255);
    return;
  }
  if (slot < 0) return;
  g_pickup[slot].x = x; g_pickup[slot].y = y;
  g_pickup[slot].weapon = PICKUP_CASH;
  g_pickup[slot].world = value;
  g_pickup[slot].alive = 1;
}

// ---------------------------------------------------------------------------
// The room
// ---------------------------------------------------------------------------

void inside_enter(uint8_t kind, int32_t dx, int32_t dy, int32_t nx, int32_t ny) {
  uint8_t venue = (uint8_t)(((uint32_t)(dx >> 6) * 7 + (uint32_t)(dy >> 6) * 13) & 7);

  // Bring the room's code in before anything else: if it will not load there
  // is no room to walk into, and the door does nothing.
  // The same door means different things on different islands: on the
  // coast the gun store's lot is the seafood restaurant.
  uint8_t dom = kind == IN_CLUB ? DOM_CLUB : kind == IN_AIRPORT ? DOM_AIRPORT
              : kind == IN_HOME ? DOM_HOME
              : island_at(g_p.x, g_p.y) == ISL_COAST ? DOM_DINER : DOM_STORE;
  if (!domain_use(dom)) return;

  g_in.kind = kind; g_in.venue = venue;
  g_in.door_x = dx; g_in.door_y = dy; g_in.nx = (int8_t)nx; g_in.ny = (int8_t)ny;
  g_in.meter = 0; g_in.level = 0; g_in.stay = 0; g_in.angry = 0; g_in.hostile = 0;
  g_in.alarm = 0; g_in.pinned = 0; g_in.unlocked = 0;
  g_in.credit = g_in.spent[venue];
  g_in.stage = 0; g_in.ticket = 0; g_in.wait = 0;
  g_p.inside = kind;
  g_p.in_car = 0;
  city_set_interior(true);
  ent_reset();
  memset(g_pickup, 0, sizeof(g_pickup));
  domain_call(DOP_ENTER, 0);          // the domain furnishes its own room
  g_p.x = ROOM_OX + g_in.w / 2;
  g_p.y = ROOM_OY + g_in.h - TILE / 2;
  g_p.ang = 49152;
  g_p.spd = 0; g_p.strafe = 0; g_p.turn = 0;
  g_p.inv[g_p.weapon % W_COUNT] = g_p.ammo;
  g_p.weapon = W_CASH; g_p.ammo = 1;
  audio_sfx(SFX_DOOR);
  if (kind == IN_GUNSHOP && dom == DOM_STORE) {
    game_caption(CAP_PRICES, 120);
    domain_call(DOP_MONEY, 0);        // put anything already bought on the counter
    g_in.credit = g_in.spent[venue];
  g_in.stage = 0; g_in.ticket = 0; g_in.wait = 0;  // (that call counted a bill; undo it)
  }
  if (kind == IN_CLUB && (g_in.banned & (1 << venue))) g_in.angry = 1;
}

// Leaving is deferred whenever a domain asks: see after_call() in
// wr_domain.c. The kernel's own callers (the door, death) go straight in.
void inside_leave(void) { g_in.leave = 1; }
void inside_exit(void) { inside_exit_now(); }
void inside_exit_now(void) {
  if (!g_p.inside) return;
  g_p.inside = 0;
  g_in.kind = IN_NONE;
  g_in.pinned = 0;
  // The room's code goes with the room. Everything it owned - its people,
  // its fittings - was destroyed by ent_reset() below, so nothing is left
  // holding a pointer into an arena that is about to be reused.
  city_set_interior(false);
  domain_scenery();                  // outdoors again: the island's own scenery
  g_p.x = g_in.door_x + g_in.nx * 90;
  g_p.y = g_in.door_y + g_in.ny * 90;
  g_p.spd = 0; g_p.strafe = 0;
  ent_reset();
  city_refresh(g_p.x, g_p.y);
  // What a room asked for on the way out, now that it is safe to do.
  if (g_in.after == 1) {             // landed: onto the nearest road
    int32_t x = g_p.x, y = g_p.y; uint16_t a;
    road_snap(&x, &y, &a);
    g_p.x = x; g_p.y = y; g_p.ang = a;
  } else if (g_in.after == 2) {      // hijacked: the air domain makes the airliner
    domain_air_spawn_plane();
  }
  g_in.after = 0;
  props_refresh();
  pickups_refresh();
  if (g_p.weapon == W_CASH) weapon_next();
  audio_sfx(SFX_DOOR);
  // An alarm, or a body on the floor, means the police are already on
  // their way to the door you just came out of.
  if (g_in.alarm) {
    if (g_p.heat < 250) g_p.heat = 250;
    if (g_p.wanted < 3) g_p.wanted = 3;
    g_p.evade = 0;
  }
}

// ---------------------------------------------------------------------------
// House rules
// ---------------------------------------------------------------------------

// A bill has landed. In the store it is credit against the cabinets; in
// the club it is the show, if it went anywhere near the stage.
void inside_money(int32_t x, int32_t y) {
  domain_call(DOP_MONEY, ((y / 4) << 16) | ((x / 4) & 0xFFFF));
}

// A shot fired, a swing taken, a body on the floor, or a boot on the stage.
void inside_violence(bool killed) {
  if (!g_p.inside) return;
  domain_call(DOP_VIOLENCE, killed ? 1 : 0);
  // Consequences that outlive the room stay in the kernel: a domain cannot
  // reach the wanted level, and should not - the police are not part of the
  // furniture. An alarm, or a body, means they are called and are waiting at
  // the door when you come out.
  if (killed) g_in.alarm = 1;
  if (g_in.alarm) {
    if (g_p.heat < 250) g_p.heat = 250;
    if (g_p.wanted < 3) g_p.wanted = 3;
    g_p.evade = 0;
  }
}

void inside_update(void) {
  if (!g_p.inside) return;
  domain_call(DOP_UPDATE, 0);
}

bool inside_person(Ent *e) {
  if (!g_p.inside || e->state == ST_DEAD) return false;
  return domain_call(DOP_PERSON, (int)(e - g_ent)) != 0;
}

// The money gun: ten dollars a throw, thrown, not fired.
void money_throw(void) { domain_call(DOP_THROW, 0); }

// The taxi lives in the THREAT arena - the second bench - because a fare
// needs zero stars and the helicopter needs four: they can never both be
// wanted, which is the whole trick. The state stays here; the behaviour is
// the domain's.
Taxi g_taxi;
void taxi_update(void) { domain2_call(DOP_UPDATE, 0); }
