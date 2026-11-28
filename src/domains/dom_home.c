#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"
static int dom_dispatch(int op, int arg);
#include "wr_shim.h"

// ---------------------------------------------------------------------------
// The safe house. One room: a bed against the back wall, a table, and your
// wife, who lives here and follows you about the place. Take her to the
// bed and press: the screen fades to black for a moment and your health
// is full. That is the whole trick, and it is the only place it works.
// ---------------------------------------------------------------------------

#define HW (3 * TILE)
#define HH (3 * TILE)
#define BED_X (ROOM_OX + HW / 2)
#define BED_Y (ROOM_OY + 120)
enum { PA_WIFE = PA_CLERK };

static const uint8_t WALL[3] = { ARGB(3,3,2), ARGB(3,3,2), ARGB(2,2,1) };

static void build(void) {
  g_in.w = HW; g_in.h = HH;
  int32_t x0 = ROOM_OX, y0 = ROOM_OY, x1 = x0 + HW, y1 = y0 + HH;
  const int32_t T = 40;
  memset(g_lot, 0, sizeof(Lot) * 4);
  Lot *L = g_lot;
  L[0].x0 = x0 - T; L[0].y0 = y0 - T; L[0].x1 = x1 + T; L[0].y1 = y0;
  L[1].x0 = x0 - T; L[1].y0 = y1;     L[1].x1 = x1 + T; L[1].y1 = y1 + T;
  L[2].x0 = x0 - T; L[2].y0 = y0;     L[2].x1 = x0;     L[2].y1 = y1;
  L[3].x0 = x1;     L[3].y0 = y0;     L[3].x1 = x1 + T; L[3].y1 = y1;
  for (int i = 0; i < 4; i++) { L[i].kind = LOT_SOLID; L[i].h = 160; }
  g_nlot = 4;
  memset(g_prop, 0, sizeof(Prop) * MAX_PROP);
  Prop *d = &g_prop[0]; d->alive = 1; d->kind = PROP_DOOR; d->hp = 255; d->x = x0 + HW / 2; d->y = y1; d->ang = 0;
  // the wife, by the window
  for (int i = 0; i < MAX_ENT; i++) if (g_ent[i].type == ENT_NONE) {
    Ent *e = &g_ent[i]; memset(e, 0, sizeof(Ent));
    e->type = ENT_PED; e->sub = PED_WOMAN; e->aux = PA_WIFE; e->hp = 100; e->state = ST_IDLE;
    e->x = x0 + HW / 4; e->y = y0 + HH / 2; e->ang = 16384;
    break;
  }
  g_p.x = x0 + HW / 2; g_p.y = y1 - 90; g_p.ang = 49152;
}

// She follows you, keeping a little distance, and never leaves.
static bool wife(Ent *e) {
  int32_t d = wr_dist(e->x - g_p.x, e->y - g_p.y);
  if (d > 110) {
    e->ang = angle_to(e->x, e->y, g_p.x, g_p.y);
    e->spd = 5; e->x += fmul(5, fcos(e->ang)); e->y += fmul(5, fsin(e->ang)); e->phase += 8;
  } else { e->spd = 0; e->ang = angle_to(e->x, e->y, g_p.x, g_p.y); }
  return true;
}

static void draw_room_lot(int i, const uint8_t *pal) {
  Lot *L = &g_lot[i];
  int32_t cx = (L->x0 + L->x1) / 2, cy = (L->y0 + L->y1) / 2;
  int32_t hl = (L->x1 - L->x0) / 2, hw = (L->y1 - L->y0) / 2;
  bool horiz = hl > hw;
  int32_t rx = ROOM_OX + g_in.w / 2, ry = ROOM_OY + g_in.h / 2;
  int32_t nx = rx - cx, ny = ry - cy;
  int32_t vx = fcos(g_p.ang), vy = fsin(g_p.ang);
  int32_t dot = (vx * nx + vy * ny);
  int32_t nl = wr_abs(nx) + wr_abs(ny);
  bool behind = nl && dot > (nl * 1024) / 3;
  gfx_set_stipple(behind);
  gfx_part(cx, cy, 0, 0, 0, 0, hl, hw, L->h, 0, pal[0], pal[0], pal[0]);
  gfx_part(cx, cy, 0, 0, 0, 0, hl + 2, hw + 2, 14, 0, pal[2], pal[2], pal[2]);
  int32_t fx = cx + (horiz ? 0 : (nx > 0 ? hl : -hl)), fy = cy + (horiz ? (ny > 0 ? hw : -hw) : 0);
  int32_t len = horiz ? hl : hw;
  for (int32_t o = -len + 150; o < len - 100; o += 300) {
    int32_t wx = horiz ? fx + o : fx, wy = horiz ? fy : fy + o;
    gfx_part(wx, wy, 0, 0, 0, 80, horiz ? 36 : 6, horiz ? 6 : 36, 96, 0, ARGB(2,3,3), ARGB(2,3,3), pal[2]);
  }
  gfx_set_stipple(false);
  if (i == 0) {
    // the furniture: the bed (frame, mattress, pillows, a red cover), a table, a rug
    const int32_t rug[4][3] = { { ROOM_OX + 200, ROOM_OY + 300, 1 }, { ROOM_OX + HW - 200, ROOM_OY + 300, 1 }, { ROOM_OX + HW - 200, ROOM_OY + HH - 200, 1 }, { ROOM_OX + 200, ROOM_OY + HH - 200, 1 } };
    gfx_world_poly(rug, 4, ARGB(2,1,0));
    gfx_part(BED_X, BED_Y, 0, 0, 0, 0, 90, 60, 16, 0, ARGB(2,1,0), ARGB(2,1,0), ARGB(1,0,0));
    gfx_part(BED_X, BED_Y, 0, 0, 0, 16, 84, 54, 10, 0, ARGB(3,3,3), ARGB(3,3,3), ARGB(2,2,2));
    gfx_part(BED_X, BED_Y, 0, 10, 0, 26, 70, 50, 6, 0, ARGB(3,0,0), ARGB(3,0,0), ARGB(2,0,0));
    gfx_part(BED_X, BED_Y, 0, -66, -24, 26, 14, 20, 8, 0, ARGB(3,3,3), ARGB(3,3,3), ARGB(2,2,2));
    gfx_part(BED_X, BED_Y, 0, -66, 24, 26, 14, 20, 8, 0, ARGB(3,3,3), ARGB(3,3,3), ARGB(2,2,2));
    gfx_part(BED_X, BED_Y, 0, -92, 0, 0, 4, 64, 60, 0, ARGB(2,1,0), ARGB(2,1,0), ARGB(1,0,0));   // headboard
    gfx_part(ROOM_OX + HW - 160, ROOM_OY + HH - 260, 0, 0, 0, 30, 50, 30, 6, 0, ARGB(2,1,0), ARGB(2,1,0), ARGB(1,0,0));   // table
  }
}

// The fade: a moment of black, then full health. g_in.wait counts it.
static void draw_hud_in(GContext *ctx) {
  if (!g_in.wait) return;
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, HUD_W, HUD_H), 0, GCornerNone);
}

static int action(void) {
  if (g_in.wait) return 1;
  if (wr_dist(g_p.x - BED_X, g_p.y - BED_Y) < 150) { g_in.wait = 50; audio_sfx(SFX_DOOR); return 1; }
  return 0;
}

static int dom_dispatch(int op, int arg) {
  switch (op) {
    case DOP_ENTER:    build(); return 1;
    case DOP_UPDATE:   if (g_in.wait && !--g_in.wait) { g_p.hp = HP_MAX; g_p.armor = g_p.armor > ARMOR_MAX ? g_p.armor : ARMOR_MAX; audio_sfx(SFX_PICKUP); } return 1;
    case DOP_PERSON:   return (uint8_t)g_ent[arg].aux == PA_WIFE ? (wife(&g_ent[arg]) ? 1 : 0) : 0;
    case DOP_DRAW_LOT: draw_room_lot(arg, WALL); return 1;
    case DOP_DRAW_HUD: draw_hud_in((GContext *)(intptr_t)arg); return 1;
    case DOP_ACTION:   return action();
    default: return 0;
  }
}

DOMAIN_ENTRY(dom_home_entry)
