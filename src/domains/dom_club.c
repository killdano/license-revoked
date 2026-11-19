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
#define SW (6 * TILE)
#define SH (5 * TILE)
#define CW (5 * TILE)
#define CH (4 * TILE)
#define BOUNCER_HP 150
#define STAGE_H    30

static inline int32_t room_x1(void) { return ROOM_OX + g_in.w; }
static inline int32_t room_y1(void) { return ROOM_OY + g_in.h; }
static inline int32_t pole_x(void)  { return ROOM_OX + g_in.w / 2; }
static inline int32_t pole_y(void)  { return ROOM_OY + TILE / 2 + 20; }
static inline int32_t chair_x(void) { return ROOM_OX + g_in.w / 2; }
static inline int32_t chair_y(void) { return ROOM_OY + TILE + 90; }
static inline int32_t door_x(void)  { return ROOM_OX + g_in.w / 2; }

typedef struct { uint8_t what, a, b; int16_t x, y; uint16_t ang; } Fit;
#define FP(kind, tier, x, y, ang) { 0, kind, tier, x, y, ang }
#define FM(role, wep, x, y, ang)  { 1, role, wep, x, y, ang }
#define FW(role, wep, x, y, ang)  { 2, role, wep, x, y, ang }
#define FB(x, y)                  { 3, PA_BOUNCER, W_FIST, x, y, 49152 }

// The club: five by four tiles. It was briefly bigger, which only made it
// feel empty - the room should be cramped and the stage should not be.
static const Fit CLUB_FIT[] = {
  FP(PROP_DOOR, 0, CW / 2, CH, 0),
  FP(PROP_STAGE, 0, CW / 2, TILE / 2 + 20, 0),
  FP(PROP_POLE, 0, CW / 2, TILE / 2 + 20, 0),
  FP(PROP_POLE, 0, CW / 2 - 260, TILE / 2 + 20, 0),
  FP(PROP_DISCO, 0, CW / 2, CH / 2, 0),
  FP(PROP_COUNTER, 0, CW - 80, CH / 2, 16384),
  FP(PROP_CHAIR, 0, CW / 2, TILE + 90, 49152),
  FP(PROP_CHAIR, 0, 90, TILE + 190, 49152),  FM(PA_PATRON, W_FIST, 90, TILE + 186, 49152),
  FP(PROP_CHAIR, 0, 250, TILE + 280, 49152), FM(PA_PATRON, W_FIST, 250, TILE + 276, 49152),
  FP(PROP_CHAIR, 0, 420, TILE + 190, 49152),
  FP(PROP_CHAIR, 0, 590, TILE + 280, 49152), FM(PA_PATRON, W_FIST, 590, TILE + 276, 49152),
  FP(PROP_CHAIR, 0, 760, TILE + 190, 49152),  FM(PA_PATRON, W_FIST, 760, TILE + 186, 49152),
  FW(PA_DANCER, W_FIST, CW / 2 + 80, TILE / 2 + 20, 16384),
  FB(CW / 2 - 110, CH - 60), FB(CW / 2 + 110, CH - 60),
};

DOM_SHARED void put_ped(uint8_t type, uint8_t sub, uint8_t role, uint8_t wep,
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
DOM_SHARED void build_room(const Fit *f, int n, int32_t w, int32_t h, uint8_t mat) {
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

// Walk toward a point. Returns true once there.
DOM_SHARED bool go_to(Ent *e, int32_t x, int32_t y, int32_t spd) {
  int32_t d = wr_dist(e->x - x, e->y - y);
  if (d < 14) { e->spd = 0; return true; }
  e->ang = angle_to(e->x, e->y, x, y);
  e->spd = (int16_t)(d < spd ? d : spd);
  e->x += fmul(e->spd, fcos(e->ang));
  e->y += fmul(e->spd, fsin(e->ang));
  e->phase += (uint8_t)(3 + e->spd);
  return false;
}

// Circle a point: the dance. `r` is the radius, `rate` the angular speed.
DOM_SHARED void orbit(Ent *e, int32_t cx, int32_t cy, int32_t r, int32_t rate) {
  uint16_t a = (uint16_t)(e->timer * rate);
  e->x = cx + fmul(r, fcos(a)); e->y = cy + fmul(r, fsin(a));
  e->ang = (uint16_t)(a + 32768);            // facing the thing she circles
  e->phase += 5;
  e->timer++;
}


// Per-frame behaviour for someone in the club.
static bool club_person(Ent *e) {
  if (e->state == ST_DEAD) return false;
  uint8_t role = (uint8_t)e->aux;
  int32_t x0 = ROOM_OX, y0 = ROOM_OY;
  bool scared = g_in.angry || g_in.hostile || g_in.alarm;
  (void)x0; (void)y0;
  if (role == PA_DANCER) {
      if (scared) {                          // into the corner, hands over ears
        go_to(e, x0 + 40 + (e->timer & 1) * 40, y0 + 40, 6);
        e->ang = 49152;
        return true;
      }
      uint8_t clothes = (uint8_t)(e->aux >> 8);
      bool mine = (g_in.dancer == (uint8_t)(e - g_ent) + 1) && g_in.level >= 3;
      if (mine) {
        // Yours. Level three is a dance around the chair; four is closer.
        orbit(e, chair_x(), chair_y(), g_in.level >= 4 ? 34 : 70, 500);
      } else if (e->aux & PA_BUSY) {
        // A patron's lap dance: around his chair, bottoms on, for a while.
        Ent *p = &g_ent[(e->aux >> 10) & 63];
        orbit(e, p->x, p->y, 60, 600);
        if ((e->timer & 255) == 0) e->aux &= ~PA_BUSY;
      } else if ((e->timer >> 6) & 1) {
        // On the pole: a tight fast spin around it, riding up and sliding
        // down. The rig cannot lean, so this is the shape of it.
        orbit(e, pole_x(), pole_y(), 26, 1400);
        e->z = (int16_t)(STAGE_H + 40 + ((fsin((uint16_t)(e->timer * 700)) * 36) >> 10));
        e->phase += 9;
      } else {
        orbit(e, pole_x(), pole_y(), 90, 300);
        e->z = STAGE_H;
      }
      if (mine || (e->aux & PA_BUSY)) e->z = 0;
      // Undressing follows the show level: tops at one, everything at two -
      // but only the paying customer's show goes past the first.
      uint8_t want = (uint8_t)(g_in.level >= 2 && g_in.paid ? 2 : g_in.level >= 1 ? 1 : 0);
      if (want > clothes) {
        clothes++;
        gib_burst_debris(e->x, e->y, 60, 0, 0, 3, clothes == 1 ? ARGB(3,1,2) : ARGB(2,0,2));
      } else if (want < clothes && (g_frame & 127) == 0) clothes--;
      e->aux = (int16_t)((e->aux & ~0x300) | (clothes << 8));
      return true;
    }
  if (role == PA_PATRON) {
      if (scared || e->hp < 100) { go_to(e, x0 + 40 + (e->timer & 1) * 40, y0 + 40, 7); return true; }
      // Sits, and every so often walks up and throws a few bills, which
      // moves the show along a little for everyone.
      if (e->state == ST_WALK) {
        if (go_to(e, pole_x() + (e->timer & 1 ? 120 : -120), pole_y() + 140, 5)) {
          // Their money is never enough to take the show past the first
          // level, and it never tops up yours.
          // Onto the stage, not the floor: that is the dancer's money, and
          // the house minds who picks it up.
          money_burst(pole_x() + (int32_t)wr_rand_n(60) - 30, pole_y() + (int32_t)wr_rand_n(40) - 20, 2);
          if (g_in.meter < 90) g_in.meter = (uint16_t)wr_clamp(g_in.meter + 15, 0, 90);
          e->state = ST_FLEE;              // and back to the chair
        }
      } else if (e->state == ST_FLEE) {
        if (go_to(e, e->vx * 4, e->vy * 4, 5)) e->state = ST_IDLE;
      } else {
        e->ang = 49152;
        if (wr_rand_n(1200) == 0) { e->vx = (int16_t)(e->x >> 2); e->vy = (int16_t)(e->y >> 2); e->state = ST_WALK; }
      }
      return true;
    }
  if (role == PA_BOUNCER) {
      if (e->hp < BOUNCER_HP) g_in.hostile = 1;
      int32_t d = wr_dist(e->x - g_p.x, e->y - g_p.y);
      if (g_in.hostile) {
        // You hit the door staff. They do not push any more.
        if (d > 50) go_to(e, g_p.x, g_p.y, 9);
        else if ((e->timer++ & 15) == 0) { player_absorb(14); g_p.hurt_flash = 4; e->anim = 6; audio_sfx(SFX_PUNCH); }
      } else if (g_in.angry) {
        // Walk him to the door, one shove at a time, then out.
        if (d > 60) go_to(e, g_p.x, g_p.y, 8);
        else {
          uint16_t to = angle_to(g_p.x, g_p.y, door_x(), room_y1());
          g_p.x += fmul(6, fcos(to)); g_p.y += fmul(6, fsin(to));
          g_p.spd = 0; e->anim = 4;
          if (g_p.y > room_y1() - 70) inside_leave();
        }
      } else {
        e->spd = 0; e->ang = 49152;
      }
      return true;
    }
  return false;
}

static void club_update(void) {
  if (g_in.stay < 65000) g_in.stay++;

  // The meter runs down; the show follows it. Five levels: the dance,
  // tops off, everything off, over to your chair, and closer still.
  if ((g_frame % 10) == 0 && g_in.meter) g_in.meter--;
  uint8_t lvl = (uint8_t)(g_in.meter >= 160 ? 4 : g_in.meter >= 110 ? 3 :
                          g_in.meter >= 70 ? 2 : g_in.meter >= 30 ? 1 : 0);
  if (g_in.paid == 0 && lvl > 1) lvl = 1;    // the house never goes further on its own
  if (lvl >= 3 && g_in.level < 3 && !g_in.angry) {
    // She comes over and you go where you are put.
    g_p.x = chair_x(); g_p.y = chair_y() + 40; g_p.ang = 49152;
    g_p.spd = 0; g_p.strafe = 0; g_in.pinned = 1;
    g_in.dancer = 0;
    for (int i = 0; i < MAX_ENT; i++)
      if (g_ent[i].type == ENT_PED && (uint8_t)g_ent[i].aux == PA_DANCER) { g_in.dancer = (uint8_t)(i + 1); break; }
    game_caption(CAP_CHAIR, 50);
  }
  if (lvl < 3) g_in.pinned = 0;
  // The lap dance is restorative: full health, and a vest's worth on top.
  if (lvl >= 4 && g_in.level < 4) { g_p.hp = HP_MAX; g_p.armor = ARMOR_MAX; audio_sfx(SFX_PICKUP); }
  g_in.level = lvl;

  // More money brings more dancers out.
  int dancers = 0;
  for (int i = 0; i < MAX_ENT; i++)
    if (g_ent[i].type == ENT_PED && (uint8_t)g_ent[i].aux == PA_DANCER && g_ent[i].state != ST_DEAD) dancers++;
  if (dancers < 1 + lvl / 2 && dancers < 3 && !g_in.angry)
    put_ped(ENT_PED, PED_WOMAN, PA_DANCER, W_FIST, pole_x() - 80, pole_y(), 16384);

  // A patron gets a turn now and then, while the house is calm.
  if ((g_frame & 511) == 0 && lvl >= 1 && !g_in.pinned) {
    int p = -1, d = -1;
    for (int i = 0; i < MAX_ENT; i++) {
      if (g_ent[i].type != ENT_PED || g_ent[i].state == ST_DEAD) continue;
      if ((uint8_t)g_ent[i].aux == PA_PATRON && g_ent[i].state == ST_IDLE) p = i;
      if ((uint8_t)g_ent[i].aux == PA_DANCER && !(g_ent[i].aux & PA_BUSY)) d = i;
    }
    if (p >= 0 && d >= 0) g_ent[d].aux = (int16_t)((g_ent[d].aux & 0x3FF) | PA_BUSY | (p << 10));
  }

  // On the stage yourself: out. Loitering without paying: eventually out.
  if (!g_in.angry) {
    if (wr_abs(g_p.x - pole_x()) < 300 && wr_abs(g_p.y - pole_y()) < 130) inside_violence(false);
    else if (g_in.stay > 3000 && (g_frame & 63) == 0 && wr_rand_n(40) == 0) {
      g_in.angry = 1; game_caption(CAP_OUT, 60);
    }
  }
}

// A bill landed near the stage: the show runs on it.
static void club_money(int32_t x, int32_t y) {
  if (wr_dist(x - pole_x(), y - pole_y()) < 3 * TILE) {
    g_in.meter = (uint16_t)wr_clamp(g_in.meter + 10, 0, 400);
    g_in.stay = 0;
  }
}

// A shot, a swing, a body, or a boot on the stage.
static void club_violence(bool killed) {
  if (killed) g_in.alarm = 1;
  if (!g_in.angry) game_caption(CAP_OUT, 60);
  g_in.angry = 1;
  g_in.banned |= (uint8_t)(1 << g_in.venue);
  g_in.pinned = 0;
}

// Deep purple, dim: a room lit by the stage and not much else.
static const uint8_t CLUBWALL[3] = { ARGB(1,0,2), ARGB(2,1,3), ARGB(0,0,1) };

// Interior walls. The city's draw_lot lives in the scenery domain and is not
// resident while you are indoors, so a room draws its own - which it wants to
// anyway: a wall is a box, not a building, and each venue can colour its own.
// The wall the camera is beyond is skipped, so it never stands between the
// eye and you as you walk in, and is back the moment you turn round.
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


// The show's meter, on the HUD.
static void draw_hud_in(GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, GRect(60, 34, 80, 4), 1, GCornersAll);
  graphics_context_set_fill_color(ctx, GColorMagenta);
  graphics_fill_rect(ctx, GRect(60, 34, g_in.meter / 5, 4), 1, GCornersAll);
}

// The rescue. With every bouncer down, press beside a dancer and you pick
// her up - carried out in your arms, through cars, boats and planes, to the
// house on the secret island. She is not an entity while carried: the
// kernel draws her with you, and the island takes delivery.
static int rescue(void) {
  if (g_p.carry) return 0;
  for (int i = 0; i < MAX_ENT; i++)
    if (g_ent[i].type == ENT_PED && (uint8_t)g_ent[i].aux == PA_BOUNCER && g_ent[i].state != ST_DEAD) return 0;
  int d = ent_nearest_of(ENT_PED, g_p.x, g_p.y, 120, PA_DANCER);
  if (d < 0 || g_ent[d].state == ST_DEAD) return 0;
  g_ent[d].type = ENT_NONE;
  g_p.carry = 1;
  game_caption(CAP_RESCUE, 100); audio_sfx(SFX_PICKUP);
  return 1;
}

static int dom_dispatch(int op, int arg) {
  switch (op) {
    case DOP_ACTION:   return rescue();
    case DOP_ENTER:
      build_room(CLUB_FIT, (int)(sizeof CLUB_FIT / sizeof CLUB_FIT[0]), CW, CH, 5);
      return 1;
    case DOP_UPDATE: {
      // The stage's money is not yours. On or beside the stage, cash
      // going up means you took some, and the bouncers walk you out.
      static uint16_t s_cash_seen;
      if (g_p.cash > s_cash_seen && !g_in.angry &&
          wr_abs(g_p.x - pole_x()) < 200 && wr_abs(g_p.y - pole_y()) < 130) {
        g_in.angry = 1; game_caption(CAP_OUT, 70); audio_sfx(SFX_ARREST);
      }
      s_cash_seen = g_p.cash;
      club_update(); return 1;
    }
    case DOP_PERSON:   return club_person(&g_ent[arg]) ? 1 : 0;
    case DOP_MONEY:    club_money((arg & 0xFFFF) * 4, (arg >> 16) * 4); return 1;
    case DOP_DRAW_HUD: draw_hud_in((GContext *)(intptr_t)arg); return 1;
    case DOP_DRAW_LOT: draw_room_lot(arg, CLUBWALL); return 1;
    case DOP_THROW:    money_throw_in(); return 1;
    case DOP_VIOLENCE: club_violence(arg != 0); return 1;
    default: return 0;
  }
}

DOMAIN_ENTRY(dom_club_entry)
