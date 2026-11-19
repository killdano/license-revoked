#include "wr.h"
#include "wr_domain.h"
// See wr_ent.c: kept out of line so the renderer has parts. render_frame was
// 8,644 bytes with the terrain, the roads and the draw list all folded in.
#define WR_PART __attribute__((noinline))
#include "wr_gfx.h"

uint32_t g_frame = 0;

// ---------------------------------------------------------------------------
// Ground and architecture palette
// ---------------------------------------------------------------------------

#define C_ROAD     ARGB(1,1,1)
#define C_LINE     ARGB(3,3,1)
#define C_PAVE     ARGB(2,2,2)
#define C_FOAM     ARGB(3,3,3)   // surf where the sea meets the sand
#define C_LINE_W   ARGB(3,3,3)   // white paint: stop bars, crosswalks
#define C_LOT      ARGB(2,2,1)   // buildable ground, so gaps do not read as road
#define C_PARK     ARGB(0,2,0)
#define HUD_H 228
#define HUD_W (SCR_W * 228 / SCR_H)
#define L(v)  ((v) * 228 / SCR_H)
uint8_t g_park_col = C_PARK;
uint8_t g_lot_col = C_LOT;
#define C_SAND     ARGB(3,3,1)
#define C_WATER    ARGB(0,1,3)
#define C_WATER_D  ARGB(0,0,2)
#define C_GARAGE   ARGB(1,1,1)
#define C_SHARD    ARGB(1,3,3)
#define C_BLOOD    ARGB(2,0,0)
#define C_BLOOD_D  ARGB(1,0,0)
#define C_MUZZLE   ARGB(3,3,0)
#define C_SKY      ARGB(1,2,3)
#define C_SKY_LOW  ARGB(2,3,3)
#define C_HAZE     ARGB(1,1,1)
#define C_TRACER   ARGB(3,2,0)

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------

int32_t g_cam_cp = 711, g_cam_sp = 737;
int32_t g_proj_cy = PLAYER_Y_FAR;
int32_t g_horizon = -1000;
// Which shoulder the camera looks over. Everything about the framing is
// mirrored through this: the projection centre, the yaw bias, the zoom
// strip and the stick's resting corner.
static int32_t s_proj_cx;           // set every frame below; the width may be a run-time value on a phone

static int32_t s_cam_x, s_cam_y;
int32_t s_eye_x, s_eye_y;   // eye position on the ground plane
static int32_t s_cyaw, s_syaw;
static int32_t s_cam_back = CAM_BACK_NEAR;
static int32_t s_cam_h    = CAM_H_NEAR;
static int32_t s_zoom_t;                 // eased 0..256

// Two things move the camera: the zoom you dialled in, and how fast you are
// going. Speed only ever pushes it further out, never pulls it in, so it can
// never fight a deliberate zoom.
//
// Pitch rides the same slider. Close in the camera looks up the street; far
// out it tips toward straight overhead. Locking pitch to zoom is what makes
// both ends of the slider useful - a close top-down view shows you nothing,
// and a distant low view is all rooftops.
static void update_zoom(void) {
  int32_t spd = wr_abs(g_p.spd);
  int32_t auto_t = (spd * spd * 200) / (30 * 30);
  // Indoors there is nothing to look ahead at, so pull right in.
  if (g_p.jump) auto_t += 110;                    // pull back for the jump: straight on, the trick shows
  int32_t want = g_p.inside ? (g_p.inside == IN_CLUB ? 30 : 16) : (g_p.zoom + auto_t);
  // Air support is the one thing that lives above the frame. Pull out so
  // the helicopter is in it, and further still when you are the one flying.
  if (g_p.in_car && g_ent[g_p.in_car - 1].type == ENT_HELI) want += 120;
  else if (g_heli_up && want < 150) want = (want + 150) / 2 + 40;
  // Death and arrest get a cinematic: hold a close-up on the body, then a
  // slow pull up and away while the caption sits on screen. The easing
  // below does the smoothing; this just moves the target.
  if (g_p.dead) {
    want = (g_p.cine < 30) ? 0 : (g_p.cine - 30) * 2;
    if (want > ZOOM_MAX) want = ZOOM_MAX;
  }
  if (want > ZOOM_MAX) want = ZOOM_MAX;
  if (want < 0) want = 0;

  int32_t dz = want - s_zoom_t;
  s_zoom_t += (dz >= 0) ? (dz >> CAM_EASE) : -((-dz) >> CAM_EASE);
  if (wr_abs(want - s_zoom_t) <= 2) s_zoom_t = want;   // actually arrive

  int32_t back_t = CAM_BACK_NEAR + (((CAM_BACK_FAR - CAM_BACK_NEAR) * s_zoom_t) >> 8);
  int32_t h_t    = CAM_H_NEAR    + (((CAM_H_FAR    - CAM_H_NEAR)    * s_zoom_t) >> 8);
  int32_t db = back_t - s_cam_back;
  int32_t dh = h_t - s_cam_h;
  s_cam_back += (db >= 0) ? (db >> CAM_EASE) : -((-db) >> CAM_EASE);
  s_cam_h    += (dh >= 0) ? (dh >> CAM_EASE) : -((-dh) >> CAM_EASE);
  if (wr_abs(back_t - s_cam_back) <= 2) s_cam_back = back_t;
  if (wr_abs(h_t - s_cam_h) <= 2)       s_cam_h = h_t;

  int32_t deg = PITCH_NEAR + (((PITCH_FAR - PITCH_NEAR) * (s_zoom_t > 256 ? 256 : s_zoom_t)) >> 8);
  uint16_t a = (uint16_t)(deg * 182);
  g_cam_cp = fcos(a);
  g_cam_sp = fsin(a);

  // Solve for the projection centre that pins the character to a fixed spot on
  // screen. Without this the pitch slide drags him from below the bottom edge
  // when zoomed in to near the top when zoomed out - he has to stay put while
  // the world tilts around him, not the other way round.
  int32_t depth = (s_cam_back * g_cam_cp + s_cam_h * g_cam_sp) >> 10;
  if (depth < NEAR_Z) depth = NEAR_Z;
  int32_t up = (s_cam_back * g_cam_sp - s_cam_h * g_cam_cp) >> 10;
  int32_t target = PLAYER_Y_NEAR +
                   (((PLAYER_Y_FAR - PLAYER_Y_NEAR) * s_zoom_t) >> 8);
  g_proj_cy = target + (up * FOCAL) / depth;

  // The ground plane vanishes at tan(pitch) above the view axis. Below the
  // camera pitch of about 20 degrees this lands on screen, which is when the
  // sky starts to matter.
  g_horizon = g_proj_cy - (FOCAL * g_cam_sp) / g_cam_cp;
}

Proj gfx_project(int32_t wx, int32_t wy, int32_t wz) {
  Proj o;
  int32_t dx = wx - s_cam_x, dy = wy - s_cam_y;
  int32_t ry = (dx * s_cyaw + dy * s_syaw) >> 10;   // forward
  int32_t rx = (dy * s_cyaw - dx * s_syaw) >> 10;   // right

  int32_t ry2 = ry + s_cam_back;
  int32_t rz2 = wz - s_cam_h;

  int32_t depth = (ry2 * g_cam_cp - rz2 * g_cam_sp) >> 10;
  if (depth < NEAR_Z) { o.ok = 0; o.depth = NEAR_Z; o.sx = 0; o.sy = 0; return o; }
  int32_t up = (ry2 * g_cam_sp + rz2 * g_cam_cp) >> 10;

  o.sx = (int16_t)wr_clamp(s_proj_cx + (rx * FOCAL) / depth, -2000, 2000);
  o.sy = (int16_t)wr_clamp(g_proj_cy - (up * FOCAL) / depth, -2000, 2000);
  o.depth = depth;
  o.ok = 1;
  return o;
}

int32_t gfx_depth_at(int32_t wx, int32_t wy) {
  int32_t dx = wx - s_cam_x, dy = wy - s_cam_y;
  int32_t ry = (dx * s_cyaw + dy * s_syaw) >> 10;
  return ((ry + s_cam_back) * g_cam_cp + s_cam_h * g_cam_sp) >> 10;
}

// ---------------------------------------------------------------------------
// Convex polygon fill, straight into the framebuffer
// ---------------------------------------------------------------------------

static GBitmap *s_fb;
int32_t s_psx, s_psy;      // player position on screen
int32_t s_pdepth;          // and how far away he is

#ifdef WR_PROFILE
int wr_prof_murals;
#endif

#ifdef WR_PROFILE
uint32_t wr_prof_polys, wr_prof_pixels, wr_prof_items, wr_prof_culled;
uint32_t wr_prof_lots_seen, wr_prof_occ_depth, wr_prof_occ_bbox;
uint32_t wr_px_terrain, wr_px_street, wr_px_lots, wr_px_ents;
static uint32_t *s_px_bucket;
#define PROF_POLY()  (wr_prof_polys++)
#define PROF_PIX(n)  (wr_prof_pixels += (uint32_t)(n))
#else
#define PROF_POLY()  ((void)0)
#define PROF_PIX(n)  ((void)0)
#endif

// When set, fills lay down a checkerboard instead of a solid span. One flag
// rather than a second rasteriser, because the only thing that ever wants it
// is the building standing between you and the camera.
static bool s_stipple;
void gfx_set_stipple(bool on) { s_stipple = on; }

void gfx_fill_poly(const Proj *p, int n, uint8_t col) {
  int32_t ymin = p[0].sy, ymax = p[0].sy;
  int32_t xmin = p[0].sx, xmax = p[0].sx;
  for (int i = 1; i < n; i++) {
    if (p[i].sy < ymin) ymin = p[i].sy;
    if (p[i].sy > ymax) ymax = p[i].sy;
    if (p[i].sx < xmin) xmin = p[i].sx;
    if (p[i].sx > xmax) xmax = p[i].sx;
  }
  if (ymax < 0 || ymin >= SCR_H || xmax < 0 || xmin >= SCR_W) return;
  PROF_POLY();
  if (ymin < 0) ymin = 0;
  if (ymax > SCR_H - 1) ymax = SCR_H - 1;

  // Precompute each edge once. The inner loop used to do a divide per edge per
  // scanline; a tall polygon spanning most of the screen meant hundreds of
  // them, and with the street bands running to the horizon that was the single
  // biggest cost in the frame. Slope is 16.16, so stepping down a scanline is
  // a multiply and a shift.
  struct { int32_t ya, yb, xa, slope; } ed[12];
  int ne = 0;
  for (int i = 0; i < n && ne < 12; i++) {
    int j = (i + 1 == n) ? 0 : i + 1;
    int32_t ya = p[i].sy, yb = p[j].sy, xa = p[i].sx, xb = p[j].sx;
    if (ya == yb) continue;
    if (ya > yb) { int32_t t = ya; ya = yb; yb = t; t = xa; xa = xb; xb = t; }
    ed[ne].ya = ya;
    ed[ne].yb = yb;
    ed[ne].xa = xa;
    ed[ne].slope = ((xb - xa) * 65536) / (yb - ya);
    ne++;
  }

  for (int32_t y = ymin; y <= ymax; y++) {
    int32_t xl = 30000, xr = -30000;
    for (int i = 0; i < ne; i++) {
      if (y < ed[i].ya || y >= ed[i].yb) continue;
      int32_t x = ed[i].xa + ((ed[i].slope * (y - ed[i].ya)) >> 16);
      if (x < xl) xl = x;
      if (x > xr) xr = x;
    }
    if (xr < xl) continue;

    GBitmapDataRowInfo row = gbitmap_get_data_row_info(s_fb, (uint16_t)y);
    if (xl < row.min_x) xl = row.min_x;
    if (xr > row.max_x) xr = row.max_x;
    if (xr < xl) continue;
    PROF_PIX(xr - xl + 1);
#ifdef WR_PROFILE
    if (s_px_bucket) *s_px_bucket += (uint32_t)(xr - xl + 1);
#endif
    if (s_stipple) {
      // Every other pixel, on a checkerboard keyed to screen position so the
      // pattern is stable as the world moves under it. This is how an
      // occluding building is drawn instead of being deleted outright.
      for (int32_t x = xl; x <= xr; x++)
        if (((x + y) & 1) == 0) row.data[x] = col;
    } else {
      memset(row.data + xl, col, (size_t)(xr - xl + 1));
    }
  }
}

// When set, fills lay down a checkerboard instead of a solid span. One flag
// rather than a second rasteriser, because the only thing that ever wants
// it is the building standing between you and the camera.
// Screen-space signed area, used for backface culling.
int32_t gfx_area2(const Proj *p, int n) {
  int32_t a = 0;
  for (int i = 0; i < n; i++) {
    int j = (i + 1 == n) ? 0 : i + 1;
    a += (int32_t)p[i].sx * p[j].sy - (int32_t)p[j].sx * p[i].sy;
  }
  return a;
}

bool gfx_all_ok(const Proj *p, int n) {
  for (int i = 0; i < n; i++) if (!p[i].ok) return false;
  return true;
}

// ---------------------------------------------------------------------------
// World-space polygons, clipped for real.
//
// gfx_project() handles a vertex behind the eye by refusing (ok = 0) and a
// vertex projecting far off screen by clamping to +/-2000. Both answers are
// wrong for big geometry. Dropping a quad because one corner crossed the near
// plane is why buildings and road under your feet used to vanish as you drove
// past; clamping is worse - moving a vertex bends both edges that meet at it,
// so a street band passing beside the camera (true screen x in the tens of
// thousands) turned into a wedge sweeping across the frame, and it swung to a
// new shape every time the camera turned. That was the flicker.
//
// The fix is textbook: clip against the near plane in CAMERA space, where the
// interpolation is exact, project without clamping (coordinates are large but
// finite once nothing is behind the eye - projection maps straight edges to
// straight edges), then clip the projected polygon to a guard box just larger
// than the screen so the rasterizer only ever sees small coordinates.
// ---------------------------------------------------------------------------

typedef struct { int32_t rx, up, d; } CamV;
typedef struct { int32_t x, y; } Pt2;

#define GUARD 32   // screen clip box overhang, px

static CamV cam_v(int32_t wx, int32_t wy, int32_t wz) {
  CamV o;
  int32_t dx = wx - s_cam_x, dy = wy - s_cam_y;
  int32_t ry = (dx * s_cyaw + dy * s_syaw) >> 10;
  o.rx = (dy * s_cyaw - dx * s_syaw) >> 10;
  int32_t ry2 = ry + s_cam_back;
  int32_t rz2 = wz - s_cam_h;
  o.d  = (ry2 * g_cam_cp - rz2 * g_cam_sp) >> 10;
  o.up = (ry2 * g_cam_sp + rz2 * g_cam_cp) >> 10;
  return o;
}

static int clip_near_plane(const CamV *in, int n, CamV *out) {
  int m = 0;
  for (int i = 0; i < n; i++) {
    const CamV *a = &in[i];
    const CamV *b = &in[(i + 1 == n) ? 0 : i + 1];
    int ain = a->d >= NEAR_Z, bin = b->d >= NEAR_Z;
    if (ain) out[m++] = *a;
    if (ain != bin) {
      // d is affine along the edge, so this intersection is exact.
      // Multiply, not shift: the numerator is negative when a is the inside
      // vertex, and left-shifting a negative value is UB.
      int32_t t = wr_muldiv(NEAR_Z - a->d, 65536, b->d - a->d);
      CamV c;
      c.rx = a->rx + wr_muldiv(b->rx - a->rx, t, 65536);
      c.up = a->up + wr_muldiv(b->up - a->up, t, 65536);
      c.d  = NEAR_Z;
      out[m++] = c;
    }
  }
  return m;
}

// One Sutherland-Hodgman pass against an axis-aligned half-plane.
static int clip_half(const Pt2 *in, int n, Pt2 *out,
                     int axis, int32_t lim, int keep_ge) {
  int m = 0;
  for (int i = 0; i < n; i++) {
    const Pt2 *a = &in[i];
    const Pt2 *b = &in[(i + 1 == n) ? 0 : i + 1];
    int32_t av = axis ? a->y : a->x, bv = axis ? b->y : b->x;
    int ain = keep_ge ? (av >= lim) : (av <= lim);
    int bin = keep_ge ? (bv >= lim) : (bv <= lim);
    if (ain) out[m++] = *a;
    if (ain != bin) {
      Pt2 c;
      int32_t den = bv - av;               // nonzero: the sides differ
      if (axis) {
        c.y = lim;
        c.x = a->x + wr_muldiv(b->x - a->x, lim - av, den);
      } else {
        c.x = lim;
        c.y = a->y + wr_muldiv(b->y - a->y, lim - av, den);
      }
      out[m++] = c;
    }
  }
  return m;
}

// Clip a world polygon and hand back screen-space vertices ready to fill.
// Returns the vertex count, 0 if nothing survives. n <= 8 in; out holds 12.
int gfx_clip_world_poly(const int32_t (*w)[3], int n, Proj *out) {
  CamV cv[8], cn[12];
  int behind = 0;
  for (int i = 0; i < n; i++) {
    cv[i] = cam_v(w[i][0], w[i][1], w[i][2]);
    if (cv[i].d < NEAR_Z) behind++;
  }
  const CamV *src = cv;
  int m = n;
  if (behind == n) return 0;
  if (behind) {
    m = clip_near_plane(cv, n, cn);
    src = cn;
    if (m < 3) return 0;
  }

  Pt2 pa[12], pb[12];
  for (int i = 0; i < m; i++) {
    pa[i].x = s_proj_cx + (src[i].rx * FOCAL) / src[i].d;
    pa[i].y = g_proj_cy - (src[i].up * FOCAL) / src[i].d;
  }
  m = clip_half(pa, m, pb, 0, -GUARD, 1);          if (m < 3) return 0;
  m = clip_half(pb, m, pa, 0, SCR_W + GUARD, 0);   if (m < 3) return 0;
  m = clip_half(pa, m, pb, 1, -GUARD, 1);          if (m < 3) return 0;
  m = clip_half(pb, m, pa, 1, SCR_H + GUARD, 0);   if (m < 3) return 0;

  for (int i = 0; i < m; i++) {
    out[i].sx = (int16_t)pa[i].x;
    out[i].sy = (int16_t)pa[i].y;
    out[i].depth = NEAR_Z;
    out[i].ok = 1;
  }
  return m;
}

void gfx_world_poly(const int32_t (*w)[3], int n, uint8_t col) {
  Proj q[12];
  int m = gfx_clip_world_poly(w, n, q);
  if (m >= 3) gfx_fill_poly(q, m, col);
}

// A wheel drawn as a wheel. The old one was a thin box, which from the flank
// is exactly the "two silly triangles" it looked like. This is an octagonal
// tyre disc with a lighter hub, standing in the vertical plane along the
// car's axis, and only the flank facing the camera gets one - from the far
// side the tread box already does the silhouette. Two polygons per visible
// wheel.
void gfx_wheel_disc(int32_t cx, int32_t cy, uint16_t ang,
                    int32_t ox, int32_t oy, int32_t cz, int32_t r,
                    uint8_t tyre, uint8_t hub) {
  int32_t c = fcos(ang), s = fsin(ang);
  int32_t px = cx + ((ox * c - oy * s) >> 10);
  int32_t py = cy + ((ox * s + oy * c) >> 10);
  int32_t qx = cx + ((ox * c) >> 10), qy = cy + ((ox * s) >> 10);
  if (gfx_depth_at(px, py) >= gfx_depth_at(qx, qy)) return;   // far flank

  static const int16_t OCT[8][2] = {           // Q8 cos/sin, 45-degree steps
    { 256, 0 }, { 181, 181 }, { 0, 256 }, { -181, 181 },
    { -256, 0 }, { -181, -181 }, { 0, -256 }, { 181, -181 },
  };
  int32_t w[8][3], hb[8][3];
  for (int i = 0; i < 8; i++) {
    int32_t lx = ox + (r * OCT[i][0]) / 256;
    int32_t lz = cz + (r * OCT[i][1]) / 256;
    w[i][0] = cx + ((lx * c - oy * s) >> 10);
    w[i][1] = cy + ((lx * s + oy * c) >> 10);
    w[i][2] = lz;
    lx = ox + ((r / 2) * OCT[i][0]) / 256;
    lz = cz + ((r / 2) * OCT[i][1]) / 256;
    hb[i][0] = cx + ((lx * c - oy * s) >> 10);
    hb[i][1] = cy + ((lx * s + oy * c) >> 10);
    hb[i][2] = lz;
  }
  gfx_world_poly((const int32_t (*)[3])w, 8, tyre);
  gfx_world_poly((const int32_t (*)[3])hb, 8, hub);
}

void gfx_ground_quad(int32_t cx, int32_t cy, int32_t half, uint8_t col) {
  const int32_t w[4][3] = {
    { cx - half, cy - half, 0 }, { cx + half, cy - half, 0 },
    { cx + half, cy + half, 0 }, { cx - half, cy + half, 0 },
  };
  gfx_world_poly(w, 4, col);
}

void gfx_ground_band(int32_t cx, int32_t cy, uint16_t ang,
                     int32_t half_len, int32_t half_wid, uint8_t col) {
  int32_t c = fcos(ang), s = fsin(ang);

  // A street is thousands of units long, so at a low camera pitch one end of
  // it is behind the camera. Projecting a vertex that is behind the eye gives
  // nonsense - which is why the bands appeared to slew around the moment you
  // turned, and vanished entirely when a corner crossed over. Clip the span
  // to the part that is actually in front before projecting anything.
  int32_t t0 = -half_len, t1 = half_len;
  int32_t d0 = gfx_depth_at(cx + ((t0 * c) >> 10), cy + ((t0 * s) >> 10));
  int32_t d1 = gfx_depth_at(cx + ((t1 * c) >> 10), cy + ((t1 * s) >> 10));

  // This is only a COARSE bound now, to keep the far end's coordinates sane
  // and throw away bands entirely behind the camera. It deliberately keeps a
  // margin of geometry behind the eye - the exact near-plane cut happens per
  // corner inside gfx_world_poly(), where the interpolation is done in camera
  // space and cannot punch a hole in the road under your feet. The previous
  // version tried to be exact here by estimating how much the corners differ
  // from the centreline, and both of its failure modes (holes when the
  // estimate was too big, screen-crossing wedges via the projection clamp
  // when it was too small) are gone with the estimate itself.
  const int32_t lim = -(8 * TILE);
  if (d0 < lim && d1 < lim) return;
  // 64-bit intermediates: a street is tens of thousands of units long and a
  // point well behind the eye has a large negative depth, so the naive
  // product overflows int32 and the clip lands somewhere absurd.
  if (d0 < lim)
    t0 = t0 + wr_muldiv(t1 - t0, lim - d0, d1 - d0);
  if (d1 < lim)
    t1 = t1 + wr_muldiv(t0 - t1, lim - d1, d0 - d1);
  if (t1 - t0 < 16) return;

  const int32_t tt[4] = { t1, t1, t0, t0 };
  static const int32_t sgy[4] = { -1, 1, 1, -1 };
  int32_t w[4][3];
  for (int i = 0; i < 4; i++) {
    int32_t lx = tt[i], ly = sgy[i] * half_wid;
    w[i][0] = cx + ((lx * c - ly * s) >> 10);
    w[i][1] = cy + ((lx * s + ly * c) >> 10);
    w[i][2] = 0;
  }
  gfx_world_poly((const int32_t (*)[3])w, 4, col);
}

// ---------------------------------------------------------------------------
// The workhorse: one box in an entity's local frame
// ---------------------------------------------------------------------------

// A box is a frustum whose top is the same size as its bottom. This used to
// be a full copy of gfx_taper() below - the same projection, the same four
// side quads, the same cap - differing only in that the top corners were not
// scaled. Two copies of a routine called 134 times across the models is
// nearly a kilobyte of duplicated code, which matters when the image ceiling
// is 65535 bytes and the last four rounds were fought over tens of them.
void gfx_part(int32_t cx, int32_t cy, uint16_t ang,
              int32_t ox, int32_t oy, int32_t oz,
              int32_t hl, int32_t hw, int32_t h,
              uint16_t dang,
              uint8_t top, uint8_t bright, uint8_t dark) {
  gfx_taper(cx, cy, ang, ox, oy, oz, hl, hw, h, 256, 256, 0, dang,
            top, bright, dark);
}

// A box whose top face is a different size from its bottom - a frustum. It
// costs exactly what gfx_part() costs (four sides and a cap) but it is the
// single cheapest cure for how blocky everything looks: a car roof can slope
// back, a head can narrow at the crown, a tree can taper to its canopy, a
// cone can actually be conical. `tl`/`tw` are the top half-extents as
// 8.8 fractions of the bottom, so 256 is a plain box, 128 is half the size
// at the top, and 320 flares outward.
// ---------------------------------------------------------------------------
// Light.
//
// Every side face used to get its colour from its INDEX - front and left
// bright, back and right dark - regardless of where it actually pointed,
// which is why a box lit from any angle read as two flat planes and a cap.
// The colour now comes from the angle between the face's normal and one
// light. The light rides the camera, from behind the eye and off to the
// left, so the two faces you can see are always shaded differently and
// everything reads as solid whichever way it is turned. Four steps, not
// three: the two given colours, one lifted a notch and one dropped a notch.
// ---------------------------------------------------------------------------
uint16_t g_light;

// A notch up lifts the dominant channels (navy gets bluer, not greyer);
// once they are at full it lifts the others (red gets a pink sheen). A
// notch down lowers whatever is lit.
static uint8_t col_step(uint8_t c, int d) {
  int v[3] = { (c >> 4) & 3, (c >> 2) & 3, c & 3 };
  int mx = v[0] > v[1] ? v[0] : v[1]; if (v[2] > mx) mx = v[2];
  for (int i = 0; i < 3; i++) {
    if (d > 0) { if (mx == 3 ? v[i] < 3 : v[i] == mx) v[i]++; }
    else if (v[i] > 0) v[i]--;
  }
  return (uint8_t)(0xC0 | (v[0] << 4) | (v[1] << 2) | v[2]);
}

uint8_t gfx_shade(uint16_t normal, uint8_t bright, uint8_t dark) {
  int32_t k = fcos((uint16_t)(normal - g_light));
  if (k >  560) return col_step(bright, 1);
  if (k > -200) return bright;
  if (k > -760) return dark;
  return col_step(dark, -1);
}

// Was this point inside the last frame? For spawning things where the
// camera is not looking.
static uint32_t s_last_render;
bool gfx_in_view(int32_t x, int32_t y) {
  if (g_frame - s_last_render > 2) return false;    // no recent frame: no opinion
  Proj p = gfx_project(x, y, 0);
  return p.ok && p.sx >= -20 && p.sx < SCR_W + 20 && p.sy >= -20 && p.sy < SCR_H + 20;
}

void gfx_parts(int32_t x, int32_t y, uint16_t ang, int32_t z, int32_t lean,
               const PartRec *p, int n, int scale, const uint8_t *pal) {
  for (; n > 0; n--, p++) {
    uint8_t c[3];
    for (int k = 0; k < 3; k++) c[k] = (p->c[k] < 0x10) ? pal[p->c[k]] : p->c[k];
    gfx_taper(x, y, ang, p->ox * scale + lean, p->oy * scale, z + p->oz * scale,
              p->hl * scale, p->hw * scale, p->h * scale, p->tl * 4, p->tw * 4,
              p->ts * scale, (uint16_t)(p->dang << 8), c[0], c[1], c[2]);
  }
}

void gfx_taper(int32_t cx, int32_t cy, uint16_t ang,
               int32_t ox, int32_t oy, int32_t oz,
               int32_t hl, int32_t hw, int32_t h,
               int32_t tl, int32_t tw, int32_t tshift,
               uint16_t dang,
               uint8_t top, uint8_t bright, uint8_t dark) {
  int32_t c = fcos(ang), s = fsin(ang);
  uint16_t a2 = (uint16_t)(ang + dang);
  int32_t c2 = fcos(a2), s2 = fsin(a2);

  int32_t px = cx + ((ox * c - oy * s) >> 10);
  int32_t py = cy + ((ox * s + oy * c) >> 10);

  static const int32_t sgx[4] = {  1,  1, -1, -1 };
  static const int32_t sgy[4] = { -1,  1,  1, -1 };

  Proj t[4], g[4];
  for (int i = 0; i < 4; i++) {
    int32_t bx = sgx[i] * hl,          by = sgy[i] * hw;
    int32_t ux = sgx[i] * ((hl * tl) >> 8) + tshift;
    int32_t uy = sgy[i] * ((hw * tw) >> 8);
    int32_t wxb = px + ((bx * c2 - by * s2) >> 10);
    int32_t wyb = py + ((bx * s2 + by * c2) >> 10);
    int32_t wxt = px + ((ux * c2 - uy * s2) >> 10);
    int32_t wyt = py + ((ux * s2 + uy * c2) >> 10);
    t[i] = gfx_project(wxt, wyt, oz + h);
    g[i] = gfx_project(wxb, wyb, oz);
  }
  if (!gfx_all_ok(t, 4) || !gfx_all_ok(g, 4)) return;

  static const uint8_t idx2[4][2] = { {0,1}, {1,2}, {2,3}, {3,0} };
  for (int f = 0; f < 4; f++) {
    int a = idx2[f][0], b = idx2[f][1];
    Proj w[4] = { t[a], t[b], g[b], g[a] };
    if (gfx_area2(w, 4) <= 0) continue;
    gfx_fill_poly(w, 4, gfx_shade((uint16_t)(a2 + f * 16384), bright, dark));
  }
  gfx_fill_poly(t, 4, top);
}

// Integer to decimal, and a string copy, both bounded. These exist so the
// HUD can show a number without linking the C formatting machinery: this
// game deliberately has no snprintf and no float formatting in it at all.
int gfx_utoa(char *dst, int cap, uint32_t v) {
  char tmp[12];
  int n = 0;
  do { tmp[n++] = (char)('0' + (v % 10u)); v /= 10u; } while (v && n < 11);
  if (n > cap) n = cap;
  for (int i = 0; i < n; i++) dst[i] = tmp[n - 1 - i];
  return n;
}

int gfx_puts(char *dst, int cap, const char *src) {
  int n = 0;
  while (src[n] && n < cap) { dst[n] = src[n]; n++; }
  return n;
}

// ---------------------------------------------------------------------------
// City
// ---------------------------------------------------------------------------

WR_PART static void draw_ground_tile(int32_t tx, int32_t ty, uint8_t col) {
  int32_t x0 = tx * TILE, y0 = ty * TILE;   // tx/ty go negative; shifting would be UB
  const int32_t w[4][3] = {
    { x0,        y0,        0 }, { x0 + TILE, y0,        0 },
    { x0 + TILE, y0 + TILE, 0 }, { x0,        y0 + TILE, 0 },
  };
  gfx_world_poly(w, 4, col);
}

// One lot, one box. Because a lot spans several tiles there are no interior
// party walls to rasterise and no near-identical depths for the painter sort
// to swap between frames - which is what was making blocks flicker and look
// like they were missing walls.


// ---------------------------------------------------------------------------
// Draw list
// ---------------------------------------------------------------------------

typedef struct { int32_t depth; uint8_t kind; uint8_t a, b; } Item;
enum { IT_LOT = 0, IT_ENT, IT_PICKUP, IT_PLAYER, IT_BOOM, IT_PROP };

// Lots + entities + props + pickups + explosions + the player can add up to
// well over a hundred. This used to cap at 110, and push_item() silently
// dropped everything past it - so on a busy street the furthest buildings and
// whatever happened to be pushed last just stopped being drawn.
// Sized above the computed worst case in systems.c (164), not below it -
// the first attempt at trimming RAM set this to 160 and the test caught the
// four-item overflow immediately. An overflowing draw list does not crash,
// it silently stops drawing things, which is the worst kind of bug.
#define MAX_ITEM 124
static Item s_item[MAX_ITEM];

// Exposed so the overflow test can check the real cap rather than a number
// copied into the test and left to drift.
int wr_draw_capacity(void) { return MAX_ITEM; }
static int s_nitem;

static void push_item(int32_t depth, uint8_t kind, uint8_t a, uint8_t b) {
  if (s_nitem >= MAX_ITEM) return;
  s_item[s_nitem].depth = depth;
  s_item[s_nitem].kind = kind;
  s_item[s_nitem].a = a;
  s_item[s_nitem].b = b;
  s_nitem++;
}

// Insertion sort, far to near. Nearly sorted frame to frame, so close to
// linear in practice.
static void sort_items(void) {
  for (int i = 1; i < s_nitem; i++) {
    Item key = s_item[i];
    int j = i - 1;
    while (j >= 0 && s_item[j].depth < key.depth) { s_item[j + 1] = s_item[j]; j--; }
    s_item[j + 1] = key;
  }
}

// ---------------------------------------------------------------------------
// HUD - rects and circles only. No text, so no font and no string formatting.
// ---------------------------------------------------------------------------

static const uint8_t WEPC[W_COUNT] = {
  ARGB(2,2,2), ARGB(2,1,0), ARGB(1,1,1), ARGB(2,0,2),
  ARGB(0,3,0), ARGB(3,1,0), ARGB(0,2,0), ARGB(0,3,0)
};

// A pointer to the nearest gun shop, shown only when you are on foot and
// only when one is actually in the loaded block cache - so it appears as you
// get near a district that has one rather than promising something the world
// has not built yet. The complaint it answers is a fair one: shops are 14%
// of lots and still easy to walk past, because from street level a shopfront
// looks like every other shopfront until you are on top of it.
// Pointers to the nearest gun store (orange) and the nearest club
// (magenta), anywhere on the map, on a ring around the character.
static void hud_ring(GContext *ctx, int32_t x, int32_t y, int32_t r, GColor col);
static void draw_venue_pointer(GContext *ctx, bool club, GColor col) {
  int32_t bd = 0x7FFFFFFF, bx = 0, by = 0;
  for (int i = 0; i < g_nvenue; i++) {
    if (g_venue[i].club != (club ? 1 : 0)) continue;
    int32_t cx = (int32_t)g_venue[i].x << 2, cy = (int32_t)g_venue[i].y << 2;
    int32_t d = wr_dist(cx - g_p.x, cy - g_p.y);
    if (d < bd) { bd = d; bx = cx; by = cy; }
  }
  if (bd == 0x7FFFFFFF || bd < 240) return;      // none, or you are at it
  hud_ring(ctx, bx, by, 62, col);
}
WR_PART static void draw_shop_pointer(GContext *ctx) {
  if (g_p.dead || g_p.inside) return;
  draw_venue_pointer(ctx, false, GColorOrange);
  draw_venue_pointer(ctx, true, GColorMagenta);
}

// Centred text in a given colour. Every HUD string went through seven
// arguments of graphics_draw_text; this is the one place they are spelled.
void hud_text(GContext *ctx, const char *s, const char *font, int x, int y,
              int w, int h, GColor col) {
  // A black stroke under everything, so the text reads on any ground.
  static const int8_t OX[5] = { -1, 1, 0, 0, 0 }, OY[5] = { 0, 0, -1, 1, 0 };
  GFont f = fonts_get_system_font(font);
  for (int o = 0; o < 5; o++) {
    graphics_context_set_text_color(ctx, o < 4 ? GColorBlack : col);
    graphics_draw_text(ctx, s, f, GRect(x + OX[o], y + OY[o], w, h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

// A dot on the ring around the character, in the direction of a point.
static void hud_ring(GContext *ctx, int32_t x, int32_t y, int32_t r, GColor col) {
  // Relative to the CAMERA's heading, not the character's: the camera is
  // yawed off him by CAM_YAW_OFF, the other way in mirror mode, and a dot
  // measured from his heading sat 25 degrees wrong - 50 when mirrored.
  uint16_t cam = (uint16_t)(g_p.ang + (g_menu.cam_left ? -CAM_YAW_OFF : CAM_YAW_OFF));
  int32_t rel = (int16_t)(uint16_t)(angle_to(g_p.x, g_p.y, x, y) - cam);
  GPoint p = GPoint((int)(L(s_proj_cx) + ((fsin((uint16_t)rel) * r) >> 10)),   // logical space
                    (int)(140 - ((fcos((uint16_t)rel) * r) >> 10)));
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, p, 7);
  graphics_context_set_fill_color(ctx, col);
  graphics_fill_circle(ctx, p, 5);
}

void hud_marker(GContext *ctx, int32_t x, int32_t y, int32_t z, GColor col) {
  Proj p = gfx_project(x, y, z);
  bool on = p.ok && p.sx >= 4 && p.sx < SCR_W - 4 && p.sy >= 4 && p.sy < SCR_H - 4;
  if (!on) { if (g_frame & 4) hud_ring(ctx, x, y, 70, col); return; }
  int32_t sx = L(p.sx), sy = L(p.sy) - 10;      // projected in framebuffer space; drawn in the HUD's
  if (g_frame & 4) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect((int)sx - 7, (int)sy - 7, 14, 14), 3, GCornersAll);
    graphics_context_set_fill_color(ctx, col);
    graphics_fill_rect(ctx, GRect((int)sx - 5, (int)sy - 5, 10, 10), 2, GCornersAll);
  }
}

WR_PART static void draw_hud(GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  // No bar at all. Dithering it still cluttered the readouts sitting on it,
  // and the readouts are legible on their own provided each has a dark
  // outline to separate it from whatever is behind. Less furniture, more
  // game visible.
  (void)0;

  graphics_context_set_fill_color(ctx, GColorFolly);
  for (int i = 0; i < g_p.wanted; i++)
    graphics_fill_rect(ctx, GRect(4 + i * 9, 4, 6, 6), 1, GCornersAll);

  // Money: what you have, and indoors what you have put down.
  {
    char line[12]; int n = 0;
    line[n++] = '$';
    n += gfx_utoa(line + n, 8, g_p.cash);
    line[n] = 0;
    // Top centre, between the stars and the health bar: the weapon name
    // owns the top-left corner.
    hud_text(ctx, line, FONT_KEY_GOTHIC_18_BOLD, 64, 12, 72, 20, GColorWhite);
    // A room's own HUD - the store's PAID line, the club's meter - is the
    // room's to draw.
    if (g_p.inside) domain_call(DOP_DRAW_HUD, (int)(intptr_t)ctx);
  }

  // Health, and armour drawn ON it.
  //
  // A vest is a second life bar laid over the first: it is spent before any
  // health is, so showing it as a separate strip somewhere else made you
  // read two gauges to answer one question - how much can I take. Blue over
  // green, same track, and the blue drains away to reveal the green
  // underneath, which is exactly what the damage code does.
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, GRect(HUD_W - 54, 4, 50, 6), 1, GCornersAll);
  graphics_context_set_fill_color(ctx, g_p.hp > 30 ? GColorGreen : GColorRed);
  graphics_fill_rect(ctx, GRect(HUD_W - 54, 4, (g_p.hp * 50) / HP_MAX, 6), 1, GCornersAll);
  if (g_p.armor) {
    // Blue is a vest. Green is military grade - the tank's - and it is a
    // full bar until it has been shot down to a vest's worth.
    bool mil = g_p.armor > ARMOR_MAX;
    int aw = mil ? 50 : (g_p.armor * 50) / ARMOR_MAX;
    graphics_context_set_fill_color(ctx, mil ? GColorGreen : GColorPictonBlue);
    graphics_fill_rect(ctx, GRect(HUD_W - 54, 4, aw, 6), 1, GCornersAll);
    // A dark seam at the edge of the vest, so full armour over full health is
    // still readable as two things rather than one blue bar.
    graphics_context_set_stroke_color(ctx, GColorBlack);
    int ax = HUD_W - 54 + aw;
    graphics_draw_line(ctx, GPoint(ax, 4), GPoint(ax, 9));
  }

  GColor wc; wc.argb = WEPC[g_p.weapon];
  graphics_context_set_fill_color(ctx, wc);
  graphics_fill_rect(ctx, GRect(HUD_W / 2 - 18, 3, 10, 8), 1, GCornersAll);
  graphics_context_set_fill_color(ctx, GColorWhite);
  int pips = g_p.ammo / 2; if (pips > 8) pips = 8;
  for (int i = 0; i < pips; i++)
    graphics_fill_rect(ctx, GRect(HUD_W / 2 - 4 + i * 4, 5, 2, 5), 0, GCornerNone);

  // The pad is always drawn, dim when idle, so it needs no tutorial. On foot
  // it is a ring; behind the wheel it collapses to a vertical slot, because
  // there is no steering on it while driving.
  {
    bool on = g_p.stick_on != 0;
    graphics_context_set_stroke_color(ctx, on ? GColorWhite : GColorDarkGray);

    // Floating: it draws under your thumb while you hold it, and parks in the
    // corner when you let go so it stays discoverable.
    int32_t home_x = g_menu.cam_left ? (SCR_W - STICK_HOME_X) : STICK_HOME_X;   // framebuffer space
    int32_t cx = L(on ? g_p.stick_ox : home_x);
    int32_t cy = L(on ? g_p.stick_oy : STICK_HOME_Y);
    int32_t kx = cx;
    if (g_p.in_car) {
      graphics_draw_round_rect(ctx,
        GRect(cx - 10, cy - L(STICK_RANGE), 20, L(STICK_RANGE) * 2), 10);
      graphics_draw_line(ctx, GPoint(cx - 6, cy), GPoint(cx + 6, cy));
    } else {
      graphics_draw_circle(ctx, GPoint(cx, cy), (uint16_t)L(STICK_RANGE));
      kx = cx + wr_clamp((g_p.stick_s * L(STICK_RANGE)) >> 8,
                         -L(STICK_RANGE), L(STICK_RANGE));
    }
    int32_t ky = cy - wr_clamp((g_p.stick_f * L(STICK_RANGE)) >> 8,
                               -L(STICK_RANGE), L(STICK_RANGE));
    graphics_context_set_fill_color(ctx, on ? GColorWhite : GColorLightGray);
    graphics_fill_circle(ctx, GPoint(kx, ky), on ? 7 : 5);
  }

  // Being grabbed: a bar that fills as the officer gets a grip.
  // Zoom strip. Pebble's touch API reports a single point with no finger id,
  // so a real two-finger pinch is not available - this is the single-touch
  // equivalent: drag anywhere on the right edge.
#if !WR_ANDROID     // the phone pinches to zoom; no slider
  {
    int x = g_menu.cam_left ? 9 : HUD_W - 9;
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(x - 3, 20, 8, HUD_H - 46), 4, GCornersAll);
    graphics_context_set_fill_color(ctx,
      g_p.zoom_on ? GColorWhite : GColorDarkGray);
    int span = HUD_H - 54;
    // Top of the strip = camera in close, bottom = pulled out, so the knob
    // travels the same way your thumb does.
    int ky = 24 + (g_p.zoom * span) / ZOOM_MAX;
    graphics_fill_rect(ctx, GRect(x - 4, ky, 10, 8), 4, GCornersAll);
  }
#endif


  if (g_menu.open) domain_call(DOP_DRAW_MENU, (int)(intptr_t)ctx);   // the menu is a PLACE domain

  if (g_p.hurt_flash || g_p.dead) {
    graphics_context_set_stroke_color(ctx, g_p.busted ? GColorBlue : GColorRed);
    graphics_draw_rect(ctx, GRect(0, 0, HUD_W, HUD_H));
    graphics_draw_rect(ctx, GRect(1, 1, HUD_W - 2, HUD_H - 2));
  }

  // The caption for the cinematic: a moment of quiet, then the verdict, then
  // how to continue. Without this it genuinely was not clear whether you had
  // died or been arrested.
  if (g_p.dead) domain3_call(DOP_DRAW_HUD, (int)(intptr_t)ctx);   // the verdict: the dead tenant's

  // What you are holding, and how much of it, in the corner opposite the
  // zoom slider - so it never sits under the thumb working the camera and
  // mirrors with the rest of the layout.
  {
    int wx = g_menu.cam_left ? (HUD_W - 62) : 6;
    // Tight to the text: 20 tall, not 30. The old box had a third of its
    // height as empty padding under the word.
    graphics_context_set_fill_color(ctx, GColorBlack);
    // No plate at all. Every version of a background - solid, dithered at a
    // half, dithered at three quarters - was a box of furniture sitting on
    // the game. The name carries itself if it is heavy enough.

    // Back to the name. The silhouettes did not read at twenty pixels - a
    // pistol and a machine gun are the same handful of dark rectangles at
    // that size, and guessing which is which is worse than reading a short
    // word. draw_weapon_icon() is kept below for reference but unused.
    static const char *WNAME[W_COUNT] = {
      "FIST", "BAT", "PISTOL", "SHOTGUN", "MG", "FLAME", "ROCKET", "GRENADE", "CASH"
    };
    // Bold, and drawn four more times a pixel out in each direction. There
    // is no stroke or outline in this graphics API, so weight has to be
    // built by hand - and a single-weight glyph disappears the moment it
    // crosses a dark roof or a shadow.
    const char *wn = WNAME[g_p.weapon % W_COUNT];
    if (g_p.in_car && g_ent[g_p.in_car - 1].type == ENT_HELI)
      wn = g_p.heli_wep == 3 ? "LANDING" : g_p.heli_wep == 2 ? "LAND" : g_p.heli_wep ? "ROCKET" : "MG";
    else if (g_p.in_car && g_ent[g_p.in_car - 1].sub == CAR_TANK && g_ent[g_p.in_car - 1].type == ENT_COPCAR)
      wn = g_p.heli_wep ? "SHELL" : "MG";
    hud_text(ctx, wn, FONT_KEY_GOTHIC_18_BOLD, wx + 2, 21, 52, 18, GColorWhite);

    // No round count here - the bar along the top already carries it, and
    // two readouts of the same number is one too many.
  }

  draw_shop_pointer(ctx);

  // Markers: air support, a fare, a destination. A blinking square on the
  // thing when it is on screen, a chevron at the edge of the frame toward
  // it when it is not - a helicopter you cannot see is just gunfire from
  // nowhere, and a fare you cannot find is a walk.
  // The THREAT bench's own marks: the helicopter's marker, the fare's, the
  // arrest bar - each drawn by the tenant that owns it.
  if (!g_p.dead) domain2_call(DOP_DRAW_HUD, (int)(intptr_t)ctx);

  // Captions: short, loud, and gone in a couple of seconds.
  if (g_p.caption_t && !g_p.dead) {
    // The captions, packed: one string, walked. (A pointer table was four
    // bytes a caption.)
    static const char CAPS[] = "\0SWAT!\0LOST THEM\0SWAP CARS: LOSE THE HEAT\0TAP FIRE. HOLD MG. BOX SWAPS\0GUN UNLOCKED\0ALARM!\0YOU'RE OUT\0SIT DOWN...\0A FARE: STOP BY THEM\0GO TO THE MARKER\0FARE PAID\0PISTOL 40 SHOTGUN 100 VEST 120 GRENADE 150 MG 180 FLAME 250 ROCKET 400\0TAXI: WAIT FOR A WAVE\0GUN ON THE COUNTER\0TICKET: $50 ON COUNTER\0NO WEAPONS PAST HERE\0TICKET PAID: GATE AT BACK\0NOW BOARDING\0CALLING REINFORCEMENTS\0SHE IS SAFE HERE\0THE PASSENGERS STAY\0CARRIED: TAKE HER HOME\0SIT BACK. OR DON'T.\0LANDED\0";
    const char *cap = CAPS;                       // walk to the caption: no offset table at all
    for (int k = g_p.caption % CAP_COUNT; k > 0; k--) { while (*cap) cap++; cap++; }
    // Tall enough to wrap: the price list is two lines.
    int ch = (g_p.caption == CAP_PRICES) ? 34 : 18;
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(4, 46, HUD_W - 8, ch), 3, GCornersAll);
    hud_text(ctx, cap, FONT_KEY_GOTHIC_14_BOLD, 4, 46, HUD_W - 8, ch,
             g_p.caption == CAP_SWAT ? GColorRed : GColorWhite);
  }
}

// ---------------------------------------------------------------------------
// Street bands, stopped at the shoreline
// ---------------------------------------------------------------------------

// The diagonal families used to be drawn at full reach no matter what was
// under them, which painted tarmac straight out over the open sea near every
// coast - the "water under the roads". cell_at() only classifies a street
// inside (city radius - 2) of an island centre, so the band is clipped to its
// chord through each island's circle, with the same coastal wobble the
// classifier applies. Both islands get a segment, because the street grid is
// one global pattern that both are cut from. The highway is NOT masked; it is
// the bridge.
// A road marking belongs to exactly one road. Where two carriageways
// overlap - a crossing, the causeway over a street, a street meeting the
// ring - no line is painted, which is what makes a junction read as a
// junction rather than two roads happening to overlap.
// May a centre-line dash be painted at (x,y) on a road of this kind? Only
// where this is the ONLY road that owns the point - and a crossing road
// owns everything out to its stop line and crosswalk, which run to about
// 300 units past its kerb; the old 30-unit margin let dashes end on top of
// the zebra stripes.
bool road_paint_ok(int32_t x, int32_t y, uint8_t kind) {
  // The painter cannot tell the two diagonal families apart, so "mine" is
  // the diagonal this point is nearest to; any other diagonal is a crossing,
  // and a crossing owns its stop line and zebra - 300 past its kerb.
  Road r[4];
  int n = road_here(x, y, r), on = 0, mine = 0, near = -1; int32_t best = 0x7FFF;
  bool diag = kind == RD_DIAG_U || kind == RD_DIAG_V;
  for (int i = 0; i < n; i++)
    if ((r[i].kind == RD_DIAG_U || r[i].kind == RD_DIAG_V) && wr_abs(r[i].lat) < best) { best = wr_abs(r[i].lat); near = i; }
  for (int i = 0; i < n; i++) {
    bool rdiag = r[i].kind == RD_DIAG_U || r[i].kind == RD_DIAG_V;
    bool same = diag ? (i == near) : (r[i].kind == kind);
    int32_t margin = (rdiag && !same) ? 300 : 30;
    if (wr_abs(r[i].lat) >= r[i].half + margin) continue;
    on++;
    if (same) mine++;
  }
  return on == 1 && mine == 1;
}
#define road_alone road_paint_ok

WR_PART static void street_band_masked(int32_t bx, int32_t by, uint16_t ang,
                               int32_t half_wid, uint8_t col, int32_t stop) {
  int32_t c = fcos(ang), s = fsin(ang);
  // Every island in the table: a street band is painted only inside the
  // city radius of each, so it does not run out over the beach and the sea.
  for (int isl = 0; isl < ISL_SECRET; isl++) {   // the secret island has no streets
    int64_t vx = (int64_t)ISLANDS[isl].cx * TILE - bx, vy = (int64_t)ISLANDS[isl].cy * TILE - by;
    int32_t rbi = ISLANDS[isl].city_r - stop;
    int32_t along = (int32_t)((vx * c + vy * s) >> 10);   // t of closest approach
    int64_t perp  = (vy * c - vx * s) >> 10;

    // Base chord through the unwobbled circle first.
    int32_t R = rbi * TILE;
    int64_t s2 = (int64_t)R * R - perp * perp;
    if (s2 <= 0) continue;                                // band misses this island
    int32_t sh = wr_sqrt((int32_t)(s2 > 0x3FFFFFFF ? 0x3FFFFFFF : s2));

    // Refine each end once against the wobbled radius in that end's
    // direction. The wobble varies slowly with angle, so one iteration lands
    // within a fraction of a tile of where cell_at() actually stops the road.
    int32_t t0 = along - sh, t1 = along + sh;
    for (int e = 0; e < 2; e++) {
      int32_t te = e ? t1 : t0;
      int32_t ex = bx + (int32_t)(((int64_t)te * c) >> 10) - ISLANDS[isl].cx * TILE;
      int32_t ey = by + (int32_t)(((int64_t)te * s) >> 10) - ISLANDS[isl].cy * TILE;
      int32_t Rw = rbi * TILE + city_coast_wobble(ex, ey);
      if (Rw < 4 * TILE) Rw = 4 * TILE;
      int64_t s2w = (int64_t)Rw * Rw - perp * perp;
      if (s2w <= 0) { t0 = t1 = along; break; }
      int32_t shw = wr_sqrt((int32_t)(s2w > 0x3FFFFFFF ? 0x3FFFFFFF : s2w));
      if (e) t1 = along + shw; else t0 = along - shw;
    }
    if (t1 - t0 < 32) continue;

    int32_t mid = (t0 + t1) / 2, hl = (t1 - t0) / 2;
    gfx_ground_band(bx + (int32_t)(((int64_t)mid * c) >> 10),
                    by + (int32_t)(((int64_t)mid * s) >> 10),
                    ang, hl, half_wid, col);
  }
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------

// Pass 1: terrain. Regions, so axis-aligned tiles are fine for these.
// Out of line (see wr_ent.c): this is a scenery domain's job one day, and
// nothing can be handed to a domain while it is buried inside render_frame.
WR_PART static void draw_terrain(int32_t ptx, int32_t pty, int32_t r) {
  for (int32_t ty = pty - r; ty <= pty + r; ty++) {
    for (int32_t tx = ptx - r; tx <= ptx + r; tx++) {
      switch (cell_at(tx, ty)) {
        // Road tiles get the same ground colour as everything else. The
        // street bands are painted over the top, so classifying the ground
        // per tile no longer leaves a staircase of dark squares along every
        // diagonal kerb - which was the checkerboard that kept coming back.
        // Land is the colour the frame was cleared to; only the beach and
        // the sea need painting. That was a third of the frame's polygons
        // drawing lot-grey over lot-grey.
        case CELL_SAND: {
          draw_ground_tile(tx, ty, C_SAND);
          // Surf. A sand tile with water next to it gets a foam band along
          // that edge, breathing in and out on a slow cycle - the tide is
          // the cheapest thing that stops a coastline looking like a
          // colour change. One quad per shore tile, and only shore tiles
          // have one.
          for (int e2 = 0; e2 < 4; e2++) {
            static const int32_t NX[4] = { 1, -1, 0, 0 };
            static const int32_t NY[4] = { 0, 0, 1, -1 };
            if (cell_at(tx + NX[e2], ty + NY[e2]) != CELL_WATER) continue;
            int32_t swell = 34 + (int32_t)((g_frame >> 3) +
                            (uint32_t)(tx * 5 + ty * 3)) % 46;
            int32_t bx = tx * TILE + TILE / 2 + NX[e2] * (TILE / 2 - swell / 2);
            int32_t by = ty * TILE + TILE / 2 + NY[e2] * (TILE / 2 - swell / 2);
            int32_t hx = NX[e2] ? swell / 2 : TILE / 2;
            int32_t hy = NY[e2] ? swell / 2 : TILE / 2;
            const int32_t q[4][3] = {
              { bx - hx, by - hy, 1 }, { bx + hx, by - hy, 1 },
              { bx + hx, by + hy, 1 }, { bx - hx, by + hy, 1 },
            };
            gfx_world_poly(q, 4, C_FOAM);
          }
          break;
        }
        case CELL_WATER:
          draw_ground_tile(tx, ty,
            (((tx + ty + (int32_t)(g_frame >> 4)) & 3) < 2) ? C_WATER : C_WATER_D);
          break;
        default: break;
      }
    }
  }

#ifdef WR_PROFILE
  s_px_bucket = &wr_px_street;
#endif
  // Parks, as the block's own diamond - the same shape the buildings are
  // fitted into, so a park lines up with the kerbs exactly.
  {
    int32_t pq[MAX_PARKQ][4][2];
    int npq = city_park_quads(g_p.x, g_p.y, pq, MAX_PARKQ);
    for (int i = 0; i < npq; i++) {
      int32_t w[4][3];
      for (int c2 = 0; c2 < 4; c2++) {
        w[c2][0] = pq[i][c2][0]; w[c2][1] = pq[i][c2][1]; w[c2][2] = 0;
      }
      gfx_world_poly((const int32_t (*)[3])w, 4, g_park_col);
    }
  }
}

// Pass 2: the roads - pavements first, then all the tarmac over them.
WR_PART static void draw_roads(void) {
  // all of the other, so the second family's PAVEMENT was laid straight over
  // the first family's ROAD at every single crossing. Every intersection had
  // a slab of kerb through it, shifting as you moved. All the pavement has to
  // go down first, then all the tarmac on top of it.
  {
    // Long enough that the far end is always past the haze at the horizon.
    // A short band has a visible cut-off that slides along with you, which is
    // what made the roads look like they were moving with the camera.
    const uint16_t bang[2] = { 57344, 8192 };   // the two diagonal families

    typedef struct { int32_t x, y; uint16_t ang; } Band;
    Band band[2 * (2 * ROAD_LINES + 1) + N_BRIDGE * 2];
    int nb = 0;

    for (int fam = 0; fam < 2; fam++) {
      int32_t s = fam ? (g_p.x - g_p.y) : (g_p.x + g_p.y);
      int32_t o = fam ? (g_p.x + g_p.y) : (g_p.x - g_p.y);
      int32_t k0 = s / DIAG_PERIOD;
      if (s < 0) k0--;
      for (int32_t k = k0 - ROAD_LINES; k <= k0 + ROAD_LINES; k++) {
        int32_t sc = k * DIAG_PERIOD;
        band[nb].x   = fam ? (o + sc) / 2 : (sc + o) / 2;
        band[nb].y   = fam ? (o - sc) / 2 : (sc - o) / 2;
        band[nb].ang = bang[fam];
        nb++;
      }
    }

    // One pavement band under the whole street rather than two kerb strips.
    // The strips overdraw less, but they double the number of tall polygons,
    // and the scanline walk costs far more than the extra fill does.
    // The highway between the islands. It is a single straight line rather
    // than one of the diagonal families, so it gets its own band - and because
    // it is drawn after the street grid it reads as passing over it.
    // Every bridge is its own band, bounded by its span. A bridge with a gap
    // is painted as two pieces with nothing between them: the collapsed
    // stretch has no deck, so painted and driveable stay the same thing.
    typedef struct { int32_t mx, my, half; uint16_t ang; } Deck;
    Deck deck[N_BRIDGE * 2]; int nd = 0;
    for (int bi = 0; bi < N_BRIDGE; bi++) {
      const Bridge *B = &BRIDGES[bi];
      const Island *IA = &ISLANDS[B->a], *IB = &ISLANDS[B->b];
      const int32_t ax = IA->cx * TILE, ay = IA->cy * TILE;
      int32_t hdx = (IB->cx - IA->cx) * TILE, hdy = (IB->cy - IA->cy) * TILE;
      int64_t hl2 = (int64_t)hdx * hdx + (int64_t)hdy * hdy;
      int32_t full = wr_sqrt((int32_t)wr_clamp((int32_t)hl2, 0, 0x3FFFFFFF));
      uint16_t ang = bridge_angle_of(bi);
      int32_t seg[2][2] = { { B->t0, B->gap1 ? B->gap0 : B->t1 }, { B->gap1, B->t1 } };
      for (int s = 0; s < (B->gap1 ? 2 : 1); s++) {
        int32_t t0 = (full * seg[s][0]) / 1024, t1 = (full * seg[s][1]) / 1024;
        Deck *d = &deck[nd++];
        d->half = (t1 - t0) / 2; d->ang = ang;
        d->mx = ax + wr_muldiv(hdx, (t0 + t1) / 2, full);
        d->my = ay + wr_muldiv(hdy, (t0 + t1) / 2, full);
        gfx_ground_band(d->mx, d->my, ang, d->half, BRIDGE_KERB, C_PAVE);
      }
    }

    for (int i = 0; i < nb; i++)
      street_band_masked(band[i].x, band[i].y, band[i].ang, PAVE_HALF, C_PAVE, 2);
    for (int i = 0; i < nb; i++)
      street_band_masked(band[i].x, band[i].y, band[i].ang, ROAD_HALF, C_ROAD, 1);
    // The bridges' tarmac goes down over the streets they cross; their
    // centre lines are dashed like theirs, below.
    for (int i = 0; i < nd; i++) {
      gfx_ground_band(deck[i].mx, deck[i].my, deck[i].ang, deck[i].half, BRIDGE_HALF, C_ROAD);
      if (nb < (int)(sizeof band / sizeof band[0])) {
        band[nb].x = deck[i].mx; band[nb].y = deck[i].my; band[nb].ang = deck[i].ang; nb++;
      }
    }

    // Centre markings. They used to be drawn only on the SINGLE nearest
    // street per family, which is why some roads had no lines at all, and
    // why the lines jumped to a different street as you crossed the middle
    // of a block - "disappearing while I'm nearby". Every street band in
    // view gets its dashes now, skipped only past the perpendicular
    // distance where a dash is sub-pixel from any camera we have. The
    // dashes sit on absolute multiples of DASH_STEP along the street, so
    // the grid belongs to the road, never to the player.
    for (int i = 0; i < nb; i++) {
      const Band *b = &band[i];
      int32_t c = fcos(b->ang), s = fsin(b->ang);
      int32_t perp = wr_abs(((g_p.x - b->x) * -s + (g_p.y - b->y) * c) >> 10);
      if (perp > 8 * TILE) continue;
      // Centre the dash window on the player's foot on THIS street, snapped
      // to the world grid.
      int32_t foot = ((g_p.x - b->x) * c + (g_p.y - b->y) * s) >> 10;
      int32_t along = (int32_t)(((int64_t)b->x * c + (int64_t)b->y * s) >> 10);
      int32_t rem = (along + foot) % DASH_STEP;
      if (rem < 0) rem += DASH_STEP;
      int32_t base = foot - rem;
      for (int32_t m = -10; m <= 10; m++) {
        int32_t t = base + m * DASH_STEP;
        int32_t dx = b->x + ((t * c) >> 10);
        int32_t dy = b->y + ((t * s) >> 10);
        // Test the whole dash, not its centre. A dash is 256 units long and
        // the coast can cross it, so checking only the middle painted
        // markings out over the water wherever a street ran out at the
        // shore - and near the causeway the bridge corridor made
        // cell_at_point() answer "road" for water, which hid it.
        // Both ends of the dash on this road, and this road alone: no
        // paint over the water, none through a crossing.
        {
          uint8_t kind = (i == nb - 1) ? RD_BRIDGE : RD_DIAG_U;
          if (!road_alone(dx + ((DASH_LEN * c) >> 10), dy + ((DASH_LEN * s) >> 10), kind) ||
              !road_alone(dx - ((DASH_LEN * c) >> 10), dy - ((DASH_LEN * s) >> 10), kind))
            continue;
        }
#ifdef WR_DASH_TRACE
        { extern void wr_dash_trace(int32_t x, int32_t y, uint8_t kind); wr_dash_trace(dx, dy, (i == nb - 1) ? RD_BRIDGE : RD_DIAG_U); }
#endif
        // A dash is 36 units wide - under a pixel a street away, so the
        // scan converter rounded it in and out of existence frame to frame,
        // which read as z-fighting. It never was: the paint order is fixed.
        // Below a couple of pixels the dash snaps to a minimum 2x2 block
        // instead of vanishing.
        {
          const int32_t tt2[4] = { DASH_LEN, DASH_LEN, -DASH_LEN, -DASH_LEN };
          static const int32_t sg2[4] = { -1, 1, 1, -1 };
          int32_t w[4][3];
          for (int q = 0; q < 4; q++) {
            int32_t lx = tt2[q], ly = sg2[q] * 18;
            w[q][0] = dx + ((lx * c - ly * s) >> 10);
            w[q][1] = dy + ((lx * s + ly * c) >> 10);
            w[q][2] = 0;
          }
          Proj pp[12];
          int mm = gfx_clip_world_poly((const int32_t (*)[3])w, 4, pp);
          if (mm < 3) continue;
          int32_t bx0 = pp[0].sx, bx1 = bx0, by0 = pp[0].sy, by1 = by0;
          for (int q = 1; q < mm; q++) {
            if (pp[q].sx < bx0) bx0 = pp[q].sx;
            if (pp[q].sx > bx1) bx1 = pp[q].sx;
            if (pp[q].sy < by0) by0 = pp[q].sy;
            if (pp[q].sy > by1) by1 = pp[q].sy;
          }
          if (bx1 - bx0 < 2 || by1 - by0 < 2) {
            if (bx1 - bx0 < 2) bx1 = bx0 + 2;
            if (by1 - by0 < 2) by1 = by0 + 2;
            Proj r2[4] = {
              { (int16_t)bx0, (int16_t)by0, NEAR_Z, 1 },
              { (int16_t)bx1, (int16_t)by0, NEAR_Z, 1 },
              { (int16_t)bx1, (int16_t)by1, NEAR_Z, 1 },
              { (int16_t)bx0, (int16_t)by1, NEAR_Z, 1 },
            };
            gfx_fill_poly(r2, 4, C_LINE);
          } else {
            gfx_fill_poly(pp, mm, C_LINE);
          }
        }
      }
    }
  }

  // The ring road, walked as an arc of short quads on the true (wobbled)
  // circle, painted after the streets so their ends tuck under it. Dashes
  // step by arc length so they stay evenly spaced round the bend.
  {
    for (int isl = 0; isl < ISL_SECRET; isl++) {   // and no ring road
      int32_t cx = ISLANDS[isl].cx * TILE;
      int32_t cy = ISLANDS[isl].cy * TILE;
      int32_t base = ISLANDS[isl].city_r - 1;
      // Only the arc in front of you: a whole ring is 200 quads and most of
      // it is behind the camera or over the horizon.
      int32_t pd = wr_dist(g_p.x - cx, g_p.y - cy);
      if (pd > (base + 14) * TILE || pd < (base - 14) * TILE) continue;
      const int STEP = 512;                 // ~2.8 degrees per quad
      // Snapped to the quad grid: walked from the player's own angle the
      // dashes slid round the ring with him.
      uint16_t mid = (uint16_t)((angle_to(cx, cy, g_p.x, g_p.y) / STEP) * STEP);
      int32_t px0 = 0, py0 = 0, qx0 = 0, qy0 = 0;
      bool have = false;
      for (int i = -14; i <= 14; i++) {
        uint16_t a = (uint16_t)(mid + i * STEP);
        int32_t c = fcos(a), sn = fsin(a);
        int32_t rr = base * TILE + city_coast_wobble(c, sn);
        int32_t ex = cx + ((c * rr) >> 10), ey = cy + ((sn * rr) >> 10);
        // Outward normal is the radial direction; widen a touch past the
        // two-tile annulus so the tile steps underneath never show.
        int32_t nx = (c * (RING_HALF + 24)) >> 10, ny = (sn * (RING_HALF + 24)) >> 10;
        int32_t px1 = ex - nx, py1 = ey - ny;
        int32_t qx1 = ex + nx, qy1 = ey + ny;

        if (have) {
          const int32_t quad[4][3] = {
            { px0, py0, 0 }, { qx0, qy0, 0 }, { qx1, qy1, 0 }, { px1, py1, 0 },
          };
          gfx_world_poly(quad, 4, C_ROAD);
          // Centre line, one dash per two segments, following the bend.
          int32_t mx0 = (px0 + qx0) / 2, my0 = (py0 + qy0) / 2;
          int32_t mx1 = (px1 + qx1) / 2, my1 = (py1 + qy1) / 2;
          if ((((mid / STEP) + i) & 1) == 0 && road_alone((mx0 + mx1) / 2, (my0 + my1) / 2, RD_RING) &&
              road_alone(mx0, my0, RD_RING) && road_alone(mx1, my1, RD_RING)) {     // both ends clear too
#ifdef WR_DASH_TRACE
            { extern void wr_dash_trace(int32_t x, int32_t y, uint8_t kind); wr_dash_trace((mx0 + mx1) / 2, (my0 + my1) / 2, RD_RING); }
#endif
            int32_t tx2 = mx1 - mx0, ty2 = my1 - my0;
            int32_t tl = wr_dist(tx2, ty2);
            if (tl > 0) {
              int32_t hx = (-ty2 * 16) / tl, hy = (tx2 * 16) / tl;
              const int32_t dash[4][3] = {
                { mx0 - hx, my0 - hy, 1 }, { mx0 + hx, my0 + hy, 1 },
                { mx1 + hx, my1 + hy, 1 }, { mx1 - hx, my1 - hy, 1 },
              };
              gfx_world_poly(dash, 4, C_LINE);
            }
          }
        }
        px0 = px1; py0 = py1; qx0 = qx1; qy0 = qy1;
        have = true;
      }
    }
  }

  // Intersection markings: a stop bar across the approach lane and a
  // crosswalk ladder outside the box, on each of the four arms. This is
  // what makes a crossing read as a junction rather than two roads
  // happening to overlap.
  {
    int32_t jun[MAX_JUNC][3];
    int nj = city_junctions(g_p.x, g_p.y, jun, MAX_JUNC);
    for (int j = 0; j < nj; j++) {
      int32_t jx = jun[j][0], jy = jun[j][1];
      if (wr_dist(jx - g_p.x, jy - g_p.y) > 9 * TILE) continue;
      for (int a = 0; a < 4; a++) {
        if (!(jun[j][2] & (1 << a))) continue;      // no road down this arm
        int32_t ac = fcos(ROAD_ARM[a]), as = fsin(ROAD_ARM[a]);
        // Stop bar: across the near lane only, at the stop line.
        int32_t sd = ROAD_HALF + 70;
        int32_t bx = jx + ((ac * sd) >> 10), by = jy + ((as * sd) >> 10);
        const int32_t L0 = -14, L1 = 14;          // bar thickness, along arm
        const int32_t W0 = 16, W1 = ROAD_HALF - 10;   // near lane only
        const int32_t SB[4][2] = { { L0, W0 }, { L1, W0 }, { L1, W1 }, { L0, W1 } };
        int32_t q[4][3];
        for (int k = 0; k < 4; k++) {
          q[k][0] = bx + ((SB[k][0] * ac - SB[k][1] * as) >> 10);
          q[k][1] = by + ((SB[k][0] * as + SB[k][1] * ac) >> 10);
          q[k][2] = 1;
        }
        gfx_world_poly((const int32_t (*)[3])q, 4, C_LINE_W);
        // Crosswalk: four stripes just outside the stop bar, running
        // across the full carriageway.
        for (int st = 0; st < 4; st++) {
          int32_t cd = sd + 40 + st * 34;
          int32_t cx2 = jx + ((ac * cd) >> 10), cy2 = jy + ((as * cd) >> 10);
          // Six units, not eleven. At this scale a ladder of thick white
          // bars shouts louder than the road it is painted on.
          const int32_t CW[4][2] = { { -6, -ROAD_HALF + 12 }, { 6, -ROAD_HALF + 12 },
                                     { 6, ROAD_HALF - 12 },   { -6, ROAD_HALF - 12 } };
          int32_t cq[4][3];
          for (int k = 0; k < 4; k++) {
            cq[k][0] = cx2 + ((CW[k][0] * ac - CW[k][1] * as) >> 10);
            cq[k][1] = cy2 + ((CW[k][0] * as + CW[k][1] * ac) >> 10);
            cq[k][2] = 1;
          }
          gfx_world_poly((const int32_t (*)[3])cq, 4, C_LINE_W);
        }
      }
    }
  }

  for (int i = 0; i < MAX_BLOOD; i++) {
    if (!g_blood[i].life) continue;
    gfx_ground_quad(g_blood[i].x, g_blood[i].y, g_blood[i].size,
                    g_blood[i].life > 40 ? C_BLOOD : C_BLOOD_D);
  }
  // Ground paint that is the scenery's: garage bay markings on the islands
  // that have garages. Down with the ground, so parked cars sit on top.
  domain_call(DOP_DRAW_GROUND, 0);

  // Club lighting: coloured pools on the floor in front of the stage,
  // chasing each other through the palette.
  if (g_p.inside == IN_CLUB)
    for (int k = 0; k < 3; k++)
      gfx_ground_quad(ROOM_OX + g_in.w / 2 + (k - 1) * 230, ROOM_OY + TILE + 190, 80,
                      (uint8_t)(0xC0 | (((g_frame >> 2) + k * 21) & 0x3F)));

  // A helicopter's shadow. It is the one thing that tells you where an
  // aircraft is over the ground, and it costs one quad.
  for (int i = 0; i < MAX_ENT; i++) {
    Ent *e = &g_ent[i];
    if (e->type != ENT_HELI || e->z < 20) continue;
    int32_t c = fcos(e->ang), s = fsin(e->ang);
    const int32_t q[4][3] = {
      { e->x + ((c * 120 - s * 40) >> 10), e->y + ((s * 120 + c * 40) >> 10), 1 },
      { e->x + ((c * 120 + s * 40) >> 10), e->y + ((s * 120 - c * 40) >> 10), 1 },
      { e->x - ((c * 120 - s * 40) >> 10), e->y - ((s * 120 + c * 40) >> 10), 1 },
      { e->x - ((c * 120 + s * 40) >> 10), e->y - ((s * 120 - c * 40) >> 10), 1 },
    };
    gfx_set_stipple(true);
    gfx_world_poly(q, 4, ARGB(0,0,0));
    gfx_set_stipple(false);
  }

  // Droplets still in the air, before the tall stuff so bodies occlude them.
  for (int i = 0; i < MAX_GIB; i++) {
    Gib *q = &g_gib[i];
    if (!q->life) continue;
    // Tumbling shard, coloured by whatever it came off.
    gfx_part(q->x, q->y, (uint16_t)(q->x * 37 + g_frame * 900), 0, 0, q->z,
             q->size, q->size - 1, q->size + 2, 0, q->col, q->col, C_BLOOD_D);
  }

}

void render_frame(GContext *ctx, GRect bounds) {
  (void)bounds;
  // Paused: the world is not drawn - its scenery domain has given the bench
  // to the menu - so a proper pause screen instead of a stale frame.
  if (g_menu.open && ctx) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(0, 0, SCR_W, SCR_H), 0, GCornerNone);
    domain_call(DOP_DRAW_MENU, (int)(intptr_t)ctx);
    return;
  }

  update_zoom();

  s_proj_cx = g_menu.cam_left ? (SCR_W - PROJ_CX) : PROJ_CX;
  uint16_t yaw = (uint16_t)(g_p.ang +
                            (g_menu.cam_left ? -CAM_YAW_OFF : CAM_YAW_OFF));
  // The slow orbit of the death/arrest cinematic.
  if (g_p.dead) yaw = (uint16_t)(yaw + g_p.cine * 26);
  s_cam_x = g_p.x;
  s_cam_y = g_p.y;
  s_cyaw = fcos(yaw);
  s_syaw = fsin(yaw);
  s_last_render = g_frame;
  // A fixed sun. It used to follow the camera ("from behind the eye, off to
  // the left"), which lit whatever you looked at - and made every building
  // change colour as you turned. Now a face is lit by where it points in
  // the world, and stays that colour from any angle.
  g_light = 11000;
  // Where the eye actually sits on the ground plane: cam_back behind the
  // player along the view direction. The lot walls cull against this.
  s_eye_x = g_p.x - ((s_cam_back * s_cyaw) >> 10);
  s_eye_y = g_p.y - ((s_cam_back * s_syaw) >> 10);

  {
    Proj pp = gfx_project(g_p.x, g_p.y, 60);
    s_psx = pp.sx; s_psy = pp.sy;
    s_pdepth = pp.ok ? pp.depth : NEAR_Z;
  }

  s_fb = graphics_capture_frame_buffer(ctx);
  if (!s_fb) return;

  // Sky above the horizon, distant haze below it. At the top of the zoom
  // range the horizon is off screen and this is just the old flat clear; drop
  // to street level and it becomes a real skyline.
  int32_t hz = g_p.inside ? -1 : wr_clamp(g_horizon, -1, SCR_H);   // no sky indoors
  for (int y = 0; y < SCR_H; y++) {
    GBitmapDataRowInfo row = gbitmap_get_data_row_info(s_fb, (uint16_t)y);
    uint8_t col;
    if (y < hz - 12)      col = C_SKY;
    else if (y < hz)      col = C_SKY_LOW;      // a little haze at the join
    else if (y < hz + 10) col = C_HAZE;
    else                  col = g_lot_col;
    memset(row.data + row.min_x, col, (size_t)(row.max_x - row.min_x + 1));
  }

  int32_t ptx = g_p.x >> TILE_SH, pty = g_p.y >> TILE_SH;
  int32_t r = DRAW_AHEAD > DRAW_SIDE ? DRAW_AHEAD : DRAW_SIDE;
  city_refresh(g_p.x, g_p.y);

#ifdef WR_PROFILE
  s_px_bucket = &wr_px_terrain;
#endif
  draw_terrain(ptx, pty, r);
  draw_roads();

#ifdef WR_PROFILE
  s_px_bucket = &wr_px_lots;
#endif
  // Pass 2: everything with height, sorted far to near.
  s_nitem = 0;

  for (int i = 0; i < g_nlot; i++) {
    int32_t cx = (g_lot[i].x0 + g_lot[i].x1) / 2;
    int32_t cy = (g_lot[i].y0 + g_lot[i].y1) / 2;
    int32_t d = gfx_depth_at(cx, cy);
    if (d < NEAR_Z) continue;
    push_item(d, IT_LOT, (uint8_t)i, 0);
  }

  // From the air you cannot make out street furniture or people beyond a
  // few tiles, and the pulled-back camera sees far more city: leave them
  // out, or the frame goes over budget and the helicopter stutters.
  bool aloft = g_p.in_car && g_ent[g_p.in_car - 1].type == ENT_HELI && g_ent[g_p.in_car - 1].z > 60;
  for (int i = 0; i < MAX_PROP; i++) {
    if (!g_prop[i].alive) continue;
    if (aloft && wr_dist(g_prop[i].x - g_p.x, g_prop[i].y - g_p.y) > 6 * TILE) continue;
    int32_t d = gfx_depth_at(g_prop[i].x, g_prop[i].y);
    if (d < NEAR_Z) continue;
    push_item(d, IT_PROP, (uint8_t)i, 0);
  }

  for (int i = 0; i < MAX_ENT; i++) {
    if (g_ent[i].type == ENT_NONE) continue;
    if (g_p.in_car && (int)g_p.in_car - 1 == i) continue;
    if (aloft && (g_ent[i].type == ENT_PED || g_ent[i].type == ENT_COP) &&
        wr_dist(g_ent[i].x - g_p.x, g_ent[i].y - g_p.y) > 6 * TILE) continue;
    int32_t d = gfx_depth_at(g_ent[i].x, g_ent[i].y);
    if (d < NEAR_Z) continue;
    push_item(d, IT_ENT, (uint8_t)i, 0);
  }

  for (int i = 0; i < MAX_PICKUP; i++) {
    if (!g_pickup[i].alive) continue;
    int32_t d = gfx_depth_at(g_pickup[i].x, g_pickup[i].y);
    if (d < NEAR_Z) continue;
    push_item(d, IT_PICKUP, (uint8_t)i, 0);
  }

  for (int i = 0; i < MAX_BOOM; i++) {
    if (!g_boom[i].life) continue;
    int32_t d = gfx_depth_at(g_boom[i].x, g_boom[i].y);
    if (d < NEAR_Z) continue;
    push_item(d, IT_BOOM, (uint8_t)i, 0);
  }

  push_item(gfx_depth_at(g_p.x, g_p.y), IT_PLAYER, 0, 0);

  sort_items();
#ifdef WR_PROFILE
  wr_prof_items = (uint32_t)s_nitem;
#endif

  for (int i = 0; i < s_nitem; i++) {
    Item *it = &s_item[i];
    switch (it->kind) {
      case IT_LOT:
        // The island's architecture is a PLACE domain: what a building
        // looks like is the one thing that differs between the city, the
        // coast, the industrial estate and the ghetto. Everything around it
        // - the roads, the sort, the rasteriser - is shared, and stays here.
        domain_call(DOP_DRAW_LOT, it->a);
        break;

      case IT_PROP:
        model_prop(&g_prop[it->a]);
        break;

      case IT_ENT: {
        Ent *e = &g_ent[it->a];
        int32_t dist = wr_dist(e->x - g_p.x, e->y - g_p.y);
        if (e->type == ENT_HELI) {
          domain2_call(DOP_DRAW_ENT, it->a);
        } else if (e->type == ENT_AMB) {
          domain2_call(DOP_DRAW_AMB, it->a);
#if WR_BOATS
        } else if (ent_is_boat(e->type)) {
          model_boat(e, dist);
#endif
        } else if (e->type == ENT_CAR || e->type == ENT_COPCAR) {
          if (e->type == ENT_COPCAR && e->sub == CAR_TANK) { domain2_call(DOP_DRAW_ENT, it->a); break; }   // the guard's
          CarDraw c = { e->x, e->y, e->ang, e->sub,
                        (uint8_t)(e->type == ENT_COPCAR),
                        e->phase, e->door,
                        (uint8_t)(e->state == ST_DEAD), e->hp,
                        (uint8_t)(e->state == ST_FLEE), e->fire, e->z };
          model_car(&c, dist);
        } else {
          PedDraw p = { e->x, e->y, e->ang, e->sub,
                        (uint8_t)(e->type == ENT_COP),
                        e->phase, e->anim,
                        (uint8_t)((e->type == ENT_COP) ? e->wep : W_NONE),
                        (uint8_t)(e->state == ST_DEAD), 0, 0,
                        (uint8_t)((e->aux >> 8) & 3),
                        // Charred only once dead. Setting this from `fire`
                        // alone painted a man charcoal the instant he
                        // caught, so being set alight looked like being
                        // shot with a dark bullet.
                        (uint8_t)(e->fire != 0 && e->state == ST_DEAD),
                        (uint8_t)(e->fire != 0 && e->state != ST_DEAD),
                        e->z, (uint8_t)e->aux };
          model_ped(&p, dist);
        }
        break;
      }

      case IT_PICKUP:
        model_pickup(g_pickup[it->a].x, g_pickup[it->a].y,
                     g_pickup[it->a].weapon, g_frame);
        break;

      case IT_BOOM:
        model_boom(&g_boom[it->a]);
        break;

      case IT_PLAYER: {
#if WR_BOATS
        if (g_p.in_car && ent_is_boat(g_ent[g_p.in_car - 1].type)) {
          model_boat(&g_ent[g_p.in_car - 1], 0);
        } else
#endif
        if (g_p.in_car) {
          Ent *e = &g_ent[g_p.in_car - 1];
          if (e->type == ENT_HELI) domain3_call(DOP_DRAW_ENT, g_p.in_car - 1);   // yours: the flying tenant draws it
          else if (e->type == ENT_AMB) domain2_call(DOP_DRAW_AMB, g_p.in_car - 1);
          else if (e->type == ENT_COPCAR && e->sub == CAR_TANK) domain2_call(DOP_DRAW_ENT, g_p.in_car - 1);   // the guard draws its tank, yours too
          else {
            // In the air, a shadow on the water below is what sells the
            // height - it separates from the wheels as the car rises and
            // rushes back up to meet it on the way down.
            if (e->z > 0) gfx_ground_quad(e->x, e->y, 70 - e->z / 4, ARGB(0,0,1));
            CarDraw c = { e->x, e->y, e->ang, e->sub,
                          (uint8_t)(e->type == ENT_COPCAR),
                          e->phase, e->door, 0, e->hp,
                          0, e->fire, e->z };
            model_car(&c, 0);
          }
        } else {
          // Arrest is a kneel with the hands behind the back, not a corpse.
          // Knocked down by a car: the ragdoll pose and the same arc, but
          // he is very much alive and about to get back up.
          // Knocked flying: drawn at his height, tumbling, flat like the dead.
          PedDraw p = { g_p.x, g_p.y, (uint16_t)(g_p.ang + (g_p.knock ? g_p.kspin : 0)), PED_MAN, 0,
                        g_p.phase, g_p.anim, g_p.weapon,
                        (uint8_t)((g_p.dead && !g_p.busted) || g_p.knock), 1,
                        (uint8_t)(g_p.dead && g_p.busted), 0, 0, 0,
                        (int16_t)(g_p.knock ? g_p.kz : g_p.swim ? -34 : 0), 0 };
          model_ped(&p, 0);
          if (g_p.carry && !g_p.in_car) {              // someone in your arms, carried like a child
            PedDraw c = { g_p.x + ((fcos(g_p.ang) * 26) >> 10), g_p.y + ((fsin(g_p.ang) * 26) >> 10),
                          (uint16_t)(g_p.ang + 16384), PED_WOMAN, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 46, 0 };
            model_ped(&c, 0);
          }
        }
        break;
      }
    }
  }

  for (int i = 0; i < MAX_BULLET; i++) {
    Bullet *b = &g_bullet[i];
    if (!b->life) continue;
    if (b->kind == B_FIRE) {
      // A tongue of flame that swells and cools as it dies.
      int32_t s = 14 + (9 - b->life) * 5;
      uint8_t hot  = b->life > 6 ? ARGB(3,3,1) : b->life > 3 ? ARGB(3,2,0) : ARGB(3,0,0);
      uint8_t cool = b->life > 4 ? ARGB(3,1,0) : ARGB(1,1,1);
      gfx_part(b->x, b->y, (uint16_t)(g_frame << 11), 0, 0, 14,
               s, s, s + 10, 0, hot, hot, cool);
    } else if (b->kind == B_GRENADE) {
      // A dark ball on a lob: it rises and falls over its life.
      int32_t t = b->life, z = 12 + (t * (28 - t)) / 4;
      gfx_ground_quad(b->x, b->y, 7, ARGB(0,0,0));
      gfx_part(b->x, b->y, (uint16_t)(g_frame << 12), 0, 0, z, 7, 7, 7, 0, ARGB(1,1,0), ARGB(0,1,0), ARGB(0,0,0));
    } else {
      // A streak ALONG its flight - drawn at yaw zero it was a dot from
      // most angles. Rockets are fat, grey, with a flame behind them.
      uint16_t a = angle_to(0, 0, b->vx, b->vy);
      if (b->kind == B_ROCKET) {
        gfx_part(b->x, b->y, a, 0, 0, 26, 16, 6, 8, 0, ARGB(2,2,2), ARGB(2,2,2), ARGB(1,1,1));
        gfx_part(b->x, b->y, a, 18, 0, 26, 4, 5, 6, 0, ARGB(3,0,0), ARGB(3,0,0), ARGB(2,0,0));   // the nose
        gfx_part(b->x, b->y, a, -22, 0, 25, 10, 5, 7, 0, ARGB(3,3,0), ARGB(3,2,0), ARGB(3,1,0)); // the flame
      } else {
        gfx_part(b->x, b->y, a, 0, 0, 24, 14, 3, 4, 0, C_MUZZLE, C_MUZZLE, C_TRACER);
        gfx_part(b->x, b->y, a, -18, 0, 23, 12, 2, 3, 0, C_TRACER, C_TRACER, ARGB(2,1,0));
      }
    }
  }

  graphics_release_frame_buffer(ctx, s_fb);
  s_fb = NULL;

  draw_hud(ctx);
}
