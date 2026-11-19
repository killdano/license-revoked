#include "wr.h"
#include "wr_domain.h"

// ---------------------------------------------------------------------------
// The city is an algorithm, not a grid.
//
// cell_at() is a pure function of the tile coordinate - roads, sidewalks,
// parks, beach and sea all fall out of a couple of modulos and a hash. That
// removed the 4.6 KB grid, and it means the island can be any size without
// costing a byte.
//
// Buildings used to be drawn one box per tile, which is why blocks looked
// hollow and flickered: adjacent tiles of equal height had almost identical
// depths, so the painter sort swapped them every frame, and every interior
// party wall was being rasterised inside the solid mass. Now a block is
// subdivided once into a handful of LOTS and each lot is drawn as a single
// box. One box, one depth, no seams, nothing to fight over.
// ---------------------------------------------------------------------------

Lot  g_lot[MAX_LOT];
int  g_nlot;
Prop g_prop[MAX_PROP];

static int32_t s_cache_bx = 0x7FFFFFFF, s_cache_by = 0x7FFFFFFF;
static bool s_interior;

//                            lamp sign box tbl chr hyd tree palm rock parasol counter cabinet
// Nothing on the beach is an immovable wall any more. A boulder used to have
// 255 hp and count as solid, so a stretch of sand was a minefield of things
// that stopped a car dead.
// A destroyed prop is deliberately never rebuilt - props_refresh() keeps
// anything knocked loose where the player left it - so anything that should
// still be there tomorrow has to be indestructible today. Traffic signals
// were 40, which a single stray round destroys, and once gunfire started
// breaking props they began vanishing from junctions permanently. Steel
// masts, bridge pylons, jetties and shop doors are all 255 now.
const uint8_t PROP_HP[PROP_KIND_COUNT] =
  { 50, 45, 60, 22, 12, 70, 120, 70, 90, 18, 120, 34, 255, 255, 70, 4,
    30, 255, 18, 255, 255, 255 };


// Cheap deterministic hash. Same input always gives the same city.
static inline uint32_t hash2(int32_t a, int32_t b, uint32_t salt) {
  uint32_t h = (uint32_t)(a * 0x9E3779B1u) ^ (uint32_t)(b * 0x85EBCA77u) ^ salt;
  h ^= h >> 15; h *= 0x2C1B3C6Du;
  h ^= h >> 12; h *= 0x297A2D39u;
  h ^= h >> 15;
  return h;
}

// Island outline. The old version compared wr_dist() - an octagonal metric -
// against an integer radius, which is why the coast came out in blocky steps
// and the ring road broke into slabs. This works in SQUARED distance against a
// true circle, so the shoreline is smooth.
// The coastline's wobble, in SUBUNITS, for a direction from an island
// centre. Two things were wrong with the first version and both showed as
// a spike at the top of each island: the pseudo-angle covered only half a
// turn, so the 3x and 5x harmonics did not meet up where it wrapped, and
// quadrants three and four were joined the wrong way round. The result was
// rounded to whole tiles as well, which is what staircased the shore.
static int32_t coast_wobble(int32_t dx, int32_t dy) {
  int32_t adx = wr_abs(dx), ady = wr_abs(dy);
  int32_t den = adx + ady;
  uint16_t a = 0;
  if (den) {
    int32_t t = (ady * 16384) / den;                    // 0..16384 per quadrant
    a = (uint16_t)(dx >= 0 ? (dy >= 0 ? t : 65536 - t)
                           : (dy >= 0 ? 32768 - t : 32768 + t));
  }
  return (fsin((uint16_t)(a * 3)) * 4 * TILE + fsin((uint16_t)(a * 5 + 9000)) * 3 * TILE) >> 10;
}

int32_t city_coast_wobble(int32_t dx, int32_t dy) {
  return coast_wobble(dx, dy);
}


static inline int32_t modp(int32_t a, int32_t m) {
  int32_t r = a % m;
  return r < 0 ? r + m : r;
}

// Perpendicular distance from a point to the nearest centreline of one
// diagonal family. s is (x + y) or (x - y); dividing by root two converts the
// offset along that axis into a true perpendicular distance.
// Signed offset from the nearest centreline, in band units.
static inline int32_t band_off(int32_t s) {
  int32_t m = modp(s, DIAG_PERIOD);
  return (m > DIAG_PERIOD / 2) ? m - DIAG_PERIOD : m;
}
static inline int32_t band_dist(int32_t s) {
  int32_t m = modp(s, DIAG_PERIOD);
  if (m > DIAG_PERIOD / 2) m = DIAG_PERIOD - m;
  return (m * 724) >> 10;
}
static inline int32_t band_index(int32_t s) {
  int32_t q = s / DIAG_PERIOD;
  if (s < 0) q--;
  return q;
}

uint8_t cell_at(int32_t tx, int32_t ty) {
  return cell_at_point(((tx) * TILE) + TILE / 2, ((ty) * TILE) + TILE / 2);
}

// Distance from a point to the highway centreline, and how far along it. The
// centreline runs between the two island centres, so the same test gives a
// main road on land and a bridge deck over the water.
// A simple arch: zero at each end, BRIDGE_RISE at mid-span. `along` is the
// same 0..1024 parameter on_bridge() reports, so anything that knows where
// it is on the bridge can ask how high the deck is under it.
static inline int32_t clamp_d(int32_t d) { return wr_clamp(d, -30000, 30000); }

const Island ISLANDS[N_ISLAND] = {
  { ISLAND_CX, ISLAND_CY, CITY_R,  SAND_R  },
  { ISLAND_BX, ISLAND_BY, CITY_BR, SAND_BR },
  { ISLAND_GX, ISLAND_GY, CITY_GR, SAND_GR },
  { ISLAND_SX, ISLAND_SY, CITY_SR, SAND_SR },
};

// Any lot whose centre is within three tiles of the island's airport spot:
// one or two adjacent lots. An airport is allowed to be big.
// The only building on the secret island is the house.
bool lot_is_home(const Lot *L) { return island_at((L->x0 + L->x1) / 2, (L->y0 + L->y1) / 2) == ISL_SECRET; }

bool lot_is_airport(const Lot *L) {
  int32_t cx = (L->x0 + L->x1) / 2, cy = (L->y0 + L->y1) / 2;
  for (int i = 0; i < N_ISLAND; i++) {
    if (!island_has_airport(i)) continue;
    int32_t dx = cx - (ISLANDS[i].cx * TILE + 2 * TILE), dy = cy - (ISLANDS[i].cy * TILE + 2 * TILE);
    if (dx > -3 * TILE && dx < 3 * TILE && dy > -3 * TILE && dy < 3 * TILE) return true;
  }
  return false;
}

int island_at(int32_t x, int32_t y) {
  int best = 0; int32_t bd = 0x7FFFFFFF;
  for (int i = 0; i < N_ISLAND; i++) {
    int32_t dx = clamp_d(x - ISLANDS[i].cx * TILE), dy = clamp_d(y - ISLANDS[i].cy * TILE);
    int32_t d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}

static void island_of(int32_t x, int32_t y, int32_t *r2, int32_t *city, int32_t *sand) {
  int i = island_at(x, y);
  int32_t dx = clamp_d(x - ISLANDS[i].cx * TILE), dy = clamp_d(y - ISLANDS[i].cy * TILE);
  int32_t w = coast_wobble(dx, dy);
  *r2 = dx * dx + dy * dy;
  *city = ISLANDS[i].city_r * TILE + w;
  *sand = ISLANDS[i].sand_r * TILE + w;
  if (*city < 6 * TILE) *city = 6 * TILE;
  if (*sand < *city + 3 * TILE) *sand = *city + 3 * TILE;
}

const Bridge BRIDGES[N_BRIDGE] = {
  { ISL_CITY, ISL_COAST,  BRIDGE_T0, BRIDGE_T1, 0, 0, 2316 },
  // City to ghetto: over twenty tiles of water, and the middle of it is
  // gone. The arch peaks at mid-span, which is exactly where the gap is,
  // so the deck itself is the ramp.
  // The deck begins at the city's ring road (24 tiles of 68: t=361) so
  // the streets lead onto it, and ends at the ghetto's (52 of 68: t=783).
  // The break is at the ghetto end, just short of its beach - you drive
  // onto the bridge from town, and the gap is the last thing before the
  // far shore. It can be jumped from either side.
  { ISL_CITY, ISL_GHETTO, 361, 783, 690, 734, 15466 },
};

// A simple arch: zero at each end, BRIDGE_RISE at mid-span. `along` is the
// same 0..1024 parameter bridge_find() reports.
int32_t bridge_height_of(int b, int32_t along) {
  int32_t t = along - BRIDGES[b].t0;
  int32_t span = BRIDGES[b].t1 - BRIDGES[b].t0;
  if (t <= 0 || t >= span) return 0;
  return wr_muldiv(4 * BRIDGE_RISE * t, span - t, span * span);
}
int32_t bridge_height(int32_t along) { return bridge_height_of(BR_CAUSEWAY, along); }

bool bridge_in_gap(int b, int32_t t) {
  return BRIDGES[b].gap1 && t >= BRIDGES[b].gap0 && t <= BRIDGES[b].gap1;
}

static bool bridge_lat_t_of(int b, int32_t x, int32_t y, int32_t *lat, int32_t *t) {
  const Island *A = &ISLANDS[BRIDGES[b].a], *B = &ISLANDS[BRIDGES[b].b];
  const int32_t ax = A->cx * TILE, ay = A->cy * TILE;
  const int32_t dx = (B->cx - A->cx) * TILE, dy = (B->cy - A->cy) * TILE;
  const int32_t len2 = dx * dx + dy * dy;
  int32_t dot = (x - ax) * dx + (y - ay) * dy;
  *t = wr_muldiv(dot, 1024, len2);
  if (*t < BRIDGES[b].t0 || *t > BRIDGES[b].t1) return false;
  int32_t cr = (y - ay) * dx - (x - ax) * dy;
  static int32_t s_len[N_BRIDGE];
  if (!s_len[b]) s_len[b] = wr_sqrt(len2 >> 4) << 2;
  *lat = cr / s_len[b];
  return true;
}

int bridge_find(int32_t x, int32_t y, int32_t *lat, int32_t *t) {
  for (int b = 0; b < N_BRIDGE; b++) {
    int32_t l, tt;
    if (bridge_lat_t_of(b, x, y, &l, &tt) && wr_abs(l) < BRIDGE_KERB + 2 * TILE) {
      if (lat) *lat = l;
      if (t) *t = tt;
      return b;
    }
  }
  return -1;
}

bool on_bridge(int32_t x, int32_t y, int32_t *dist, int32_t *along) {
  int32_t lat, t;
  if (bridge_find(x, y, &lat, &t) < 0) return false;
  if (dist) *dist = wr_abs(lat);
  if (along) *along = t;
  return true;
}

// The heading along a bridge, a->b. A constant in the table: it used to be
// found by scanning the sine table, which cost 150 bytes to compute a
// number that never changes.
uint16_t bridge_angle_of(int b) { return BRIDGES[b].ang; }
uint16_t bridge_angle(void) { return bridge_angle_of(BR_CAUSEWAY); }


// A junction on the diagonal grid is only real if the crossing itself is
// diagonal road (not buried under the ring or the causeway) and at least
// three arms actually lead somewhere. Returns a bitmask of live arms in
// ROAD_ARM order, or 0 for "not a junction".
uint8_t city_junction_arms(int32_t jx, int32_t jy) {
  Road r[4];
  int n = road_here(jx, jy, r);
  for (int i = 0; i < n; i++)
    if (r[i].kind >= RD_RING && wr_abs(r[i].lat) < r[i].half + 60) return 0;
  // An arm is live only if it is road both near and far - where the ring
  // road or the coast cuts a junction, an arm that is road at one sample
  // and sand at the next used to get a crosswalk painted onto the beach.
  uint8_t mask = 0; int cnt = 0;
  for (int a = 0; a < 4; a++) {
    int32_t qx = jx + ((fcos(ROAD_ARM[a]) * (TILE * 3)) >> 10);
    int32_t qy = jy + ((fsin(ROAD_ARM[a]) * (TILE * 3)) >> 10);
    if (cell_at_point(qx, qy) != CELL_ROAD) continue;
    int32_t px = jx + ((fcos(ROAD_ARM[a]) * (TILE + TILE / 2)) >> 10);
    int32_t py = jy + ((fsin(ROAD_ARM[a]) * (TILE + TILE / 2)) >> 10);
    if (cell_at_point(px, py) == CELL_ROAD) { mask |= (uint8_t)(1 << a); cnt++; }
  }
  return cnt == 4 ? mask : 0;                    // a full cross, or no paint at all
}

const uint16_t ROAD_ARM[4] = { 8192, 24576, 40960, 57344 };

// A point on the ring road's centreline at polar angle `a`, `lat` toward
// the island centre.
static void ring_point(int32_t cx, int32_t cy, int32_t base, uint16_t a,
                       int32_t lat, int32_t *ox, int32_t *oy) {
  int32_t c = fcos(a), s = fsin(a);
  int32_t r = base + coast_wobble(c, s) - 2 * TILE + RING_HALF - lat;
  *ox = cx + fmul(r, c);
  *oy = cy + fmul(r, s);
}

void road_point(const Road *rd, int32_t x, int32_t y, uint16_t hd, int32_t d,
                int32_t lat, int32_t *ox, int32_t *oy) {
  int32_t dh = (int16_t)(uint16_t)(hd - rd->head);
  int dir = (wr_abs(dh) <= 16384) ? 1 : -1;
  if (rd->kind == RD_RING) {
    uint16_t a = angle_to(rd->cx, rd->cy, x, y);
    int32_t rr = wr_dist(x - rd->cx, y - rd->cy);
    if (rr < TILE) rr = TILE;
    // Arc length to angle: 65536 / 2*pi = 10430.
    int32_t da = wr_muldiv(d, 10430, rr);
    ring_point(rd->cx, rd->cy, rd->base, (uint16_t)(a + dir * da), lat, ox, oy);
  } else {
    int32_t c = fcos(rd->head), s = fsin(rd->head);
    int32_t shift = lat - rd->lat;
    *ox = x + fmul(dir * d, c) - fmul(shift, s);
    *oy = y + fmul(dir * d, s) + fmul(shift, c);
  }
}

// Every road within pavement reach of a point. Up to four: the causeway,
// the ring, and one street of each diagonal family can all overlap.
int road_here(int32_t x, int32_t y, Road *out) {
  int n = 0;
  if (s_interior) return 0;
  if (x < -32000 || x > 65000 || y < -32000 || y > 65000) return 0;
  int32_t bl;
  int32_t bt2; int bi2 = bridge_find(x, y, &bl, &bt2);
  if (bi2 >= 0 && !bridge_in_gap(bi2, bt2) && wr_abs(bl) < BRIDGE_KERB) {
    out[n].kind = RD_BRIDGE; out[n].head = bridge_angle_of(bi2);
    out[n].lat = (int16_t)bl; out[n].half = BRIDGE_HALF; out[n].pave = BRIDGE_KERB;
    n++;
  }
  int32_t r2, rc, rs;
  island_of(x, y, &r2, &rc, &rs);
  if (r2 > rc * rc) return n;                 // beach or sea: nothing else here
  if (island_at(x, y) == ISL_SECRET) return n;   // no streets, no ring road on the secret island
  int32_t ring_r = rc - 2 * TILE + RING_HALF;  // centreline radius
  int32_t lo = ring_r - RING_HALF - RING_PAVE, hi = rc;
  if (r2 > lo * lo && r2 <= hi * hi) {
    int32_t cx = ISLAND_CX * TILE, cy = ISLAND_CY * TILE, base = CITY_R * TILE;
    int32_t dxb = clamp_d(x - ISLAND_BX * TILE), dyb = clamp_d(y - ISLAND_BY * TILE);
    if (dxb * dxb + dyb * dyb == r2) {
      cx = ISLAND_BX * TILE; cy = ISLAND_BY * TILE; base = CITY_BR * TILE;
    }
    int32_t rr = wr_sqrt(r2);
    uint16_t a = angle_to(cx, cy, x, y);
    // The direction along the wobbled curve, not the circle's tangent.
    int32_t p0x, p0y, p1x, p1y;
    ring_point(cx, cy, base, (uint16_t)(a - 1024), 0, &p0x, &p0y);
    ring_point(cx, cy, base, (uint16_t)(a + 1024), 0, &p1x, &p1y);
    out[n].kind = RD_RING;
    out[n].head = angle_to(p0x, p0y, p1x, p1y);                 // anticlockwise
    out[n].lat = (int16_t)(ring_r - rr);                         // centre is to the left
    out[n].half = RING_HALF; out[n].pave = RING_HALF + RING_PAVE;
    out[n].cx = cx; out[n].cy = cy; out[n].base = base;
    n++;
  }
  for (int fam = 0; fam < 2; fam++) {
    int32_t s = fam ? (x - y) : (x + y);
    int32_t m = modp(s, DIAG_PERIOD);
    if (m > DIAG_PERIOD / 2) m -= DIAG_PERIOD;
    int32_t lat = (m * 724) >> 10;
    if (wr_abs(lat) >= ROAD_HALF + 300) continue;    // to the far edge of the crosswalk: the paint rule needs it
    out[n].kind = (uint8_t)(fam ? RD_DIAG_V : RD_DIAG_U);
    out[n].head = fam ? 8192 : 57344;
    out[n].lat  = (int16_t)(fam ? -lat : lat);
    out[n].half = ROAD_HALF; out[n].pave = PAVE_HALF;
    n++;
  }
  return n;
}

// The road a vehicle pointing `pref` is driving on: of everything under it,
// the one it is best aligned with. That is what keeps a car crossing the
// causeway on its own street instead of being handed to the highway.
bool road_at(int32_t x, int32_t y, uint16_t pref, Road *out) {
  Road r[4];
  int n = road_here(x, y, r);
  int best = -1; int32_t bd = -1;
  for (int i = 0; i < n; i++) {
    if (wr_abs(r[i].lat) > r[i].half + 70) continue;
    int32_t d = wr_abs((int32_t)(int16_t)(uint16_t)(r[i].head - pref));
    if (d > 16384) d = 32768 - d;
    int32_t score = 16384 - d;
    if (score > bd) { bd = score; best = i; }
  }
  if (best < 0) return false;
  *out = r[best];
  return true;
}

uint8_t cell_at_point(int32_t x, int32_t y) {
  if (s_interior) return CELL_PAVE;      // floor in every direction: no sea through the walls
  // Guard the squaring: a stray coordinate would otherwise overflow int32
  // and report the open sea as dry land.
  if (x < -32000 || x > 65000 || y < -32000 || y > 65000) return CELL_WATER;

  // Any carriageway beats any pavement. The causeway used to win outright,
  // which laid its kerb across every street it crossed on land.
  int32_t bl, bt; int bi = bridge_find(x, y, &bl, &bt);
  // In a bridge's gap there is no deck at all - the collapsed span. The
  // stubs either side keep their kerb, so the lip is a real edge.
  bool onb = bi >= 0 && !bridge_in_gap(bi, bt);
  if (onb && wr_abs(bl) < BRIDGE_HALF) return CELL_ROAD;
  bool bpave = onb && wr_abs(bl) < BRIDGE_KERB;

  int32_t r2, rc, rs;
  island_of(x, y, &r2, &rc, &rs);
  if (r2 > rs * rs) return bpave ? CELL_PAVE : CELL_WATER;
  if (r2 > rc * rc) return bpave ? CELL_PAVE : CELL_SAND;

  if (island_at(x, y) == ISL_SECRET) {
    // The secret island: lawn, a runway east-west through the middle, and
    // the house's footprint north of it. No ring road, no grid.
    int32_t dx = x - ISLAND_SX * TILE, dy = y - ISLAND_SY * TILE;
    if (wr_abs(dy + 2 * TILE) < 120) return CELL_ROAD;                                   // the runway
    if (dx > -2 * TILE && dx < 2 * TILE && dy < -3 * TILE - 40 && dy > -6 * TILE) return CELL_BLD;   // the house
    return bpave ? CELL_PAVE : CELL_PARK;
  }
  int32_t rin = rc - 2 * TILE;                  // ring road inner edge
  if (r2 > rin * rin) return CELL_ROAD;

  int32_t du = band_dist(x + y), dv = band_dist(x - y);
  int32_t d = du < dv ? du : dv;
  if (d < ROAD_HALF) return CELL_ROAD;
  if (bpave) return CELL_PAVE;
  rin -= RING_PAVE;
  if (r2 > rin * rin) return CELL_PAVE;
  if (d < PAVE_HALF) return CELL_PAVE;

  int32_t bu = band_index(x + y), bv = band_index(x - y);
  if ((hash2(bu, bv, 0xA17Eu) % 12) == 0) return CELL_PARK;
  return CELL_BLD;
}

// ---------------------------------------------------------------------------
// Lots
// ---------------------------------------------------------------------------

// Buildings are no longer derived from the road grid. Candidate footprints sit
// on a fixed lattice and a lot is only built where every sample inside it is
// still buildable ground. That decouples the architecture from the street
// layout completely - the avenues can run at any angle and the buildings just
// fill whatever is left.
// One building per diamond block, inscribed in it.
//
// The previous version fitted axis-aligned footprints onto a global lattice
// and only kept those that landed entirely on buildable ground. Inside a
// diamond block almost nothing lined up, so most blocks came up empty and
// rendered as bare road - which is what made the city look like a
// checkerboard of pavement stripes. Working from the block itself guarantees
// every block gets built on.
static void block_lots(int32_t bu, int32_t bv, Lot *out, int *n, int max) {
  if (*n >= max) return;

  int32_t cu = bu * DIAG_PERIOD + DIAG_PERIOD / 2;
  int32_t cv = bv * DIAG_PERIOD + DIAG_PERIOD / 2;
  int32_t cx = (cu + cv) / 2;
  int32_t cy = (cu - cv) / 2;
  if (island_at(cx, cy) == ISL_SECRET) {
    // The secret island has one building, the house, at a fixed place;
    // the block whose centre is nearest it owns it.
    int32_t hx = ISLAND_SX * TILE, hy = ISLAND_SY * TILE - 4 * TILE - TILE / 2;
    if (wr_abs(cx - hx) > DIAG_PERIOD / 2 || wr_abs(cy - hy) > DIAG_PERIOD / 2) return;
    Lot *L = &out[(*n)++];
    memset(L, 0, sizeof(Lot));
    L->x0 = hx - 2 * TILE + 20; L->x1 = hx + 2 * TILE - 20; L->y0 = hy - TILE - 40; L->y1 = hy + TILE + 40;
    L->kind = LOT_SOLID; L->h = 120;
    return;
  }
  if (cell_at(cx >> TILE_SH, cy >> TILE_SH) != CELL_BLD) return;

  int32_t half = ((DIAG_PERIOD / 2) - PAVE_HALF - 40) * 724 / 1024 / 2;
  if (half < TILE / 2) return;
  int32_t x0 = cx - half, y0 = cy - half;

  // Split the block once: one lot, two side by side (either way), or four
  // around a light well. Each lot is cut from a grid of nx by ny cells
  // with a 44-unit gap between them.
  uint32_t hh = hash2(bu, bv, 0xB105u);
  int split = (int)(hh % 10);
  int nx = (split < 4) ? 1 : (split < 8) ? ((hh & 1) ? 2 : 1) : 2;
  int ny = (split < 4) ? 1 : (split < 8) ? ((hh & 1) ? 1 : 2) : 2;
  int32_t w = (2 * half) / nx, hgt = (2 * half) / ny;

  for (int i = 0; i < nx * ny && *n < max; i++) {
    int ix = i % nx, iy = i / nx;
    int32_t rx = x0 + ix * w + (ix ? 44 : 0), rX = x0 + (ix + 1) * w - (ix + 1 < nx ? 44 : 0);
    int32_t ry = y0 + iy * hgt + (iy ? 44 : 0), rY = y0 + (iy + 1) * hgt - (iy + 1 < ny ? 44 : 0);
    // Only where every sample inside it is still buildable ground.
    bool ok = true;
    for (int si = 0; si < 9 && ok; si++)
      if (cell_at_point(rx + ((rX - rx) * (si % 3)) / 2, ry + ((rY - ry) * (si / 3)) / 2) != CELL_BLD)
        ok = false;
    if (!ok) continue;

    uint32_t lh = hash2(bu * 8 + i, bv, 0xB105u);
    Lot *L = &out[(*n)++];
    L->x0 = rx; L->y0 = ry; L->x1 = rX; L->y1 = rY;
    L->mat = (uint8_t)(lh % 6);
    L->glass = 0;

    uint32_t k = (lh >> 8) % 100;
    if (k < 24)      { L->kind = LOT_GARAGE;  L->h = (int16_t)(TILE + TILE / 2); }
    else if (k < 27) { L->kind = LOT_STORE;   L->h = (int16_t)(TILE * (1 + (int32_t)((lh >> 16) % 2))); }
    else if (k < 34) { L->kind = LOT_GUNSHOP; L->h = (int16_t)(TILE + TILE / 2); }
    else if (k < 40) { L->kind = LOT_CLUB;    L->h = (int16_t)(TILE + TILE / 2); }
    else             { L->kind = LOT_SOLID;   L->h = (int16_t)(TILE * (1 + (int32_t)((lh >> 16) % 5))); }

    // Downtown: taller toward the centre of the main island, and no sheds.
    int32_t dtx = ((rx + rX) / 2) / TILE - ISLAND_CX;
    int32_t dty = ((ry + rY) / 2) / TILE - ISLAND_CY;
    int32_t d2 = dtx * dtx + dty * dty;
    if (d2 < 8 * 8) {
      if (L->kind == LOT_GUNSHOP || L->kind == LOT_CLUB || d2 < 6 * 6) L->kind = LOT_SOLID;
      if (L->kind == LOT_SOLID) {
        L->h = (int16_t)(TILE * (3 + (8 - wr_sqrt(d2)) + (int32_t)((lh >> 20) % 3)));
        if (L->h > 9 * TILE) L->h = 9 * TILE;
      }
    }
  }
}

// Which side does a shop's door face? The +y face was hardcoded, and on a
// lattice of lots that regularly pointed the door straight at the building
// next door. Walk the four faces and take the first with walkable ground a
// half-tile outside its midpoint; both the renderer (sign, door slab) and
// the entry test call this, so they can never disagree.
void city_shop_door(const Lot *L, int32_t *dx, int32_t *dy,
                    int32_t *nx, int32_t *ny) {
  int32_t mx = (L->x0 + L->x1) / 2, my = (L->y0 + L->y1) / 2;
  const int32_t px[4] = { mx, mx, L->x1, L->x0 };
  const int32_t py[4] = { L->y1, L->y0, my, my };
  const int32_t vx[4] = { 0, 0, 1, -1 };
  const int32_t vy[4] = { 1, -1, 0, 0 };
  // Score each face by how far a person could actually stand in front of
  // it - probe outward until something (another lot, deep water) blocks.
  // Cell class is the wrong question: the inside of a block is all
  // CELL_BLD, yet the courtyard gaps in it are perfectly walkable. The
  // face with the longest clear frontage wins, which naturally prefers
  // street and pavement frontage over a neighbour's back wall.
  int best = 0; int32_t bestclear = -1;
  for (int f = 0; f < 4; f++) {
    int32_t clear = 0;
    for (int32_t d = 40; d <= 200; d += 40) {
      int32_t sx = px[f] + vx[f] * d, sy = py[f] + vy[f] * d;
      if (city_blocked(sx, sy, false)) break;
      clear = d;
    }
    if (clear > bestclear) { bestclear = clear; best = f; }
  }
  *dx = px[best]; *dy = py[best];
  *nx = vx[best]; *ny = vy[best];
}

// Swap the world for the shop interior: a handful of wall lots instead of a
// city. Everything else - drawing, collision, physics - carries over.
void city_set_interior(bool on) {
  s_interior = on;
  s_cache_bx = 0x7FFFFFFF;             // force a rebuild either way
}

void city_refresh(int32_t px, int32_t py) {
  if (s_interior) return;              // the room is built once on entry
  int32_t ptx = px >> TILE_SH, pty = py >> TILE_SH;
  // Rebuild whenever the player moves a few tiles, not a whole block.
  int32_t bx = ptx >> 2, by = pty >> 2;
  if (bx == s_cache_bx && by == s_cache_by) return;

  s_cache_bx = bx; s_cache_by = by;
  g_nlot = 0;

  // Walk the diamond blocks around the player in diagonal space.
  int32_t bu0 = band_index(px + py), bv0 = band_index(px - py);
  for (int32_t j = -3; j <= 3; j++)
    for (int32_t i = -3; i <= 3; i++)
      block_lots(bu0 + i, bv0 + j, g_lot, &g_nlot, MAX_LOT);

  props_refresh();
  pickups_refresh();
}

// Streets run at +/-45 degrees, in two families. Snap onto whichever
// centreline is nearer and hand back a heading that runs along it.
// Two moorings on each island, on opposite shores so there is always one
// within reach. Placed just outside the beach line.
const int16_t MARINA_TX[MARINA_COUNT] = {
  ISLAND_CX - SAND_R - 1, ISLAND_CX + 4,
  ISLAND_BX + SAND_BR + 1, ISLAND_BX - 3,
};
const int16_t MARINA_TY[MARINA_COUNT] = {
  ISLAND_CY + 3,          ISLAND_CY + SAND_R + 1,
  ISLAND_BY - 3,          ISLAND_BY - SAND_BR - 1,
};

int nearest_marina(int32_t x, int32_t y, int32_t *dist) {
  int best = 0; int32_t bd = 0x7FFFFFFF;
  for (int i = 0; i < MARINA_COUNT; i++) {
    int32_t d = wr_dist(MARINA_TX[i] * TILE - x, MARINA_TY[i] * TILE - y);
    if (d < bd) { bd = d; best = i; }
  }
  if (dist) *dist = bd;
  return best;
}

void road_snap(int32_t *x, int32_t *y, uint16_t *ang) {
  int32_t px = *x, py = *y;
  Road r[4];
  int n = road_here(px, py, r);
  if (n) {
    // On or beside a road: into its right-hand lane for a random direction.
    int best = 0;
    for (int i = 1; i < n; i++) if (wr_abs(r[i].lat) < wr_abs(r[best].lat)) best = i;
    bool back = ang && wr_rand_n(2);
    uint16_t dir = back ? (uint16_t)(r[best].head + 32768) : r[best].head;
    road_point(&r[best], px, py, dir, 0, ang ? (back ? 92 : -92) : 0, x, y);
    if (ang) *ang = dir;
    return;
  }
  // Nowhere near a road: the nearest street centreline on the lattice.
  int32_t su = px + py, sv = px - py;
  int32_t ku = (su >= 0 ? su + DIAG_PERIOD / 2 : su - DIAG_PERIOD / 2) / DIAG_PERIOD;
  int32_t kv = (sv >= 0 ? sv + DIAG_PERIOD / 2 : sv - DIAG_PERIOD / 2) / DIAG_PERIOD;
  int32_t cu = ku * DIAG_PERIOD, cv = kv * DIAG_PERIOD;
  if (wr_abs(su - cu) <= wr_abs(sv - cv)) { *x = (cu + sv) / 2; *y = (cu - sv) / 2; if (ang) *ang = 57344u; }
  else                                     { *x = (su + cv) / 2; *y = (su - cv) / 2; if (ang) *ang = 8192u; }
}

// Where the venues are, found once by walking every block on both
// islands. The lot cache only knows the blocks around you; the pointer on
// the HUD needs to know about the club on the other island.
Venue g_venue[MAX_VENUE];
int   g_nvenue;
void city_venues_init(void) {
  g_nvenue = 0;
  Lot tmp[8];
  // bu is (x+y)/period, so the far corner of the world is at 2*CITY_MAX:
  // the old bound stopped short of the ghetto, and the map had no marks
  // on it.
  for (int32_t bu = 0; bu < (2 * CITY_MAX) / DIAG_PERIOD + 1 && g_nvenue < MAX_VENUE; bu++)
    for (int32_t bv = -CITY_MAX / DIAG_PERIOD; bv < CITY_MAX / DIAG_PERIOD + 1 && g_nvenue < MAX_VENUE; bv++) {
      int n = 0;
      block_lots(bu, bv, tmp, &n, 8);
      for (int i = 0; i < n && g_nvenue < MAX_VENUE; i++) {
        bool ap = lot_is_airport(&tmp[i]);
#ifdef WR_VENUE_TRACE
        { extern int printf(const char*,...); if (tmp[i].kind == LOT_GARAGE) printf("garage isl=%d at %d,%d airport=%d\n", island_at((tmp[i].x0 + tmp[i].x1) / 2, (tmp[i].y0 + tmp[i].y1) / 2), (int)(tmp[i].x0/TILE), (int)(tmp[i].y0/TILE), lot_is_airport(&tmp[i])); }
#endif
        if (tmp[i].kind != LOT_CLUB && tmp[i].kind != LOT_GUNSHOP && !ap) continue;
        Venue *v = &g_venue[g_nvenue++];
        v->x = (int16_t)(((tmp[i].x0 + tmp[i].x1) / 2) >> 2);
        v->y = (int16_t)(((tmp[i].y0 + tmp[i].y1) / 2) >> 2);
        v->club = ap ? 2 : (tmp[i].kind == LOT_CLUB);      // 0 store, 1 club, 2 airport
      }
    }
}

int lot_at(int32_t x, int32_t y) {
  for (int i = 0; i < g_nlot; i++) {
    Lot *L = &g_lot[i];
    if (x >= L->x0 && x <= L->x1 && y >= L->y0 && y <= L->y1) return i;
  }
  return -1;
}

// ---------------------------------------------------------------------------
// Collision
// ---------------------------------------------------------------------------

// Two rows of bays with an aisle down the middle of the longer axis. The
// aisle is what makes a garage a shortcut rather than a car park: you can
// still drive straight through, you just have to thread it. Deterministic
// from the lot itself, so the renderer and the collision test agree without
// anything being stored.
int city_garage_slots(const Lot *L, int32_t (*out)[3], int max) {
  int32_t w = L->x1 - L->x0, hgt = L->y1 - L->y0;
  bool along_x = w >= hgt;
  int32_t len = along_x ? w : hgt;
  int n = (int)(len / 150);
  if (n > GARAGE_SLOTS) n = GARAGE_SLOTS;
  if (n < 2) return 0;
  int32_t cx = (L->x0 + L->x1) / 2, cy = (L->y0 + L->y1) / 2;
  int32_t bay = (along_x ? hgt : w) / 2 - 46;      // centre of a bay row
  if (bay < 40) return 0;
  uint32_t h = hash2(L->x0, L->y0, 0x6A12u);
  int wrote = 0;
  for (int i = 0; i < n && wrote < max; i++) {
    int32_t t = (len * (i * 2 + 1)) / (n * 2) - len / 2;
    for (int row = 0; row < 2 && wrote < max; row++) {
      if (((h >> (i * 2 + row)) & 3u) == 0) continue;   // some bays empty
      int32_t off = row ? bay : -bay;
      out[wrote][0] = along_x ? (cx + t) : (cx + off);
      out[wrote][1] = along_x ? (cy + off) : (cy + t);
      out[wrote][2] = along_x ? (row ? 32768 : 0) : (row ? 49152 : 16384);
      wrote++;
    }
  }
  return wrote;
}

// The four corners of a block's interior, inset from the street centrelines
// by `inset` world units. Band space is 45 degrees to the world, so a world
// inset is 1.414x along u and v.
static void block_quad(int32_t bu, int32_t bv, int32_t inset,
                       int32_t (*out)[2]) {
  int32_t i2 = (inset * 1448) >> 10;
  int32_t u0 = bu * DIAG_PERIOD + i2, u1 = (bu + 1) * DIAG_PERIOD - i2;
  int32_t v0 = bv * DIAG_PERIOD + i2, v1 = (bv + 1) * DIAG_PERIOD - i2;
  const int32_t U[4] = { u0, u1, u1, u0 };
  const int32_t V[4] = { v0, v0, v1, v1 };
  for (int i = 0; i < 4; i++) {
    out[i][0] = (U[i] + V[i]) / 2;
    out[i][1] = (U[i] - V[i]) / 2;
  }
}

int city_park_quads(int32_t px, int32_t py, int32_t (*out)[4][2], int max) {
  if (s_interior) return 0;
  int32_t bu0 = band_index(px + py), bv0 = band_index(px - py);
  int n = 0;
  for (int32_t a = bu0 - 1; a <= bu0 + 1 && n < max; a++)
    for (int32_t b = bv0 - 1; b <= bv0 + 1 && n < max; b++) {
      if ((hash2(a, b, 0xA17Eu) % 12) != 0) continue;      // same test as cell_at
      // Only where the block really is land - a park diamond half in the
      // sea would be worse than the squares it replaces.
      int32_t cu = a * DIAG_PERIOD + DIAG_PERIOD / 2;
      int32_t cv = b * DIAG_PERIOD + DIAG_PERIOD / 2;
      int32_t cx = (cu + cv) / 2, cy = (cu - cv) / 2;
      if (cell_at_point(cx, cy) != CELL_PARK) continue;
      block_quad(a, b, PAVE_HALF, out[n]);
      n++;
    }
  return n;
}


int city_junctions(int32_t px, int32_t py, int32_t (*out)[3], int max) {
  if (s_interior) return 0;
  int32_t k1 = band_index(px + py), k2 = band_index(px - py);
  int n = 0;
  for (int32_t a = k1 - 1; a <= k1 + 1 && n < max; a++)
    for (int32_t b = k2 - 1; b <= k2 + 1 && n < max; b++) {
      int32_t u = a * DIAG_PERIOD, v = b * DIAG_PERIOD;
      int32_t jx = (u + v) / 2, jy = (u - v) / 2;
      uint8_t arms = city_junction_arms(jx, jy);
      if (!arms) continue;
      out[n][0] = jx; out[n][1] = jy; out[n][2] = arms;
      n++;
    }
  return n;
}

bool city_blocked(int32_t x, int32_t y, bool is_car) {
  (void)is_car;
  uint8_t c = cell_at_world(x, y);
  if (c == CELL_WATER) return true;      // the sea stops everything

  // A tree or a lamp post is a real obstacle until you have snapped it off.
  for (int i = 0; i < MAX_PROP; i++) {
    Prop *p = &g_prop[i];
    if (!p->alive || p->loose || !prop_solid(p->kind)) continue;
    if (wr_dist(p->x - x, p->y - y) < 34) return true;
  }

  int li = lot_at(x, y);
  if (li < 0) return false;

  Lot *L = &g_lot[li];
  // A garage is open at ground level, and a storefront stops being solid once
  // its window is gone.
  if (L->kind == LOT_GARAGE) {
    // Open at ground level, and nothing here blocks. The bays used to, to
    // give the decorative cars some presence - but decorative cars were the
    // wrong idea. Real vehicles park in them now, they block by being
    // vehicles, and an empty bay or one you have just driven out of is
    // exactly as passable as it looks.
    return false;
  }
  if (L->kind == LOT_STORE && L->glass > 200) return false;
  return true;
}

// A boat's world is the exact inverse of a car's.
bool water_blocked(int32_t x, int32_t y) {
  return cell_at_point(x, y) != CELL_WATER;
}

bool car_lane_blocked(int32_t x, int32_t y) {
  if (city_blocked(x, y, true)) return true;
  // Measured against the continuous band, so traffic can follow a diagonal
  // street smoothly instead of clipping in and out of it tile by tile.
  return cell_at_point(x, y) != CELL_ROAD;
}

// ---------------------------------------------------------------------------
// Street furniture
// ---------------------------------------------------------------------------


// Placed deterministically from the tile, so furniture does not wander when
// the cache is rebuilt.
// Place a prop in the next free slot. A prop that has been knocked loose
// keeps its slot and its physics; placement steps over it. (Every placer
// used to write `n++` then `n--; continue` on a loose slot, which meant the
// first lamp post you knocked over blocked every prop placed after it.)
static Prop *prop_put(int *n, int32_t x, int32_t y, uint8_t kind, uint16_t ang) {
  while (*n < MAX_PROP && g_prop[*n].alive && g_prop[*n].loose) (*n)++;
  if (*n >= MAX_PROP) return NULL;
  Prop *p = &g_prop[(*n)++];
  memset(p, 0, sizeof(Prop));
  p->x = x; p->y = y; p->kind = kind; p->ang = ang;
  p->hp = kind >= PROP_DOMAIN0 ? 255 : PROP_HP[kind]; p->alive = 1;   // a domain's prop is indestructible
  return p;
}

void props_refresh(void) {
  if (s_interior) return;              // the shop sets its own fittings
  int n = 0;
  int32_t ptx = g_p.x >> TILE_SH, pty = g_p.y >> TILE_SH;

  // The causeway: pylons in the water beside the deck, barriers along its
  // kerb, every three tiles along the nearest stretch.
  for (int b = 0; b < N_BRIDGE; b++) {
    const Island *IA = &ISLANDS[BRIDGES[b].a], *IB = &ISLANDS[BRIDGES[b].b];
    const int32_t ax = IA->cx * TILE, ay = IA->cy * TILE;
    int32_t hdx = (IB->cx - IA->cx) * TILE;
    int32_t hdy = (IB->cy - IA->cy) * TILE;
    uint16_t ha = bridge_angle_of(b);
    int32_t hl2 = hdx * hdx + hdy * hdy;
    int32_t tp = wr_muldiv((g_p.x - ax) * hdx + (g_p.y - ay) * hdy, 1024, hl2);
    for (int s = -4; s <= 4 && n < MAX_PROP - 20; s++) {
      int32_t px = ax + wr_muldiv(hdx, tp, 1024) + fmul(s * 3 * TILE, fcos(ha));
      int32_t py = ay + wr_muldiv(hdy, tp, 1024) + fmul(s * 3 * TILE, fsin(ha));
      if (cell_at_point(px, py) != CELL_ROAD) continue;
      for (int side = -1; side <= 1; side += 2) {
        int32_t qx = px + fmul(side * (BRIDGE_KERB + 30), -fsin(ha));
        int32_t qy = py + fmul(side * (BRIDGE_KERB + 30),  fcos(ha));
        if (cell_at_point(qx, qy) != CELL_WATER) continue;
        prop_put(&n, qx, qy, PROP_PYLON, ha);
      }
    }
  }

#if WR_BOATS
  // The marina: a jetty out over the water the way it is deepest, with
  // mooring posts at the end, and a couple of planked sections ashore.
  {
    int32_t md;
    int mi = nearest_marina(g_p.x, g_p.y, &md);
    if (md < 16 * TILE) {
      int32_t mx = MARINA_TX[mi] * TILE, my = MARINA_TY[mi] * TILE;
      uint16_t best = 0; int bestrun = -1;
      for (int t = 0; t < 16; t++) {
        uint16_t a2 = (uint16_t)(t * 4096);
        int run = 0;
        for (int r = 1; r <= 8; r++) {
          if (cell_at_point(mx + fmul(r * 110, fcos(a2)), my + fmul(r * 110, fsin(a2))) != CELL_WATER) break;
          run++;
        }
        if (run > bestrun) { bestrun = run; best = a2; }
      }
      if (bestrun >= 2) {
        for (int r = 1; r <= bestrun; r++)
          prop_put(&n, mx + fmul(r * 110, fcos(best)), my + fmul(r * 110, fsin(best)),
                   PROP_DOCK, (uint16_t)(best + 16384));
        for (int side = -1; side <= 1; side += 2)
          prop_put(&n, mx + fmul(bestrun * 110, fcos(best)) + fmul(side * 60, -fsin(best)),
                       my + fmul(bestrun * 110, fsin(best)) + fmul(side * 60,  fcos(best)),
                   PROP_PYLON, best);
      }
      for (int j = 0; j < 2; j++)
        prop_put(&n, mx + j * 160, my + j * 120, PROP_DOCK, (uint16_t)(j * 12000));
    }
  }

#endif
  // Traffic signals at real crossings, on two opposite corners.
  {
    int32_t jun[MAX_JUNC][3];
    int nj = city_junctions(g_p.x, g_p.y, jun, MAX_JUNC);
    int32_t D = ((ROAD_HALF + 140) * 1448) >> 10;
    for (int j = 0; j < nj; j++)
      for (int k = -1; k <= 1; k += 2) {
        int32_t x = jun[j][0] + k * D;
        prop_put(&n, x, jun[j][1], PROP_LIGHT, angle_to(x, jun[j][1], jun[j][0], jun[j][1]));
      }
  }

  // Street furniture, flora and beach clutter, hashed per tile so it never
  // wanders when the cache rebuilds.
  for (int32_t ty = pty - 9; ty <= pty + 9 && n < MAX_PROP; ty++) {
    for (int32_t tx = ptx - 9; tx <= ptx + 9 && n < MAX_PROP; tx++) {
      uint8_t c = cell_at(tx, ty);
      if (c != CELL_PAVE && c != CELL_PARK && c != CELL_SAND) continue;
      uint32_t hh = hash2(tx, ty, 0xF0E1u);
      if ((hh % (c == CELL_SAND ? 7u : 2u)) != 0) continue;   // beaches stay open

      int32_t wx = tx * TILE + TILE / 2 + (int32_t)(hh % 60) - 30;
      int32_t wy = ty * TILE + TILE / 2 + (int32_t)((hh >> 7) % 60) - 30;
      int32_t du2 = band_dist(wx + wy), dv2 = band_dist(wx - wy);
      int32_t dr = du2 < dv2 ? du2 : dv2;
      // Clear of the tarmac by its own radius, and out of the corner
      // pavements where it would stand in the crosswalk.
      if (dr < ROAD_HALF + 46 || cell_at_point(wx, wy) == CELL_ROAD) continue;
      if (c == CELL_PAVE && du2 < PAVE_HALF + 190 && dv2 < PAVE_HALF + 190) continue;

      // A lamp post faces the street it stands beside.
      int32_t mu = band_off(wx + wy), mv = band_off(wx - wy);
      uint16_t face = (wr_abs(mu) <= wr_abs(mv)) ? (uint16_t)(mu > 0 ? 40960u : 8192u)
                                                 : (uint16_t)(mv > 0 ? 24576u : 57344u);
      uint32_t k = (hh >> 3) % 100;
      uint8_t kind;
      // The island's scenery gets first say: what stands here on THIS
      // island. Zero means the city's usual furniture; a kind at or above
      // PROP_DOMAIN0 is one the domain draws itself.
      int own = domain_call(DOP_PROP, (int)((c << 8) | k));
      if (own > 0)             kind = (uint8_t)own;
      else if (c == CELL_SAND) kind = (k < 42) ? PROP_PALM : (k < 68) ? PROP_ROCK : (k < 86) ? PROP_PARASOL : PROP_BIRD;
      else if (c == CELL_PARK) kind = (k < 62) ? PROP_TREE : (k < 84) ? PROP_ROCK : PROP_BIRD;
      else if (k < 26)         kind = (k < 9) ? PROP_TABLE : PROP_CHAIR;
      else if (k < 45)         kind = PROP_LAMP;
      else if (k < 60)         kind = PROP_SIGN;
      else if (k < 70)         kind = PROP_POSTBOX;
      else if (k < 76)         kind = PROP_BIRD;
      else if (k < 88)         kind = PROP_TREE;
      else                     kind = PROP_HYDRANT;
      prop_put(&n, wx, wy, kind, kind == PROP_LAMP ? face : (uint16_t)(hh >> 13));
    }
  }
  for (int i = n; i < MAX_PROP; i++)
    if (!g_prop[i].loose) g_prop[i].alive = 0;
}

// Weapons lying around the island. Placed deterministically per tile like the
// furniture, so they do not shuffle when the cache rebuilds - but dropped
// weapons live in the same array and are never touched by this.
void pickups_refresh(void) {
  int32_t px = g_p.x, py = g_p.y;

  for (int i = 0; i < MAX_PICKUP; i++)
    if (g_pickup[i].alive && g_pickup[i].world &&
        wr_dist(g_pickup[i].x - px, g_pickup[i].y - py) > 13 * TILE)
      g_pickup[i].alive = 0;

  int32_t ptx = px >> TILE_SH, pty = py >> TILE_SH;
  for (int32_t ty = pty - 8; ty <= pty + 8; ty++) {
    for (int32_t tx = ptx - 8; tx <= ptx + 8; tx++) {
      uint8_t cc = cell_at(tx, ty);
      if (cc != CELL_PAVE && cc != CELL_PARK && cc != CELL_SAND) continue;
      uint32_t hh = hash2(tx, ty, 0x77E4u);
      if ((hh % 40) != 0) continue;              // sparse: a bat now and then, not an armoury

      int32_t wx = ((tx) * TILE) + TILE / 2;
      int32_t wy = ((ty) * TILE) + TILE / 2;

      bool here = false;
      for (int i = 0; i < MAX_PICKUP; i++)
        if (g_pickup[i].alive && wr_dist(g_pickup[i].x - wx, g_pickup[i].y - wy) < 50)
          { here = true; break; }
      if (here) continue;

      for (int i = 0; i < MAX_PICKUP; i++) {
        if (g_pickup[i].alive) continue;
      (void)0;
        g_pickup[i].x = wx; g_pickup[i].y = wy;
        // Guns are not lying about: they come off dead police (pistols,
        // shotguns) and SWAT (machine guns), or over the counter.
        g_pickup[i].weapon = W_BAT;
        g_pickup[i].alive = 1;
        g_pickup[i].world = 1;
        break;
      }
    }
  }
}

// What each kind of furniture throws off when you hit it: splinters, glass,
// painted metal, bark. Same particle physics as blood, different material.
static const uint8_t DEBRIS[PROP_KIND_COUNT] = {
  0xD5, /* lamp    - grey metal   */ 0xDD, /* sign    - white/red  */
  0xC7, /* postbox - blue         */ 0xEC, /* table   - pale wood  */
  0xE4, /* chair   - brown wood   */ 0xF0, /* hydrant - red paint  */
  0xC4, /* tree    - green leaf   */ 0xD8, /* palm    - frond      */
  0xD5, /* rock    - grey stone   */ 0xF5, /* parasol - fabric     */
  0xE4, /* counter - brown wood   */ 0xDB, /* cabinet - glass      */
  0xD5, 0xE4, 0xD5, 0xD5, 0xD5, 0xE4, 0xD5, 0xF2, 0xFF, 0xFF,
};

void prop_hit(int i, int32_t vx, int32_t vy, int32_t force) {
  Prop *p = &g_prop[i];
  if (!p->alive) return;

  uint8_t dmg = (uint8_t)wr_clamp(force, 1, 90);
  uint8_t deb = DEBRIS[p->kind % PROP_KIND_COUNT];
  gib_burst_debris(p->x, p->y, 40, vx, vy, 2, deb);

  if (p->hp > dmg) {
    p->hp -= dmg;
    // Not enough to shear it off, but it leans and springs back.
    p->lean = (int16_t)wr_clamp(p->lean + force * 90, -14000, 14000);
    audio_sfx(p->kind == PROP_CABINET ? SFX_GLASS : SFX_SMASH);
    return;
  }

  // A smashed cabinet spills what was inside it. This is the point of the
  // gun shop: break the glass and help yourself.
  if (p->kind == PROP_CABINET && p->spin > 0) {
    // The glass goes, the gun is on the floor, and that is theft: the
    // stock never comes back and the alarm is ringing.
    static const uint8_t STOCK[4] = { W_PISTOL, W_SHOTGUN, W_FLAME, W_ROCKET };
    int t = p->spin - 1;
    if (!(g_in.took[g_in.venue] & (1 << t))) pickup_drop(p->x, p->y + 40, STOCK[t]);
    g_in.took[g_in.venue] |= (uint8_t)(1 << t);
    inside_violence(false);
    audio_sfx(SFX_GLASS);
  }

  p->hp = 0;
  p->loose = 1;
  p->vx = (int16_t)wr_clamp(vx, -40, 40);
  p->vy = (int16_t)wr_clamp(vy, -40, 40);
  p->vz = (int16_t)(6 + (force >> 2));
  p->spin = (int16_t)(900 + (int32_t)wr_rand_n(4200));
  audio_sfx(SFX_SMASH);
  gib_burst_debris(p->x, p->y, 50, vx, vy, 7, deb);
}

void props_update(void) {
  for (int i = 0; i < MAX_PROP; i++) {
    Prop *p = &g_prop[i];
    if (!p->alive) continue;

    if (!p->loose) {
      // Springs back upright after a glancing knock.
      if (p->lean > 0) p->lean = (int16_t)(p->lean - (p->lean >> 3) - 1);
      else if (p->lean < 0) p->lean = (int16_t)(p->lean - (p->lean >> 3) + 1);
      continue;
    }

    p->x += p->vx;
    p->y += p->vy;
    p->z = (int16_t)(p->z + p->vz);
    p->vz = (int16_t)(p->vz - 3);
    p->ang = (uint16_t)(p->ang + p->spin);
    p->lean = 16384;                       // flat on its side once loose

    if (p->z <= 0) {
      p->z = 0;
      if (p->vz < -7) {
        p->vz = (int16_t)(-p->vz / 3);
        p->vx = (int16_t)((p->vx * 3) / 5);
        p->vy = (int16_t)((p->vy * 3) / 5);
        p->spin = (int16_t)((p->spin * 2) / 3);
      } else {
        p->vz = 0;
        p->vx = (int16_t)((p->vx * 5) / 8);
        p->vy = (int16_t)((p->vy * 5) / 8);
        p->spin = (int16_t)((p->spin * 5) / 8);
        if (wr_abs(p->vx) < 2 && wr_abs(p->vy) < 2) { p->vx = 0; p->vy = 0; p->spin = 0; }
      }
    }
  }
}

// Kept for the old call site; the island has no separate generate step.
void city_generate(uint32_t seed) { (void)seed; s_cache_bx = 0x7FFFFFFF; }
