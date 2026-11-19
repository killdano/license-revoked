#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"
static int dom_dispatch(int op, int arg);
#include "wr_shim.h"
#if WR_MONOLITH
void kill_entity(Ent *e);
#endif

// ---------------------------------------------------------------------------
// The car door, from outside. Loaded by the kernel for the hold that boards
// a vehicle and swapped straight out again - for the car tenant once you
// are in, or back to your weapon if there was nothing to board. Its own
// tenant so the four weapon tenants do not each carry it.
// ---------------------------------------------------------------------------
// Taking an aircraft: a vest, and one of the good guns. The airliner comes
// with neither - you took it with your fists.
// Taking the aircraft: a vest, and one of the good guns. The airliner
// comes with neither - you took it with your fists.
static void kit(int idx) {
  Ent *h = &g_ent[idx];
  game_caption(CAP_HELI, 90);
  if (h->sub == 1) return;
  g_p.armor = ARMOR_MAX;
  static const uint8_t KIT[3] = { W_MG, W_FLAME, W_ROCKET };
  uint8_t w = KIT[wr_rand_n(3)];
  g_p.inv[g_p.weapon % W_COUNT] = g_p.ammo;
  g_p.weapon = w; g_p.ammo = (uint8_t)(g_p.inv[w] + (w == W_ROCKET ? 5 : w == W_MG ? 60 : 40));
}


#define DRIVING 0
#define BOARD_ONLY 1
#include "movement.inc"

static int dom_dispatch(int op, int arg) {
  (void)arg;
  if (op == DOP_VEHICLE) { enter_or_exit_car(); return 1; }
  return 0;
}

DOMAIN_ENTRY(dom_h_board_entry)
