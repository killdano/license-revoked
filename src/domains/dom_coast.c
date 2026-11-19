#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"
static int dom_dispatch(int op, int arg);
#include "wr_shim.h"

#define C_SHARD ARGB(1,3,3)      // broken shop glass (matches the kernel)
#ifdef WR_PROFILE
extern uint32_t wr_prof_murals;
#endif

#if WR_MONOLITH
extern int32_t s_eye_x, s_eye_y, s_psx, s_psy, s_pdepth;
#endif


// Building materials x face. Face 0 = roof, 1..4 = the four walls. Light is
// fixed, so each material is a hand-picked three-step ramp.
static const uint8_t BLD[6][5] = {   // coast: pastel render, terracotta roofs
  { ARGB(3,3,2), ARGB(2,2,1), ARGB(3,1,0), ARGB(2,2,2), ARGB(1,1,1) },
  { ARGB(3,2,2), ARGB(2,1,1), ARGB(3,1,0), ARGB(2,2,2), ARGB(1,1,1) },
  { ARGB(2,3,3), ARGB(1,2,2), ARGB(2,1,0), ARGB(2,2,2), ARGB(1,1,1) },
  { ARGB(3,3,3), ARGB(2,2,2), ARGB(3,1,0), ARGB(2,2,2), ARGB(1,1,1) },
  { ARGB(3,2,1), ARGB(2,1,0), ARGB(2,1,0), ARGB(2,2,2), ARGB(1,1,1) },
  { ARGB(2,2,3), ARGB(1,1,2), ARGB(3,1,0), ARGB(2,2,2), ARGB(1,1,1) },
};


// ---------------------------------------------------------------------------
// Coast scenery.
//
// A resort. Villas - wide, low, pastel, flat-roofed - with a few hotel
// towers as the only tall things, so the skyline reads as a different place
// from the city. What the lot generator calls a parking garage is a
// swimming pool here: the same lot kind, a different meaning per island.
// You can drive into one.
//
// What a building looks like is the one thing that genuinely differs between
// the islands - towers and parking garages here, villas and pools on the
// coast, hangars and containers on the industrial estate, boarded shopfronts
// in the ghetto. Everything around it is shared and stays in the kernel: the
// roads, the painter's sort, the rasteriser, collision.
//
// Scenery and interiors are mutually exclusive - inside a room the island is
// not drawn, and outside there is no room - so they share one arena. That is
// what makes the scheme pay from the first island rather than the third.
// ---------------------------------------------------------------------------

// Backface test in world space: the walls are axis-aligned planes, so a
// wall faces the camera exactly when the eye is on its outer side. Walls
// are -y, +x, +y, -x in that order.
static bool wall_faces_eye(const Lot *L, int f) {
  switch (f) {
    case 0:  return s_eye_y < L->y0;
    case 1:  return s_eye_x > L->x1;
    case 2:  return s_eye_y > L->y1;
    default: return s_eye_x < L->x0;
  }
}

// A quad on a wall: from t0 to t1 along the face (Q10 of its length),
// between heights z0 and z1. The murals are drawn entirely with this.
static void wall_quad(int32_t ax, int32_t ay, int32_t fx, int32_t fy,
                      int32_t t0, int32_t t1, int32_t z0, int32_t z1, uint8_t col) {
  int32_t q[4][3];
  for (int i = 0; i < 4; i++) {
    int32_t t = (i == 1 || i == 2) ? t1 : t0;
    q[i][0] = ax + (int32_t)(((int64_t)fx * t) >> 10);
    q[i][1] = ay + (int32_t)(((int64_t)fy * t) >> 10);
    q[i][2] = (i < 2) ? z0 : z1;
  }
  gfx_world_poly((const int32_t (*)[3])q, 4, col);
}

// A swimming pool: what the generator calls a garage. A low wall round a
// sunk blue rectangle. Drive in and you are in it.
static void draw_pool(const Lot *L) {
  const int32_t in = 40;
  const int32_t pq[4][3] = {
    { L->x0 + in, L->y0 + in, 2 }, { L->x1 - in, L->y0 + in, 2 },
    { L->x1 - in, L->y1 - in, 2 }, { L->x0 + in, L->y1 - in, 2 },
  };
  gfx_world_poly(pq, 4, ARGB(0,2,3));
  gfx_part((L->x0 + L->x1) / 2, (L->y0 + L->y1) / 2, 0, 0, 0, 0,
           (L->x1 - L->x0) / 2, 14, 18, 0, ARGB(3,3,3), ARGB(2,2,2), ARGB(1,1,1));
}


// The airport terminal from the street: a low white hall with a band of
// glass along the front, a control tower on one corner, and a sign. It is
// wide because it is two lots side by side; it reads from a long way off.
static void draw_terminal(const Lot *L) {
  int32_t cx = (L->x0 + L->x1) / 2, cy = (L->y0 + L->y1) / 2;
  int32_t hl = (L->x1 - L->x0) / 2, hw = (L->y1 - L->y0) / 2;
  gfx_part(cx, cy, 0, 0, 0, 0, hl, hw, 150, 0, ARGB(3,3,3), ARGB(2,2,2), ARGB(1,1,1));          // the hall
  gfx_part(cx, cy, 0, 0, hw - 6, 40, hl - 20, 4, 70, 0, ARGB(1,2,3), ARGB(1,2,3), ARGB(0,1,2));  // glass front
  gfx_part(cx, cy, 0, hl - 40, -hw + 40, 150, 24, 24, 300, 0, ARGB(2,2,2), ARGB(2,2,2), ARGB(1,1,1)); // tower
  gfx_part(cx, cy, 0, 0, hw + 30, 120, 60, 4, 24, 0, ARGB(3,3,0), ARGB(3,3,0), ARGB(2,2,0));    // the sign
}

static void draw_lot(int i) {
  Lot *L = &g_lot[i];
  if (lot_is_airport(L)) { draw_terminal(L); return; }
  if (L->kind == LOT_GARAGE) { draw_pool(L); return; }
  // Villas are one storey; every fifth or so lot keeps the height it was
  // given and is a hotel, the only tall thing on the island.
  bool hotel = L->h > 3 * TILE && ((L->x0 ^ L->y0) & 0x300) == 0x300;
  int32_t h = hotel ? L->h : (L->h > TILE + TILE / 2 ? TILE + TILE / 2 : L->h);
  // Indoors, the wall the camera is beyond is the one between the camera
  // and you - the front wall as you walk in. It is not drawn; the moment
  // you turn to face the door the eye is inside and it is.
  if (g_p.inside) {
    if ((s_eye_y > ROOM_OY + g_in.h && L->y0 >= ROOM_OY + g_in.h) ||
        (s_eye_y < ROOM_OY && L->y1 <= ROOM_OY) ||
        (s_eye_x > ROOM_OX + g_in.w && L->x0 >= ROOM_OX + g_in.w) ||
        (s_eye_x < ROOM_OX && L->x1 <= ROOM_OX)) return;
  }

  // Corner k of the footprint, matching the old t[]/g[] ordering.
  const int32_t kx[4] = { L->x0, L->x1, L->x1, L->x0 };
  const int32_t ky[4] = { L->y0, L->y0, L->y1, L->y1 };

  // Silhouette on screen: the roof quad and the ground quad, clipped. This
  // replaces the old bail-out when any corner crossed the near plane - which
  // made a whole building vanish the moment you drove close past it, and pop
  // straight back a frame later. Now the visible part just gets drawn.
  Proj roof_p[12], gnd_p[12];
  int nroof, ngnd;
  {
    int32_t rq[4][3], gq[4][3];
    for (int k = 0; k < 4; k++) {
      rq[k][0] = kx[k]; rq[k][1] = ky[k]; rq[k][2] = h;
      gq[k][0] = kx[k]; gq[k][1] = ky[k]; gq[k][2] = 0;
    }
    nroof = gfx_clip_world_poly((const int32_t (*)[3])rq, 4, roof_p);
    ngnd  = gfx_clip_world_poly((const int32_t (*)[3])gq, 4, gnd_p);
  }
  // Belt and braces: clear the stipple on entry as well as exit, so a leak
  // from any early return inside the drawing below can never survive into
  // the next lot and checkerboard the whole city.
  gfx_set_stipple(false);

  bool occluding = false;        // set if this lot is standing in your way

  if (!nroof && !ngnd) return;   // fully behind or off screen
#ifdef WR_PROFILE
  { extern uint32_t wr_prof_lots_seen; wr_prof_lots_seen++; }
#endif

  // Hide only what is genuinely between the camera and the player.
  //
  // This used to take the nearest of the four TOP corners as the building's
  // depth - which was wrong, and badly so. Depth falls as height rises, so a
  // tall block standing well in FRONT of the character still reported a tiny
  // depth and got culled. That is why buildings ahead of you kept vanishing.
  // The occlusion depth has to come from the corners on the ground.
  //
  // The window is also tight now: the old test padded the bounding box
  // outward and only asked the building to be 40 units nearer, so almost
  // anything beside you qualified. The bbox is taken from the CLIPPED
  // silhouette, so it works even while a corner of the lot is behind the eye.
  {
    // The CENTRE of the footprint, not a corner.
    //
    // The nearest corner asks "does any part of this lot lie between camera
    // and character", which is true of any long wall running alongside him -
    // it begins behind him and ends in front - so walls you were looking
    // straight at went transparent. The farthest corner is the opposite
    // mistake: a building you stand beside always has one corner past you,
    // so nothing was ever hidden at all. The centre splits them: a wall
    // squarely behind you qualifies, one you are level with does not.
    //
    // Using the nearest corner asks "does any part of this lot lie between
    // the camera and the character", which is true of any long wall running
    // alongside him - it starts behind him and ends in front. Those were
    // being made transparent while you were looking straight at the part in
    // front of you, which is the reported bug: walls ahead going see-through
    // at certain angles. Asking for the farthest corner instead means the
    // WHOLE footprint has to sit between the camera and the character before
    // anything is hidden.
    int32_t near_d = gfx_depth_at((kx[0] + kx[1] + kx[2] + kx[3]) / 4,
                                  (ky[0] + ky[1] + ky[2] + ky[3]) / 4);

#ifdef WR_PROFILE
    extern uint32_t wr_prof_culled;
#endif
    // 120, not 340. The old margin was tuned for a tower across the street
    // and quietly excluded everything close: indoors a wall is often a
    // single tile from you, so the one case where a wall reliably blocks the
    // camera was the one case this never fired on. The real guard is the
    // bounding-box test below - the lot's silhouette has to sit squarely
    // over the character - and the depth margin only needs to establish
    // that the thing is genuinely in front of him rather than behind.
#ifdef WR_PROFILE
    { extern uint32_t wr_prof_occ_depth;
      if (near_d < s_pdepth - 120) wr_prof_occ_depth++; }
#endif
    if (near_d < s_pdepth - 120) {
      int32_t bx0 = 30000, bx1 = -30000, by0 = 30000, by1 = -30000;
      for (int k = 0; k < nroof; k++) {
        if (roof_p[k].sx < bx0) bx0 = roof_p[k].sx;
        if (roof_p[k].sx > bx1) bx1 = roof_p[k].sx;
        if (roof_p[k].sy < by0) by0 = roof_p[k].sy;
        if (roof_p[k].sy > by1) by1 = roof_p[k].sy;
      }
      for (int k = 0; k < ngnd; k++) {
        if (gnd_p[k].sx < bx0) bx0 = gnd_p[k].sx;
        if (gnd_p[k].sx > bx1) bx1 = gnd_p[k].sx;
        if (gnd_p[k].sy > by1) by1 = gnd_p[k].sy;
      }
      // The top edge comes only from the roof - and when the roof is clipped
      // away entirely, `by0` kept its sentinel of 30000 and the test below
      // could never be true for any pixel on screen. That is exactly the
      // case indoors: you stand close to a wall whose top is above the view,
      // the roof polygon is discarded, and the one situation where a wall
      // certainly blocks the camera was the one situation this could not
      // detect. A discarded roof means the wall runs off the top of the
      // screen, so the box is open upward.
      if (!nroof) by0 = -1000;
      // Inset rather than padded, so it has to be squarely over him.
#ifdef WR_PROFILE
      { extern uint32_t wr_prof_occ_bbox;
        if (s_psx >= bx0 && s_psx <= bx1 && s_psy >= by0 && s_psy <= by1)
          wr_prof_occ_bbox++; }
#endif
      if (s_psx >= bx0 + 10 && s_psx <= bx1 - 10 &&
          s_psy >= by0 + 10 && s_psy <= by1 - 4) {
#ifdef WR_PROFILE
        wr_prof_culled++;
#endif
        // Stipple it rather than delete it. A hard cut means a building
        // blinks out of existence the moment it crosses in front of you and
        // blinks back when it clears - which is far more distracting than
        // whatever it was hiding. Drawn as a checkerboard it stays present,
        // you can see your character straight through it, and the transition
        // is a change of texture instead of an object disappearing.
        occluding = true;
      }
    }
  }

  if (occluding) gfx_set_stipple(true);

  uint8_t mat = L->mat % 6;
  static const uint8_t idx[4][2] = { {0,1}, {1,2}, {2,3}, {3,0} };
  // Walls face -y, +x, +y, -x in that order.
  static const uint16_t WALLN[4] = { 49152, 0, 16384, 32768 };
  uint8_t face_col[4];
  for (int f = 0; f < 4; f++) face_col[f] = gfx_shade(WALLN[f], BLD[mat][1], BLD[mat][2]);

  int32_t open_z = 0;
  if (L->kind == LOT_STORE)  open_z = h / 3;

  // One mural per building, on the first wall that actually faces you.

  for (int f = 0; f < 4; f++) {
    int a = idx[f][0], c = idx[f][1];
    if (!wall_faces_eye(L, f)) continue;

    if (open_z) {
      // Upper wall only; the ground floor is either open deck or glass.
      const int32_t u[4][3] = {
        { kx[a], ky[a], h },      { kx[c], ky[c], h },
        { kx[c], ky[c], open_z }, { kx[a], ky[a], open_z },
      };
      gfx_world_poly(u, 4, face_col[f]);

    } else {
      const int32_t w[4][3] = {
        { kx[a], ky[a], h }, { kx[c], ky[c], h },
        { kx[c], ky[c], 0 }, { kx[a], ky[a], 0 },
      };
      gfx_world_poly(w, 4, face_col[f]);
    }

    // Storey banding on the exposed face.
    if (h >= 2 * TILE) {
      uint8_t wc = BLD[mat][4];
      for (int32_t band = open_z + TILE - 74; band < h - 46; band += TILE) {
        const int32_t s[4][3] = {
          { kx[a], ky[a], band + 38 }, { kx[c], ky[c], band + 38 },
          { kx[c], ky[c], band },      { kx[a], ky[a], band },
        };
        gfx_world_poly(s, 4, wc);
      }

      // Vertical glass mullions. Crossed with the horizontal storey bands
      // they read as a window grid, for a handful of tall quads per face
      // instead of a quad per window - the flat single-colour walls were
      // not going to cut it once the core grew towers.
      // A white plinth at the base of every wall: rendered villas.
      {
        const int32_t bq[4][3] = {
          { kx[a], ky[a], 22 }, { kx[c], ky[c], 22 }, { kx[c], ky[c], 0 }, { kx[a], ky[a], 0 },
        };
        gfx_world_poly(bq, 4, ARGB(3,3,3));
      }
    }
  }

  if (nroof >= 3) gfx_fill_poly(roof_p, nroof, BLD[mat][0]);

  // Setbacks. A tower that goes straight up and stops dead is the most
  // obviously generated thing in a skyline; real ones step inward as they
  // rise. Tall lots get one or two crowns stacked on the roof, each inset
  // from the last and tapered slightly, plus a mast on the tallest. It is
  // additive - the walls below are untouched - so it costs a few parts on
  // the handful of buildings big enough to warrant them.
  if (DECOR_SETBACKS && h >= 5 * TILE) {
    int32_t rcx2 = (L->x0 + L->x1) / 2, rcy2 = (L->y0 + L->y1) / 2;
    int32_t hx = (L->x1 - L->x0) / 2, hy = (L->y1 - L->y0) / 2;
    uint32_t sh = (uint32_t)(L->x1 * 2654435761u) ^ (uint32_t)(L->y0 * 40503u);
    int tiers = (h >= 7 * TILE) ? 2 : 1;
    int32_t sz = h;
    for (int t2 = 0; t2 < tiers; t2++) {
      int32_t inset = 22 + t2 * 16;
      int32_t th = (t2 == 0) ? (TILE * 3) / 4 : TILE / 2;
      if (hx - inset < 40 || hy - inset < 40) break;
      gfx_taper(rcx2, rcy2, 0, 0, 0, sz, hx - inset, hy - inset, th,
                232, 232, 0, 0, BLD[mat][0], BLD[mat][1], BLD[mat][2]);
      sz += th;
    }
    // Mast, with a light on it that blinks like an obstruction beacon.
    if (h >= 8 * TILE) {
      gfx_part(rcx2, rcy2, 0, 0, 0, sz, 5, 5, TILE / 2, (uint16_t)(sh & 0x3FFF),
               ARGB(1,1,1), ARGB(1,1,1), ARGB(0,0,0));
      uint8_t bl2 = (g_frame & 16) ? ARGB(3,0,0) : ARGB(1,0,0);
      gfx_part(rcx2, rcy2, 0, 0, 0, sz + TILE / 2, 6, 6, 7, 0, bl2, bl2, bl2);
    }
  }

  // Roof furniture on the bigger blocks: a stair head and a unit, hashed
  // into place, so towers stop reading as extruded rectangles from above.
  if (DECOR_ROOFTOPS && h >= 3 * TILE &&
      (L->x1 - L->x0) > 340 && (L->y1 - L->y0) > 340) {
    int32_t rcx = (L->x0 + L->x1) / 2, rcy = (L->y0 + L->y1) / 2;
    uint32_t hh2 = (uint32_t)((L->x0 * 2654435761u) ^ (uint32_t)(L->y0 * 40503u));
    int32_t oxr = (int32_t)(hh2 % 90) - 45, oyr = (int32_t)((hh2 >> 8) % 90) - 45;
    gfx_part(rcx + oxr, rcy + oyr, 0, 0, 0, h, 46, 34, 42, 0,
             ARGB(1,1,1), ARGB(2,2,2), ARGB(1,1,1));
    gfx_part(rcx - oxr, rcy - oyr, 0, 0, 0, h, 24, 24, 20,
             (uint16_t)(hh2 & 0x3FFF),
             ARGB(2,2,2), ARGB(1,1,1), ARGB(0,0,0));
  }

  // A parking garage has to LOOK like one, or it just reads as a building
  // you can inexplicably drive through - which is exactly how it read. Four
  // things do it: the deck slab you can see under, columns holding it up,
  // painted bays on the floor, and cars parked in them. The cars are real
  // obstacles (city_garage_slots() is the same function collision uses), so
  // the way through is the aisle down the middle.
  if (L->kind == LOT_GUNSHOP || L->kind == LOT_CLUB) {
    // Sign over the door, and the door, in the door's own frame: +x is
    // outward from the wall. Red and white for the store; magenta with a
    // blinking pink neon band for the club.
    static const PartRec SHOPP[] = {
      BOX(8, 0, 0, 8, 90, 44, 0, 0, 1),
      BOX(14, 0, 10, 4, 62, 22, 2, 2, 3),
    };
    bool club = (L->kind == LOT_CLUB);
    uint8_t pal[4] = { club ? ARGB(3,0,3) : ARGB(3,0,0), club ? ARGB(2,0,2) : ARGB(2,0,0),
                       club ? ((g_frame & 8) ? ARGB(3,1,3) : ARGB(3,3,3)) : W3, G2 };
    int32_t ddx, ddy, dnx, dny;
    city_shop_door(L, &ddx, &ddy, &dnx, &dny);
    uint16_t out = (dnx > 0) ? 0 : (dnx < 0) ? 32768 : (dny > 0) ? 16384 : 49152;
    gfx_parts(ddx, ddy, out, (h * 2) / 3, 0, SHOPP, 2, 2, pal);
    gfx_part(ddx, ddy, out, -6, 0, 0, 6, 56, 116, 0,
             club ? ARGB(1,0,1) : ARGB(1,1,0), club ? ARGB(2,0,2) : ARGB(2,1,0), club ? ARGB(1,0,1) : ARGB(1,1,0));
  }

  for (int f = 0; f < 4; f++) {
    int a = idx[f][0], c = idx[f][1];
    if (!wall_faces_eye(L, f)) continue;
    const int32_t lip[4][3] = {
      { kx[a], ky[a], h + 26 }, { kx[c], ky[c], h + 26 },
      { kx[c], ky[c], h },      { kx[a], ky[a], h },
    };
    gfx_world_poly(lip, 4, BLD[mat][0]);
  }
  gfx_set_stipple(false);      // never leaks into the next draw call
}
#include "bikehit.inc"

static int dom_dispatch(int op, int arg) {
  switch (op) {
    case DOP_BIKE_HIT: bike_hit(arg & 0xFF, (arg >> 8) & 0xFF, (arg >> 16) & 0xFF); return 1;
    case DOP_DRAW_LOT: draw_lot(arg); return 1;
    default: return 0;
  }
}

DOMAIN_ENTRY(dom_coast_entry)
