#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"
static int dom_dispatch(int op, int arg);
#include "wr_shim.h"
#include "wr_tune.h"

// ---------------------------------------------------------------------------
// The arrest. Third tenant of the THREAT bench, resident at one to three
// stars: an officer on foot who gets within reach while you are slow
// builds a grip, hauls you out of a vehicle first, then cuffs you. At zero
// stars there is no one trying; at four and up the air has the bench and
// they have stopped trying to take you alive.
// ---------------------------------------------------------------------------
static bool on_heli(void) { return g_p.in_car && g_ent[g_p.in_car - 1].type == ENT_HELI; }
static bool airborne(void) { return on_heli() && g_ent[g_p.in_car - 1].z > 0; }

static int nearest_officer(int32_t range) {
  int best = -1; int32_t bd = range;
  for (int i = 0; i < MAX_ENT; i++) {
    if (g_ent[i].type != ENT_COP || g_ent[i].state == ST_DEAD) continue;
    int32_t d = wr_dist(g_ent[i].x - g_p.x, g_ent[i].y - g_p.y);
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}

static void update_arrest(void) {
  if (!g_p.wanted) { g_p.arrest = 0; return; }

  // Nobody on the ground reaches an aircraft. Fly, and the arrest is the
  // air's problem, not the officer's on the pavement below you.
  if (airborne() || on_heli()) { g_p.arrest = 0; return; }
  bool slow = wr_abs(g_p.spd) < (g_p.in_car ? 8 : 6);
  int cop = nearest_officer(g_p.in_car ? GRAB_CAR_RANGE : GRAB_RANGE);

  if (cop < 0 || !slow) {
    if (g_p.arrest) g_p.arrest--;      // wriggle free
    return;
  }

  // On foot you cannot be cuffed while you are still fighting fit: an
  // officer in reach hits you, and only once you are hurt does he get
  // hold of you. In a car it is different - pinned and barely moving, he
  // opens the door and hauls you out, and out of the car you are cuffed
  // on the spot. That is what the pin is for.
  // Nobody gets cuffed at full health: on foot, and in a car too - an
  // officer at the window of a car you just got into is not an arrest.
  // They have to hurt you (or the car) first.
  if (g_p.hp > 45) { g_p.arrest = 0; return; }
#if WR_GOD
  g_p.arrest = 0; return;
#endif
  g_p.arrest++;

  if (g_p.in_car && g_p.arrest > 24) {
    Ent *car = &g_ent[g_p.in_car - 1];
    car->occupied = 0;
    car->door = 64;
    car->spd = 0;
    if (car->state != ST_DEAD) car->state = ST_PARK;   // hauled out: it stays
    car->vx = (int16_t)(car->x >> 6); car->vy = (int16_t)(car->y >> 6);
    car->vz = 0; car->wep = TURN_NONE; car->spin = 0;
    g_p.in_car = 0;
    g_p.spd = 0;
    g_p.arrest = ARREST_FRAMES;          // out of the car is cuffed
    audio_sfx(SFX_DOOR);
  }

  if (g_p.arrest == 6) audio_sfx(SFX_ARREST);

  if (g_p.arrest >= ARREST_FRAMES) {
    g_p.busted = 1;
    g_p.dead = 1;
    g_p.cine = 0;
    g_p.hurt_flash = 6;
    audio_sfx(SFX_BUSTED);
  }
}

#include "ambulance.inc"


// The grip: a bar that fills as the officer gets hold of you.
static void draw_hud_t(GContext *ctx) {
  if (!g_p.arrest) return;
  int hw = SCR_W * 228 / SCR_H;
  int w = (g_p.arrest * 60) / ARREST_FRAMES; if (w > 60) w = 60;
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(hw / 2 - 31, 228 - 20, 62, 8), 2, GCornersAll);
  graphics_context_set_fill_color(ctx, GColorBlue);
  graphics_fill_rect(ctx, GRect(hw / 2 - 30, 228 - 19, w, 6), 2, GCornersAll);
}

static int dom_dispatch(int op, int arg) {
  (void)arg;
  if (op == DOP_ARREST) { update_arrest(); amb_dispatch(); return 1; }
  if (op == DOP_DRAW_HUD) { draw_hud_t((GContext *)(intptr_t)arg); return 1; }
  if (op == DOP_AMB) { update_amb(&g_ent[arg]); return 1; }
  if (op == DOP_DRAW_AMB) { draw_amb(&g_ent[arg]); return 1; }
  return 0;
}

DOMAIN_ENTRY(dom_arrest_entry)
