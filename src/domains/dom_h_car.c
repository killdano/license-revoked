#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"
static int dom_dispatch(int op, int arg);
#include "wr_shim.h"

#if WR_MONOLITH
void kill_entity(Ent *e);
#endif

// ---------------------------------------------------------------------------
// Hands on the wheel. Resident while you are in a car: driving, crashing,
// the surfaces, the collapsed bridge (through the scenery's hook), and
// getting out. The same movement source as the weapon tenants, compiled
// with DRIVING fixed at 1 so only the driving half is here.
// ---------------------------------------------------------------------------
#define DRIVING 1
static int drive(int32_t dx, int32_t dy);
#include "movement.inc"



// The bridge jump. The ghetto bridge is broken at its far end; reach the
// lip fast enough and you fly the gap, too slow and you drown. Driving
// code, so it lives with the driving.
// --- the collapsed bridge ------------------------------------------------
#define JUMP_SPD    22    // reach the lip at this or better and you clear the gap
#define JUMP_FRAMES 36
#define JUMP_RISE   140

// Into the water. The car is a wreck where it went in - not burning, just
// gone - and you are swimming beside it. Wrecks accumulate.
static void car_drowns(int idx) {
  // A wreck in the water, and you swimming beside it: the kernel's
  // write-off (which ejects you) plus the swim.
  g_p.x += fmul(120, fcos(g_p.ang)); g_p.y += fmul(120, fsin(g_p.ang));
  g_ent[idx].x = g_p.x; g_ent[idx].y = g_p.y; g_ent[idx].fire = 0; g_ent[idx].z = 0;
  vehicle_destroy(idx, false);
  g_p.jump = 0; g_p.swim = 1;
}

// The player's car is about to move by (dx,dy). Returns 1 if this domain
// took the move - the car is in the air, or has just gone into the sea.
static int drive(int32_t dx, int32_t dy) {
  Ent *car = &g_ent[g_p.in_car - 1];
  int32_t nx = g_p.x + dx, ny = g_p.y + dy;
  if (g_p.jump) {
    g_p.x = nx; g_p.y = ny;
    g_p.jump--;
    int32_t t = JUMP_FRAMES - g_p.jump;
    car->z = (int16_t)wr_muldiv(4 * JUMP_RISE * t, JUMP_FRAMES - t, JUMP_FRAMES * JUMP_FRAMES);
    car->x = g_p.x; car->y = g_p.y; car->ang = g_p.ang; car->spd = g_p.spd;
    if (g_p.jump) return 1;
    car->z = 0;
    if (cell_at_point(g_p.x, g_p.y) == CELL_WATER) { car_drowns(g_p.in_car - 1); return 1; }
    audio_sfx(SFX_CRASH);
    g_p.spd = (int16_t)((g_p.spd * 3) / 4);
    return 1;
  }
  int32_t bt, ht; int bi = bridge_find(nx, ny, NULL, &bt);
  if (bi >= 0 && bridge_in_gap(bi, bt)) {
    int hb = bridge_find(g_p.x, g_p.y, NULL, &ht);
    if (hb >= 0 && !bridge_in_gap(hb, ht)) {
      if (wr_abs(g_p.spd) >= JUMP_SPD) { g_p.jump = JUMP_FRAMES; audio_sfx(SFX_SMASH); return 1; }
      car_drowns(g_p.in_car - 1); return 1;
    }
  }
  return 0;
}

static int dom_dispatch(int op, int arg) {
  (void)arg;
  switch (op) {
    case DOP_MOVE:    move_player(); return 1;
    case DOP_TURN:    update_turn_and_speed(); return 1;
    case DOP_VEHICLE: enter_or_exit_car(); return 1;
    default: return 0;
  }
}

DOMAIN_ENTRY(dom_h_car_entry)
