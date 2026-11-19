#pragma once
// wr.h and wr_gfx.h must be included first: this header points at their types.
#include "wr_gfx.h"
// ---------------------------------------------------------------------------
// Domains: code that lives in a resource, not in the app image.
//
// The app is capped at 65,535 bytes of text + data + bss, and it is 95% code,
// so that cap is really a cap on features. A domain is a piece of the game -
// the club, the store, later the boats and an airport - compiled on its own,
// shipped as a resource, and loaded into a shared arena when it is needed.
// Ten domains then cost one arena instead of ten. Proven on hardware first
// (Exec42, Ovl2): the watch executes code from RAM, that code can call back
// into the app, and a resource-borne blob relocates and runs correctly.
//
// A domain may not name a host symbol. It has no idea where the host's
// globals live, and an absolute address to one would be a relocation the
// loader cannot resolve - mkdomain.py fails the build rather than shipping
// something that faults. So everything arrives through HostCtx, and
// wr_shim.h maps the ordinary names (g_p, fsin, audio_sfx...) onto it. That
// is why the domain sources read like the rest of the game.
// ---------------------------------------------------------------------------

// Which slice of the world a domain is. One is resident at a time.
// PLACE domains share arena 0; THREAT domains share arena 1. You can be at
// five stars anywhere, which is why the police escalation cannot share a
// bench with the scenery.
enum { DOM_NONE = 0, DOM_CITY, DOM_GHETTO, DOM_COAST, DOM_CLUB, DOM_STORE, DOM_DINER,
       DOM_AIRPORT, DOM_AIRSIDE, DOM_MENU, DOM_AIR, DOM_TAXI, DOM_ARREST, DOM_GUARD, DOM_SECRET, DOM_HOME, DOM_ISLAND,
       DOM_H_MELEE, DOM_H_GUNS, DOM_H_FLAME, DOM_H_ROCKET, DOM_H_CAR, DOM_H_BOARD, DOM_H_GRENADE, DOM_H_FLY, DOM_H_DEAD, DOM_COUNT };
#define DOM_FIRST_THREAT DOM_AIR
#define DOM_FIRST_HANDS  DOM_H_MELEE

// What a domain is being asked to do.
enum {
  DOP_ENTER = 0,   // build the room: lots, props, people
  DOP_UPDATE,      // one frame of the house's own rules
  DOP_PERSON,      // arg = entity index; returns 1 if the domain moved him
  DOP_MONEY,       // a bill landed: arg = packed x/y in tiles
  DOP_VIOLENCE,    // arg != 0 if somebody died
  DOP_DRAW_LOT,    // arg = lot index; draw that building
  DOP_PROP,        // scenery: what stands on this tile? arg = cell<<8 | hash%100; 0 = the usual
  DOP_DRAW_PROP,   // scenery: draw prop arg (a kind >= PROP_DOMAIN0)
  DOP_THROW,       // interiors: throw a bill (the money gun)
  DOP_ACTION,      // interiors: the action button; returns 1 if the room used it
  DOP_DRAW_MENU,   // menu: draw, arg = the GContext
  DOP_MENU_NAV,    // menu: up/down, arg = dir
  DOP_MENU_SELECT, // menu: SELECT
  DOP_ATTACK,      // hands: the trigger, whatever you are holding
  DOP_MOVE,        // hands: one frame of the player's movement (foot or car)
  DOP_TURN,        // hands / air: steering and throttle
  DOP_VEHICLE,     // hands: the door - board the nearest vehicle, or get out
  DOP_SPAWN,       // scenery: a pedestrian was just made; arg = index. May change who he is.
  DOP_DRIVE,       // scenery: the player's car is about to move; arg = packed dx,dy.
                   // Returns 1 if the domain moved him itself (the bridge jump).
                   // THREAT: the player is flying; move the aircraft.
  DOP_WANT,        // THREAT: how many of this unit dispatch wants right now
  DOP_ENT,         // THREAT: one frame of entity arg
  DOP_DRAW_ENT,    // THREAT: draw entity arg
  DOP_FIRE,        // THREAT: the player pulled the trigger in the aircraft
  DOP_ARREST,      // THREAT: one frame of the arrest (the grip, the cuffs)
  DOP_AMB,         // THREAT (0-3 stars): one frame of ambulance arg
  DOP_UNIT,        // THREAT: a police unit was just spawned; arg = index. The guard makes it its own.
  DOP_DRAW_HUD,    // PLACE (rooms) and THREAT: draw your own HUD marks; arg = the GContext
  DOP_DRAW_GROUND, // scenery: ground paint, before the lots (bay markings)
  DOP_BIKE_HIT,    // scenery: a motorcycle crashed; arg = closing<<16 | other<<8 | bike
  DOP_DRAW_AMB,    // THREAT (0-3 stars): draw ambulance arg
};

// The whole world, as far as a domain is concerned. Pointers to the tables
// it may touch and the host functions it may call. Adding to the end of this
// struct is safe; reordering it is not - the blobs are compiled against it.
// wr.h must be included first: this struct points at its types.
typedef struct {
  // --- state ---
  Player *p;
  Inside *in;
  Ent    *ent;
  Prop   *prop;
  Lot    *lot;
  Gib    *gib;
  Pickup *pickup;
  int    *nlot;
  uint32_t *frame;

  // --- maths (they read the host's sine table, so they cannot be inline) ---
  int32_t  (*fsin)(uint16_t a);
  int32_t  (*fcos)(uint16_t a);
  int32_t  (*fmul)(int32_t a, int32_t b);
  int32_t  (*dist)(int32_t dx, int32_t dy);
  uint16_t (*angle_to)(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
  uint32_t (*rand_n)(uint32_t n);

  // --- the world and the game ---
  uint8_t (*cell_at_point)(int32_t x, int32_t y);
  void    (*audio_sfx)(uint8_t id);
  void    (*caption)(uint8_t cap, uint8_t frames);
  void    (*pickup_drop)(int32_t x, int32_t y, uint8_t weapon);
  void    (*gib_debris)(int32_t x, int32_t y, int32_t z,
                        int32_t vx, int32_t vy, int n, uint8_t col);
  void    (*money_burst)(int32_t x, int32_t y, int n);
  void    (*player_absorb)(int32_t dmg);
  const uint8_t *prop_hp;                   // PROP_HP[], indexed by kind

  // --- drawing, for scenery domains ---
  void (*part)(int32_t cx, int32_t cy, uint16_t ang, int32_t ox, int32_t oy,
               int32_t oz, int32_t hl, int32_t hw, int32_t h, uint16_t dang,
               uint8_t top, uint8_t bright, uint8_t dark);
  void (*taper)(int32_t cx, int32_t cy, uint16_t ang, int32_t ox, int32_t oy,
                int32_t oz, int32_t hl, int32_t hw, int32_t h, int32_t tl,
                int32_t tw, int32_t ts, uint16_t dang,
                uint8_t top, uint8_t bright, uint8_t dark);
  void (*world_poly)(const int32_t (*pts)[3], int n, uint8_t col);
  int  (*clip_world_poly)(const int32_t (*w)[3], int n, Proj *out);
  void (*fill_poly)(const Proj *p, int n, uint8_t col);
  bool (*all_ok)(const Proj *p, int n);
  int32_t (*area2)(const Proj *p, int n);
  Proj (*project)(int32_t x, int32_t y, int32_t z);
  void (*parts)(int32_t x, int32_t y, uint16_t ang, int32_t z, int32_t lean,
                const PartRec *p, int n, int scale, const uint8_t *pal);
  void (*ground_band)(int32_t cx, int32_t cy, uint16_t ang,
                      int32_t half_len, int32_t half_wid, uint8_t col);
  uint8_t (*shade)(uint16_t normal, uint8_t bright, uint8_t dark);
  int32_t (*depth_at)(int32_t wx, int32_t wy);
  void (*set_stipple)(bool on);
  int32_t (*sqrt_)(int32_t v);
  int32_t (*muldiv)(int32_t a, int32_t b, int32_t c);
  int32_t (*clamp)(int32_t v, int32_t lo, int32_t hi);
  int32_t (*abs_)(int32_t v);
  // Where the eye sits on the ground plane; the walls cull against it.
  const int32_t *eye_x, *eye_y;
  // The player's screen position and depth, for the occlusion test.
  const int32_t *psx, *psy, *pdepth;
  int     (*bridge_find)(int32_t x, int32_t y, int32_t *lat, int32_t *t);
  void    (*fire)(int32_t x, int32_t y, uint16_t aim, uint8_t wep, uint8_t flags);
  int     (*nearest_of)(uint8_t type, int32_t x, int32_t y, int32_t range, uint8_t aux);
  int     (*island_at)(int32_t x, int32_t y);
  // --- for the police escalation ---
  void    (*cop_shoot)(int32_t x, int32_t y, uint16_t aim, int32_t spd, uint8_t life,
                       uint8_t dmg, uint8_t flags, uint8_t sfx);
  int     (*free_slot)(void);
  int     (*free_slot_evict)(void);
  void    (*steer)(Ent *e, uint16_t target, int32_t rate);
  void    (*vehicle_destroy)(int idx, bool explode);
  void    (*money_on_death)(const Ent *e);
  void    (*gib_burst)(int32_t x, int32_t y, int32_t z, int32_t vx, int32_t vy, int n);
  void    (*add_heat)(uint16_t amount);
  void    (*vibe)(uint8_t strength);
  bool    (*city_blocked)(int32_t x, int32_t y, bool car);
  void    (*ground_quad)(int32_t x, int32_t y, int32_t half, uint8_t col);
  const int32_t *seen_x, *seen_y;
  uint8_t *heli_up;
  Bullet  *bullet;                              // the player's rounds
  void    (*road_snap)(int32_t *x, int32_t *y, uint16_t *ang);
  uint32_t (*rand32)(void);
  const Island *islands;
  // --- the SDK, for the menu ---
  Menu    *menu;
  Taxi    *taxi;
  void    (*set_fill)(GContext *ctx, GColor c);
  void    (*set_stroke)(GContext *ctx, GColor c);
  void    (*fill_rect)(GContext *ctx, GRect r, uint16_t radius, GCornerMask m);
  void    (*round_rect)(GContext *ctx, GRect r, uint16_t radius);
  void    (*fill_circle)(GContext *ctx, GPoint p, uint16_t radius);
  void    (*hud_text)(GContext *ctx, const char *s, const char *font, int x, int y, int w, int h, GColor col);
  void    (*hud_marker)(GContext *ctx, int32_t x, int32_t y, int32_t z, GColor col);
  int     (*utoa)(char *dst, int cap, uint32_t v);
  void    (*audio_set_volume)(uint8_t v);
  void    (*audio_set_enabled)(bool on);
  void    (*draw_line)(GContext *ctx, GPoint a, GPoint b);
  const Venue *venue; const int *nvenue;
  const Bridge *bridges;
  bool    (*lot_is_airport)(const Lot *L);
  uint8_t *park_col, *lot_col;
  void    (*car_half_dims)(const Ent *e, int32_t *hl, int32_t *hw);
  int     (*garage_slots)(const Lot *L, int32_t (*out)[3], int max);
  void    (*crash_damage)(int idx, uint8_t dmg);
  uint8_t *blame;
  void    (*city_set_interior)(bool on);
  void    (*ent_reset)(void);
  void    (*props_refresh)(void);
  void    (*pickups_refresh)(void);
  void    (*city_refresh)(int32_t px, int32_t py);
  uint8_t (*cell_at)(int32_t tx, int32_t ty);
  void    (*drive_along)(Ent *e, bool seek, int32_t tx, int32_t ty, int32_t lane, int32_t cruise, bool police);
  void    (*blood_add)(int32_t x, int32_t y, uint8_t size);
  void    (*ent_spawn)(uint8_t type);
  void    (*model_car)(const CarDraw *c, int32_t dist);
  void    (*fire_bullet_kind)(uint16_t ang, uint8_t dmg, uint8_t kind);
  void    (*fire_bullet)(uint16_t ang, uint8_t dmg);
  void    (*damage_entity)(Ent *e, uint8_t dmg);
  void    (*prop_hit)(int i, int32_t vx, int32_t vy, int32_t force);
  // --- for movement ---
  void    (*kill_entity)(Ent *e);
  uint8_t (*cell_at_world)(int32_t x, int32_t y);
  int     (*lot_at)(int32_t x, int32_t y);
  int     (*ent_nearest)(uint8_t type, int32_t range);
  bool    (*water_blocked)(int32_t x, int32_t y);
  int     (*place_call)(int op, int arg);      // the scenery's hooks (the jump)
  int     (*threat_call)(int op, int arg);     // the air domain's (the kit)
  bool    (*bridge_in_gap)(int b, int32_t t);
  void    (*shop_door)(const Lot *L, int32_t *dx, int32_t *dy,
                       int32_t *nx, int32_t *ny);
  void    (*inside_exit)(void);             // ASKS to leave; the kernel does it after the call
  void    (*violence)(bool killed);         // a domain reporting its own trouble
  void   *(*mem_set)(void *d, int c, size_t n);
} HostCtx;

typedef int (*domain_fn)(const HostCtx *, int op, int arg);

// --- host side -------------------------------------------------------------
// Make `which` resident, loading it from its resource if it is not already.
// Returns false if it could not be loaded, in which case the caller must not
// enter the room. Loading is a few milliseconds; the caller draws a banner.
bool domain_use(uint8_t which);
void domain_drop(void);                     // nothing resident afterwards
uint8_t domain_current(void);
uint8_t domain2_current(void);
// Call the resident domain. Returns 0 when nothing is resident, so every
// call site degrades to "the room does nothing" rather than crashing.
int domain_call(int op, int arg);
// The second bench: the police escalation. Same loader, separate arena.
int  domain2_call(int op, int arg);
void domain_threat(void);                   // resident when the heat calls for it
void domain_air_spawn_plane(void);
// The third bench: what your hands are doing. Whatever weapon you hold (or,
// later, whatever you are driving). Resident tenant follows g_p.weapon.
int  domain3_call(int op, int arg);
void domain_hands(void);
// Board the nearest vehicle: the boarding tenant is swapped in for the call.
void domain_board(void);
// The scenery domain for wherever the player is standing. Loads it if the
// island changed. Safe to call every frame.
void domain_scenery(void);
