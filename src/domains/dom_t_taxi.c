#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"
static int dom_dispatch(int op, int arg);
#include "wr_shim.h"
#include "wr_tune.h"

// ---------------------------------------------------------------------------
// The taxi trade. Second tenant of the THREAT arena, and the one that makes
// that arena pay: a fare needs zero stars and air support needs four, so
// the two can never be resident at once. When you pick up a star the fare
// bails, this unloads, and the police get the bench.
// ---------------------------------------------------------------------------

static void put_ped(uint8_t type, uint8_t sub, uint8_t role, uint8_t wep,
                    int32_t x, int32_t y, uint16_t ang) {
  for (int i = 0; i < MAX_ENT; i++) {
    Ent *e = &g_ent[i];
    if (e->type != ENT_NONE) continue;
    memset(e, 0, sizeof(Ent));
    e->type = type; e->sub = sub; e->aux = role; e->wep = wep;
    e->hp = 100; e->state = ST_IDLE;
    e->x = x; e->y = y; e->ang = ang;
    e->timer = (uint8_t)wr_rand_n(200);
    return;
  }
}

// ---------------------------------------------------------------------------
// The taxi.
//
// Take a cab and someone on the pavement will flag you down. Stop beside
// them and they get in; a marker shows where they want to go; stop there
// and they pay by the tile. Nobody hails a cab that is being chased.
// ---------------------------------------------------------------------------

static void taxi_update_in(void) {
#if WR_TAXI
  bool cab = g_p.in_car && g_ent[g_p.in_car - 1].type == ENT_CAR &&
             g_ent[g_p.in_car - 1].sub == CAR_TAXI && !g_p.inside;
  if (!cab || g_p.wanted) {
    if (g_taxi.fare && !g_taxi.aboard) g_ent[g_taxi.fare - 1].aux = 0;
    g_taxi.fare = 0; g_taxi.aboard = 0; g_taxi.wait = 60;
    return;
  }
  if (g_taxi.aboard) {
    int32_t d = wr_dist(g_p.x - g_taxi.dx, g_p.y - g_taxi.dy);
    if (d < 200 && wr_abs(g_p.spd) < 5) {
      // Paid by the distance the trip was, and out they get.
      g_p.cash = (uint16_t)wr_clamp(g_p.cash + 30 + g_taxi.wait * 2, 0, 9999);
      put_ped(ENT_PED, PED_MAN, 0, W_FIST, g_p.x + fmul(90, fsin(g_p.ang)), g_p.y - fmul(90, fcos(g_p.ang)),
              (uint16_t)(g_p.ang + 16384));
      g_taxi.aboard = 0; g_taxi.fare = 0; g_taxi.wait = 50;
      game_caption(CAP_PAID, 60);
      audio_sfx(SFX_PICKUP);
    }
    return;
  }
  if (g_taxi.fare) {
    Ent *f = &g_ent[g_taxi.fare - 1];
    if (f->type != ENT_PED || f->state == ST_DEAD || (uint8_t)f->aux != PA_FARE) {
      if (f->type == ENT_PED) f->aux = 0;          // a dead fare is just a body
      g_taxi.fare = 0; return;
    }
    f->ang = angle_to(f->x, f->y, g_p.x, g_p.y);      // waving you down
    f->anim = (g_frame & 16) ? 6 : 0;
    if (wr_dist(g_p.x - f->x, g_p.y - f->y) < 260 && wr_abs(g_p.spd) < 5) {
      // In they get. Somewhere across town, on a road.
      f->type = ENT_NONE;
      for (int t = 0; t < 40; t++) {
        uint16_t a = (uint16_t)(wr_rand() & 0xFFFF);
        int32_t d = (8 + (int32_t)wr_rand_n(8)) * TILE;
        int32_t x = g_p.x + fmul(d, fcos(a)), y = g_p.y + fmul(d, fsin(a));
        if (cell_at_point(x, y) != CELL_ROAD) continue;
        road_snap(&x, &y, NULL);
        g_taxi.dx = x; g_taxi.dy = y; g_taxi.aboard = 1;
        g_taxi.wait = (uint8_t)(d / 128);                // the fare, by the tile
        game_caption(CAP_DROP, 60);
        break;
      }
    }
    return;
  }
  // Someone on the pavement, not too close, wants a ride.
  if (g_taxi.wait) { g_taxi.wait--; return; }
  for (int i = 0; i < MAX_ENT; i++) {
    Ent *e = &g_ent[i];
    if (e->type != ENT_PED || e->state == ST_DEAD || e->aux) continue;
    int32_t d = wr_dist(e->x - g_p.x, e->y - g_p.y);
    if (d < 3 * TILE || d > 9 * TILE) continue;
    e->aux = PA_FARE; e->state = ST_IDLE; e->spd = 0;
    g_taxi.fare = (uint8_t)(i + 1);
    game_caption(CAP_FARE, 60);
    return;
  }
#endif
}

#include "ambulance.inc"


// Where the fare is, and where they want to go.
static void draw_hud_t(GContext *ctx) {
  for (int i = 0; i < MAX_ENT; i++)
    if (g_taxi.fare == i + 1 && g_ent[i].state != ST_DEAD) hud_marker(ctx, g_ent[i].x, g_ent[i].y, 70, GColorYellow);
  if (g_taxi.aboard) hud_marker(ctx, g_taxi.dx, g_taxi.dy, 40, GColorYellow);
}

static int dom_dispatch(int op, int arg) {
  (void)arg;
  switch (op) {
    case DOP_UPDATE: taxi_update_in(); amb_dispatch(); return 1;
    case DOP_DRAW_HUD: draw_hud_t((GContext *)(intptr_t)arg); return 1;
    case DOP_AMB:      update_amb(&g_ent[arg]); return 1;
    case DOP_DRAW_AMB: draw_amb(&g_ent[arg]); return 1;
    default: return 0;
  }
}

DOMAIN_ENTRY(dom_taxi_entry)
