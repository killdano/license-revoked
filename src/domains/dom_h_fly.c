#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"
static int dom_dispatch(int op, int arg);
#include "wr_shim.h"


// ---------------------------------------------------------------------------
// Hands on the cyclic. Resident while you fly: the aircraft's steering and
// throttle (the same movement source, with the helicopter flag), the
// climb and the landing, the chain gun and the rockets. Air support's own
// behaviour - dispatch, the door gunner, SWAT on the ropes - stays on the
// THREAT bench; this is just the controls of the one you took.
// ---------------------------------------------------------------------------
static bool flying_now(void) { return g_p.in_car && g_ent[g_p.in_car - 1].type == ENT_HELI; }

#define DRIVING 1
#define HELI 1
#include "movement.inc"

// The aircraft you are in, drawn here: the police helicopter's model (the
// air domain keeps its own copy for the ones it flies) and the airliner's.
#include "aircraft.inc"

// The hijacked airliner, at the player's position, with the player in it.
static void spawn_plane(void) {
  int s = free_slot_evict();
  if (s < 0) return;
  Ent *p = &g_ent[s];
  memset(p, 0, sizeof(Ent));
  p->type = ENT_HELI; p->sub = 1; p->hp = 200; p->state = ST_DRIVE;
  p->x = g_p.x; p->y = g_p.y; p->ang = g_p.ang; p->z = 30; p->occupied = 1;
  g_p.in_car = (uint8_t)(s + 1); g_p.heli_wep = 0; g_p.spd = 20;
  audio_sfx(SFX_ROCKET);
}


static void draw_mine(const Ent *e) {
  int32_t z = e->z;
  if (e->sub == 1) {
    if (z > 0) gfx_ground_quad(e->x, e->y, 120 - z / 4, ARGB(0,0,1));
    gfx_parts(e->x, e->y, e->ang, z, 0, PLANEP, (int)(sizeof PLANEP / sizeof PLANEP[0]), 2, NULL);
    return;
  }
  gfx_parts(e->x, e->y, e->ang, z, 0, HELIP, (int)(sizeof HELIP / sizeof HELIP[0]), 2, NULL);
  uint8_t bl = (g_frame & 8) ? ARGB(3,0,0) : ARGB(0,0,3);
  gfx_part(e->x, e->y, e->ang, -30, 0, z + 58, 6, 6, 6, 0, bl, bl, bl);
  gfx_part(e->x, e->y, (uint16_t)(e->phase * 300), 0, 0, z + 52, 60, 3, 2, 0, ARGB(1,1,1), ARGB(1,1,1), ARGB(0,0,0));
}

static void heli_fire(void) {
  if (!flying_now()) return;
  Ent *h = &g_ent[g_p.in_car - 1];
  if (g_p.attack_cd) return;
  if (g_p.heli_wep == 2) { g_p.heli_wep = 3; g_p.attack_cd = 10; audio_sfx(SFX_DOOR); return; }   // LAND, fired: coming down
  if (g_p.heli_wep == 3) return;
  if (g_p.heli_wep == 0) {
    g_p.attack_cd = 4;
    audio_sfx(SFX_PISTOL);
    fire_bullet_kind((uint16_t)(g_p.ang + (uint16_t)(wr_rand_n(1400) - 700)), 16, B_SLUG);
  } else if (h->wep) {
    g_p.attack_cd = 20;
    h->wep--;
    audio_sfx(SFX_ROCKET);
    fire_bullet_kind(g_p.ang, 90, B_ROCKET);
  } else return;
  // Whatever just left the aircraft comes down from above the rooftops.
  for (int i = 0; i < MAX_BULLET; i++)
    if (g_bullet[i].life && g_bullet[i].x == g_p.x + fmul(60, fcos(g_p.ang)) &&
        g_bullet[i].y == g_p.y + fmul(60, fsin(g_p.ang))) g_bullet[i].from_cop |= 2;
}


static void move_heli(void) {
  if (!flying_now()) return;              // the frame after you get out, before the bench swaps
  Ent *h = &g_ent[g_p.in_car - 1];
  // Selecting LAND does nothing by itself: you select it, then FIRE it,
  // and the aircraft comes down wherever it is (heli_wep 3 is "landing").
  // On the ground it is an aircraft on the ground - it does not drive.
  // Select anything else and it lifts off again.
  bool landing = g_p.heli_wep == 3;
  if (landing) { if (h->z > 0) h->z = (int16_t)(h->z > 5 ? h->z - 5 : 0); }
  else if (h->z < HELI_ALT) h->z = (int16_t)(h->z + 8);
  h->phase += (uint8_t)(landing && h->z == 0 ? 8 : 40);      // rotor winding down on the ground
  if (landing && h->z == 0) {
    // An airliner down on the secret island: the passengers get off and
    // stay. Once per plane (its door remembers).
    if (h->sub == 1 && !h->aux && island_at(g_p.x, g_p.y) == ISL_SECRET) {
      h->aux = 1;
      g_p.residents = (uint8_t)(g_p.residents + 6 > 250 ? 250 : g_p.residents + 6);
      game_caption(CAP_STAY, 110); audio_sfx(SFX_DOOR);
    }
    g_p.spd = 0;
    h->x = g_p.x; h->y = g_p.y; h->ang = g_p.ang; h->spd = 0;
    return;
  }
  // An airliner cannot hover: in the air it keeps at least a cruising
  // speed, and it climbs higher and turns slower than the helicopter.
  if (h->sub == 1 && h->z > 0) {
    if (g_p.spd < 16) g_p.spd = 16;
    if (h->z < HELI_ALT * 2 && !landing) h->z = (int16_t)(h->z + 4);
  }
  if (g_p.spd) {
    int32_t nx = g_p.x + fmul(g_p.spd, fcos(g_p.ang));
    int32_t ny = g_p.y + fmul(g_p.spd, fsin(g_p.ang));
    if (h->z > 40 || !city_blocked(nx, ny, true)) { g_p.x = nx; g_p.y = ny; }
    else g_p.spd = 0;
  }
  h->x = g_p.x; h->y = g_p.y; h->ang = g_p.ang; h->spd = g_p.spd;
}


static int dom_dispatch(int op, int arg) {
  switch (op) {
    case DOP_MOVE:  move_heli(); return 1;
    case DOP_TURN:  if (flying_now()) update_turn_and_speed(); return 1;
    case DOP_FIRE:  heli_fire(); return 1;
    case DOP_SPAWN: spawn_plane(); return 1;
    case DOP_DRAW_ENT: draw_mine(&g_ent[arg]); return 1;
    default: return 0;
  }
}

DOMAIN_ENTRY(dom_h_fly_entry)
