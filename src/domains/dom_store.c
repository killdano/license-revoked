#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"
static int dom_dispatch(int op, int arg);
#include "wr_shim.h"

#if WR_MONOLITH
extern int32_t s_eye_x, s_eye_y;
#endif

// Room sizes, in tiles. Each domain owns its own; with nothing else
// resident they can afford to be bigger than they were.
#define SW (4 * TILE)
#define SH (3 * TILE)
#define CW (8 * TILE)
#define CH (7 * TILE)
#define BOUNCER_HP 150
#define STAGE_H    30

static inline int32_t room_x1(void) { return ROOM_OX + g_in.w; }
static inline int32_t room_y1(void) { return ROOM_OY + g_in.h; }
static inline int32_t pole_x(void)  { return ROOM_OX + g_in.w / 2; }
static inline int32_t pole_y(void)  { return ROOM_OY + TILE / 2 + 20; }
static inline int32_t chair_x(void) { return ROOM_OX + g_in.w / 2; }
static inline int32_t chair_y(void) { return ROOM_OY + TILE + TILE / 2 + 60; }
static inline int32_t door_x(void)  { return ROOM_OX + g_in.w / 2; }

typedef struct { uint8_t what, a, b; int16_t x, y; uint16_t ang; } Fit;
#define FP(kind, tier, x, y, ang) { 0, kind, tier, x, y, ang }
#define FM(role, wep, x, y, ang)  { 1, role, wep, x, y, ang }
#define FW(role, wep, x, y, ang)  { 2, role, wep, x, y, ang }
#define FB(x, y)                  { 3, PA_BOUNCER, W_FIST, x, y, 49152 }

// The store: counter, four cabinets (one per tier), racked guns, customers.
static const Fit STORE_FIT[] = {
  FP(PROP_DOOR, 0, SW / 2, SH, 0),
  FP(PROP_COUNTER, 0, SW / 2, TILE - 10, 0),
  FM(PA_CLERK, W_SHOTGUN, SW / 2, 50, 16384),
  FP(PROP_CABINET, 1, 36, TILE + 60, 0),        FP(PROP_CABINET, 2, SW - 36, TILE + 60, 32768),
  FP(PROP_CABINET, 3, 36, TILE + 240, 0),       FP(PROP_CABINET, 4, SW - 36, TILE + 240, 32768),
  FP(PROP_WALLGUN, 0, 120, 24, 16384), FP(PROP_WALLGUN, 0, 250, 24, 16384),
  FP(PROP_WALLGUN, 0, 380, 24, 16384), FP(PROP_WALLGUN, 0, 510, 24, 16384),
  FM(PA_CUSTOMER, W_PISTOL, 200, 330, 49152),   FM(PA_CUSTOMER, W_PISTOL, 600, 330, 49152),
};

#if WR_MONOLITH
void put_ped(uint8_t type, uint8_t sub, uint8_t role, uint8_t wep,
             int32_t x, int32_t y, uint16_t ang);
void build_room(const Fit *f, int n, int32_t w, int32_t h, uint8_t mat);
bool go_to(Ent *e, int32_t x, int32_t y, int32_t spd);
#else
static void put_ped(uint8_t type, uint8_t sub, uint8_t role, uint8_t wep,
                    int32_t x, int32_t y, uint16_t ang) {
  for (int i = 0; i < MAX_ENT; i++) {
    Ent *e = &g_ent[i];
    if (e->type != ENT_NONE) continue;
    memset(e, 0, sizeof(Ent));
    e->type = type; e->sub = sub; e->aux = role; e->wep = wep;
    e->hp = (role == PA_BOUNCER) ? BOUNCER_HP : 100;
    e->state = ST_IDLE;
    e->x = x; e->y = y; e->ang = ang;
    e->timer = (uint8_t)wr_rand_n(200);
    return;
  }
}

// Four walls with a door gap in the front, and the fittings from a table.
static void build_room(const Fit *f, int n, int32_t w, int32_t h, uint8_t mat) {
  g_in.w = w; g_in.h = h;
  int32_t x0 = ROOM_OX, y0 = ROOM_OY, x1 = x0 + w, y1 = y0 + h;
  const int32_t T = 40;
  const int32_t wall[4][4] = {
    { x0 - T, y0 - T, x1 + T, y0 },
    { x0 - T, y0,     x0,     y1 },
    { x1,     y0,     x1 + T, y1 },
    { x0 - T, y1,     x1 + T, y1 + T },
  };
  g_nlot = 0;
  for (int i = 0; i < 4; i++) {
    Lot *L = &g_lot[g_nlot++];
    L->x0 = wall[i][0]; L->y0 = wall[i][1]; L->x1 = wall[i][2]; L->y1 = wall[i][3];
    L->h = 3 * TILE; L->mat = mat; L->kind = LOT_SOLID; L->glass = 0;
  }
  memset(g_prop, 0, sizeof(Prop) * MAX_PROP);
  int np = 0;
  for (int i = 0; i < n; i++, f++) {
    int32_t x = x0 + f->x, y = y0 + f->y;
    if (f->what == 0) {
      Prop *p = &g_prop[np++];
      p->x = x; p->y = y; p->kind = f->a; p->ang = f->ang;
      p->hp = PROP_HP[f->a]; p->alive = 1; p->spin = f->b;
      if (f->a == PROP_DISCO) p->z = 210;
    } else {
      put_ped(f->what == 3 ? ENT_COP : ENT_PED, f->what == 2 ? PED_WOMAN : PED_MAN,
              f->a, f->b, x, y, f->ang);
    }
  }
}

#endif

#if !WR_MONOLITH
// Walk toward a point. Returns true once there.
static bool go_to(Ent *e, int32_t x, int32_t y, int32_t spd) {
  int32_t d = wr_dist(e->x - x, e->y - y);
  if (d < 14) { e->spd = 0; return true; }
  e->ang = angle_to(e->x, e->y, x, y);
  e->spd = (int16_t)(d < spd ? d : spd);
  e->x += fmul(e->spd, fcos(e->ang));
  e->y += fmul(e->spd, fsin(e->ang));
  e->phase += (uint8_t)(3 + e->spd);
  return false;
}

#endif

// Circle a point: the dance. `r` is the radius, `rate` the angular speed.
static void orbit(Ent *e, int32_t cx, int32_t cy, int32_t r, int32_t rate) {
  uint16_t a = (uint16_t)(e->timer * rate);
  e->x = cx + fmul(r, fcos(a)); e->y = cy + fmul(r, fsin(a));
  e->ang = (uint16_t)(a + 32768);            // facing the thing she circles
  e->phase += 5;
  e->timer++;
}


// Everything is for sale. Pay up to a tier and that item is on the counter
// - and on the counter again every time you come back this life.
#define N_TIER 7
static const uint8_t  TIER_WEP[N_TIER]   = { W_PISTOL, W_SHOTGUN, PICKUP_ARMOR, W_GRENADE, W_MG, W_FLAME, W_ROCKET };
static const uint16_t TIER_PRICE[N_TIER] = { 40, 100, 120, 150, 180, 250, 400 };

static bool store_person(Ent *e) {
  if (e->state == ST_DEAD) return false;
  uint8_t role = (uint8_t)e->aux;
  int32_t x0 = ROOM_OX, y0 = ROOM_OY;
  if (role != PA_CLERK && role != PA_CUSTOMER) return false;
  if (e->type == ENT_COP) return false;         // the alarm made him a gunman
  if (e->hp < 100) { go_to(e, x0 + 40 + (e->timer & 1) * 40, y0 + 40, 7); return true; }
  e->ang = (role == PA_CUSTOMER) ? 49152 : 16384;
  return true;
}

// Money on the counter is credit. When it reaches a tier's price the glass
// slides back and the gun is yours; anything already paid for is put out
// again every visit.
// Everything paid for goes on the counter: on every throw, and on the way
// in, so what you bought last time is waiting for you.
static void restock(void) {
  for (int t = 0; t < N_TIER; t++) {
    if (g_in.credit < TIER_PRICE[t] || (g_in.unlocked & (1 << t))) continue;   // paid for: always on the counter
    g_in.unlocked |= (uint8_t)(1 << t);
    pickup_drop(ROOM_OX + g_in.w / 2 - 180 + t * 60, ROOM_OY + TILE + 40, TIER_WEP[t]);
    game_caption(CAP_TAKE, 60);
    audio_sfx(SFX_PICKUP);
  }
}
static void store_money(void) {
  g_in.credit += 10;
  g_in.spent[g_in.venue] = g_in.credit;
  restock();
}

// Trouble in a gun shop: the alarm, the staff draw, the police are called.
static void store_violence(bool killed) {
  if (killed) g_in.alarm = 1;
  if (g_in.alarm && g_in.hostile) return;
  g_in.alarm = 1;
  g_in.hostile = 1;
  for (int i = 0; i < MAX_ENT; i++)
    if (g_ent[i].type == ENT_PED && g_ent[i].state != ST_DEAD &&
        ((uint8_t)g_ent[i].aux == PA_CLERK || (uint8_t)g_ent[i].aux == PA_CUSTOMER)) {
      g_ent[i].type = ENT_COP; g_ent[i].state = ST_CHASE;
    }
  game_caption(CAP_ALARM, 60);
}

// Drab olive board: a shop, not a showroom.
static const uint8_t STOREWALL[3] = { ARGB(2,2,3), ARGB(1,2,3), ARGB(1,1,1) };

// Interior walls. The city's draw_lot lives in the scenery domain and is not
// resident while you are indoors, so a room draws its own - which it wants to
// anyway: a wall is a box, not a building, and each venue can colour its own.
// The wall the camera is beyond is skipped, so it never stands between the
// eye and you as you walk in, and is back the moment you turn round.
#if WR_MONOLITH
void draw_room_lot(int i, const uint8_t *pal);
#else
DOM_SHARED void draw_room_lot(int i, const uint8_t *pal) {
  const Lot *L = &g_lot[i];
  // A real wall: one colour on every face so nothing shifts as you turn,
  // a dark skirting along the floor, and a row of windows on the inside
  // face with a strip of light in each. A wall that stands between the
  // camera and you is not hidden - that let you see out of the room - it
  // is stippled, the way the city's towers are.
  int32_t cx = (L->x0 + L->x1) / 2, cy = (L->y0 + L->y1) / 2;
  int32_t hl = (L->x1 - L->x0) / 2, hw = (L->y1 - L->y0) / 2;
  int32_t rx = ROOM_OX + g_in.w / 2, ry = ROOM_OY + g_in.h / 2;
  int32_t vx = fcos(g_p.ang), vy = fsin(g_p.ang);
  int32_t nx = rx - cx, ny = ry - cy, nl = wr_abs(nx) + wr_abs(ny);
  bool between = nl && (vx * nx + vy * ny) > (nl * 300);   // at your back: the camera is beyond it
  gfx_set_stipple(between);
  gfx_part(cx, cy, 0, 0, 0, 0, hl, hw, L->h, 0, pal[0], pal[0], pal[0]);
  // The inside face: which side of the wall faces the room.
  bool horiz = hl > hw;                                    // a long wall along x
  int32_t sgn = horiz ? (ny > 0 ? 1 : -1) : (nx > 0 ? 1 : -1);
  int32_t fx = horiz ? cx : cx + sgn * hl, fy = horiz ? cy + sgn * hw : cy;
  // skirting
  gfx_part(fx, fy, 0, 0, 0, 0, horiz ? hl : 3, horiz ? 3 : hw, 22, 0, pal[2], pal[2], pal[2]);
  // Windows: tall, with a dark frame and a pale pane, one every 300 units
  // - two or three on a wall, not a row of holes. The pane sits a little
  // proud of the wall so it reads as glass in a frame.
  int32_t len = horiz ? hl : hw;
  for (int32_t o = -len + 150; o < len - 100; o += 300) {
    int32_t wx = horiz ? fx + o : fx, wy = horiz ? fy : fy + o;
    gfx_part(wx, wy, 0, 0, 0, 80, horiz ? 36 : 6, horiz ? 6 : 36, 96, 0, ARGB(2,3,3), ARGB(2,3,3), pal[2]);   // pane, dark-edged
  }
  gfx_set_stipple(false);
}
#endif

#if WR_MONOLITH
void money_throw_in(void);
#else
DOM_SHARED void money_throw_in(void) {
  if (g_p.cash < 10 || !g_p.inside) return;
  g_p.cash -= 10;
  g_in.paid = 1;
  g_in.stay = 0;
  for (int i = 0; i < MAX_GIB; i++) {
    Gib *g = &g_gib[i];
    if (g->life) continue;
    g->x = g_p.x + fmul(20, fcos(g_p.ang)); g->y = g_p.y + fmul(20, fsin(g_p.ang));
    g->z = 60;
    g->vx = (int16_t)(fmul(16, fcos(g_p.ang)) + (int32_t)wr_rand_n(7) - 3);
    g->vy = (int16_t)(fmul(16, fsin(g_p.ang)) + (int32_t)wr_rand_n(7) - 3);
    g->vz = 6; g->life = 26; g->size = 4;
    g->col = ARGB(0,3,0); g->stains = GIB_BILL;
    return;
  }
}

#endif


// The HUD's PAID line: what is on the counter so far.
static void draw_hud_in(GContext *ctx) {
  char line[16]; int n = 0;
  line[n++] = 'P'; line[n++] = 'A'; line[n++] = 'I'; line[n++] = 'D'; line[n++] = ' '; line[n++] = '$';
  n += gfx_utoa(line + n, 6, g_in.credit); line[n] = 0;
  hud_text(ctx, line, FONT_KEY_GOTHIC_14_BOLD, 64, 32, 72, 16, GColorYellow);
}

static int dom_dispatch(int op, int arg) {
  switch (op) {
    case DOP_ENTER:
      build_room(STORE_FIT, (int)(sizeof STORE_FIT / sizeof STORE_FIT[0]), SW, SH, 2);
      restock();                       // what you have paid for is waiting on the counter
      return 1;
    case DOP_UPDATE:   if (g_in.stay < 65000) g_in.stay++; return 1;
    case DOP_PERSON:   return store_person(&g_ent[arg]) ? 1 : 0;
    case DOP_MONEY:    store_money(); return 1;
    case DOP_DRAW_HUD: draw_hud_in((GContext *)(intptr_t)arg); return 1;
    case DOP_DRAW_LOT: draw_room_lot(arg, STOREWALL); return 1;
    case DOP_THROW:    money_throw_in(); return 1;
    case DOP_VIOLENCE: store_violence(arg != 0); return 1;
    default: return 0;
  }
}

DOMAIN_ENTRY(dom_store_entry)
