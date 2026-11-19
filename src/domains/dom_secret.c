#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"
static int dom_dispatch(int op, int arg);
#include "wr_shim.h"

// ---------------------------------------------------------------------------
// The secret island's scenery. Lawn to the water, a runway east-west, one
// house - the safe house, white with a red roof, a door on its south face -
// palms on the lawn, and a bonfire on the beach once there are enough
// people living here. No roads, no bridge, no police: you reach it by air
// or by water, and the THREAT bench holds the residents (dom_t_island).
// ---------------------------------------------------------------------------

enum { SP_PALM = PROP_DOMAIN0, SP_FIRE, SP_CHAIR };

static int island_prop(int cell, int k) {
  if (cell == CELL_PARK) return k < 30 ? SP_PALM : k < 40 ? SP_CHAIR : 0;
  if (cell == CELL_SAND) return k < 20 ? SP_CHAIR : k < 45 ? SP_PALM : 0;
  return 0;
}

static void draw_prop(const Prop *p) {
  switch (p->kind) {
    case SP_PALM: {
      gfx_part(p->x, p->y, 0, 0, 0, 0, 6, 6, 120, 0, ARGB(2,1,0), ARGB(2,1,0), ARGB(1,1,0));   // trunk
      for (int i = 0; i < 5; i++)                                                              // fronds
        gfx_part(p->x, p->y, (uint16_t)(i * 13107 + p->ang), 34, 0, 118, 40, 8, 4, 0, ARGB(0,3,0), ARGB(0,2,0), ARGB(0,1,0));
      break;
    }
    case SP_CHAIR:   // a sun lounger
      gfx_part(p->x, p->y, p->ang, 0, 0, 10, 40, 16, 6, 0, ARGB(3,3,3), ARGB(3,3,2), ARGB(2,2,2));
      gfx_part(p->x, p->y, p->ang, -28, 0, 16, 10, 16, 20, 0, ARGB(3,3,3), ARGB(3,3,2), ARGB(2,2,2));
      break;
    case SP_FIRE: {  // the bonfire: a ring of logs, three tongues
      gfx_part(p->x, p->y, 0, 0, 0, 0, 30, 30, 12, 0, ARGB(1,1,0), ARGB(1,0,0), ARGB(0,0,0));
      uint32_t f = (uint32_t)g_frame * 7u;
      for (int i = 0; i < 3; i++)
        gfx_part(p->x, p->y, (uint16_t)((f + i * 21845u) << 8), 6, 0, 12, 10 - i * 2, 10 - i * 2,
                 36 + (int32_t)((f >> i) % 19), 0, ARGB(3,3,0), ARGB(3,2,0), ARGB(3,1,0));
      break;
    }
  }
}

// The house: white walls, a red hipped roof, a dark door and two windows
// on the south face, a veranda. One lot, so one box plus trim.
static void draw_house(const Lot *L) {
  int32_t cx = (L->x0 + L->x1) / 2, cy = (L->y0 + L->y1) / 2;
  int32_t hl = (L->x1 - L->x0) / 2 - 20, hw = (L->y1 - L->y0) / 2 - 20;
  gfx_part(cx, cy, 0, 0, 0, 0, hl, hw, 120, 0, ARGB(3,3,3), ARGB(3,3,3), ARGB(2,2,2));                  // walls
  gfx_part(cx, cy, 0, 0, 0, 120, hl + 14, hw + 14, 6, 0, ARGB(3,0,0), ARGB(3,0,0), ARGB(2,0,0));         // eaves
  gfx_part(cx, cy, 0, 0, 0, 126, hl - 20, hw - 20, 44, 0, ARGB(3,0,0), ARGB(2,0,0), ARGB(2,0,0));        // roof
  gfx_part(cx, cy + hw, 0, 0, 0, 0, 24, 6, 74, 0, ARGB(1,0,0), ARGB(1,0,0), ARGB(0,0,0));                // door
  gfx_part(cx - hl / 2, cy + hw, 0, 0, 0, 40, 22, 5, 30, 0, ARGB(2,3,3), ARGB(2,3,3), ARGB(1,2,3));      // windows
  gfx_part(cx + hl / 2, cy + hw, 0, 0, 0, 40, 22, 5, 30, 0, ARGB(2,3,3), ARGB(2,3,3), ARGB(1,2,3));
  const int32_t vq[4][3] = { { L->x0 - 10, L->y1, 3 }, { L->x1 + 10, L->y1, 3 }, { L->x1 + 10, L->y1 + 160, 3 }, { L->x0 - 10, L->y1 + 160, 3 } };
  gfx_world_poly(vq, 4, ARGB(2,2,1));                                                                     // the veranda
  // The runway, painted with the house: grey, with a dashed centreline and
  // threshold bars at both ends.
  int32_t ry = ISLAND_SY * TILE - 2 * TILE, rx0 = ISLAND_SX * TILE - 7 * TILE, rx1 = ISLAND_SX * TILE + 7 * TILE;
  const int32_t rq[4][3] = { { rx0, ry - 120, 1 }, { rx1, ry - 120, 1 }, { rx1, ry + 120, 1 }, { rx0, ry + 120, 1 } };
  gfx_world_poly(rq, 4, ARGB(1,1,1));
  for (int32_t x = rx0 + 200; x < rx1 - 200; x += 400) {
    const int32_t dq[4][3] = { { x, ry - 10, 2 }, { x + 200, ry - 10, 2 }, { x + 200, ry + 10, 2 }, { x, ry + 10, 2 } };
    gfx_world_poly(dq, 4, ARGB(3,3,3));
  }
  for (int k = 0; k < 2; k++) {
    int32_t x = k ? rx1 - 60 : rx0 + 20;
    const int32_t tq[4][3] = { { x, ry - 100, 2 }, { x + 40, ry - 100, 2 }, { x + 40, ry + 100, 2 }, { x, ry + 100, 2 } };
    gfx_world_poly(tq, 4, ARGB(3,3,3));
  }
}

#include "bikehit.inc"

static int dom_dispatch(int op, int arg) {
  switch (op) {
    case DOP_BIKE_HIT: bike_hit(arg & 0xFF, (arg >> 8) & 0xFF, (arg >> 16) & 0xFF); return 1;
    case DOP_ENTER:     g_park_col = g_lot_col = ARGB(1,3,0); return 1;   // lawn to the water
    case DOP_DRAW_LOT:  draw_house(&g_lot[arg]); return 1;
    case DOP_PROP:      return island_prop(arg >> 8, arg & 0xFF);
    case DOP_DRAW_PROP: draw_prop(&g_prop[arg]); return 1;
    default: return 0;
  }
}

DOMAIN_ENTRY(dom_secret_entry)
