#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"
static int dom_dispatch(int op, int arg);
#include "wr_shim.h"

// ---------------------------------------------------------------------------
// The secret island's residents. There are no police here, so the THREAT
// bench is theirs: the people you have brought - dancers carried out of
// the club, the passengers of an airliner you landed here - living at a
// resort, on the lawn and the beach. The count is g_p.residents, kept by
// the kernel and persisted; this tenant puts them on the ground when you
// arrive and takes delivery of whoever you are carrying.
// ---------------------------------------------------------------------------

static void populate(void) {
  // Top up to the count (at most ten shown): people arrive while you are
  // here - off a plane, out of your arms - and the lawn should show it.
  int have = 0; for (int i = 0; i < MAX_ENT; i++) if (g_ent[i].type == ENT_PED && g_ent[i].state != ST_DEAD) have++;
  int n = (g_p.residents > 10 ? 10 : g_p.residents) - have;
  int32_t hx = ISLAND_SX * TILE, hy = ISLAND_SY * TILE - 3 * TILE;     // the house's front
  for (int i = 0; i < n; i++) {
    int s = free_slot(); if (s < 0) return;
    Ent *e = &g_ent[s]; memset(e, 0, sizeof(Ent));
    e->type = ENT_PED; e->sub = (i & 1) ? PED_WOMAN : PED_MAN; e->hp = 100; e->state = ST_WALK;
    e->x = hx + (int32_t)wr_rand_n(6 * TILE) - 3 * TILE; e->y = hy + 2 * TILE + (int32_t)wr_rand_n(5 * TILE);
    e->ang = (uint16_t)wr_rand_n(65536); e->timer = (uint8_t)wr_rand_n(200);
  }
}

// Carrying someone home: at the house's door she is safe, and one more
// person lives here.
static void deliver(void) {
  if (!g_p.carry || g_p.in_car) return;
  int32_t dx = g_p.x - ISLAND_SX * TILE, dy = g_p.y - (ISLAND_SY * TILE - 3 * TILE);
  if (wr_dist(dx, dy) < 2 * TILE + TILE / 2) {   // the lawn in front of the house
    g_p.carry = 0;
    if (g_p.residents < 250) g_p.residents++;
    game_caption(CAP_SAFE, 100); audio_sfx(SFX_PICKUP);
  }
}

static int dom_dispatch(int op, int arg) {
  (void)arg;
  switch (op) {
    case DOP_ENTER:  populate(); return 1;
    case DOP_UPDATE: deliver(); if ((g_frame & 63) == 0) populate(); return 1;
    case DOP_WANT:   return 0;
    default: return 0;
  }
}

DOMAIN_ENTRY(dom_island_entry)
