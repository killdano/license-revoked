#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"
static int dom_dispatch(int op, int arg);
#include "wr_shim.h"
// The menu and the map draw through the SDK in the watch's 200x228 space;
// a phone build scales the calls. Logical width and height:
// On the round watch the same 200x228 layout is centred in the circle
// (HUD_OX, HUD_OY), and the map is centred on its own square.
#if WR_ROUND
#define MW 200
#define MH 228
#define MAPO 34
#else
#define MW (SCR_W * 228 / SCR_H)
#define MH 228
#define MAPO 0
#endif
#define OX HUD_OX
#define OY HUD_OY

#if WR_MONOLITH
void hud_text(GContext *ctx, const char *s, const char *font, int x, int y, int w, int h, GColor col);
#endif

// ---------------------------------------------------------------------------
// The pause menu. A PLACE domain: the world is not drawn while it is up, so
// it takes the bench from whatever was resident and hands it back on close.
// The menu's state (g_menu) is the kernel's; the drawing and the rows are
// here, which is about 700 bytes the image no longer carries.
// ---------------------------------------------------------------------------


// The map. The islands from the island table as discs, the bridges as lines
// with a break where the deck is missing, the clubs, stores and airports as
// marks, and you. 140 tiles of world into 180 pixels: 9 tiles to 11 px.
#define MS(t) ((int)(((int32_t)(t) * 11) / 9) + 10)
#define MP(x, y) GPoint(MS(x) + MAPO, MS(y) + MAPO)   // a map position on screen
static void draw_map(GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlue);
  graphics_fill_rect(ctx, GRect(0, 0, HUD_W, HUD_H), 0, GCornerNone);
  for (int i = 0; i < N_ISLAND; i++) {
    GPoint c = MP(ISLANDS[i].cx, ISLANDS[i].cy);
    graphics_context_set_fill_color(ctx, GColorYellow);
    graphics_fill_circle(ctx, c, (uint16_t)MS(ISLANDS[i].sand_r) - 10);
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_circle(ctx, c, (uint16_t)MS(ISLANDS[i].city_r) - 10);
  }
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  for (int b = 0; b < N_BRIDGE; b++) {
    const Island *A = &ISLANDS[BRIDGES[b].a], *B = &ISLANDS[BRIDGES[b].b];
    int32_t ax = MS(A->cx) + MAPO, ay = MS(A->cy) + MAPO, bx = MS(B->cx) + MAPO, by = MS(B->cy) + MAPO;
    int32_t t0 = BRIDGES[b].t0, t1 = BRIDGES[b].t1, g0 = BRIDGES[b].gap0, g1 = BRIDGES[b].gap1;
    #define AT(t) GPoint((int)(ax + ((bx - ax) * (t)) / 1024), (int)(ay + ((by - ay) * (t)) / 1024))
    if (g1) { graphics_draw_line(ctx, AT(t0), AT(g0)); graphics_draw_line(ctx, AT(g1), AT(t1)); }
    else      graphics_draw_line(ctx, AT(t0), AT(t1));
  }
  for (int i = 0; i < g_nvenue; i++) {
    GPoint p = MP(g_venue[i].x / 64, g_venue[i].y / 64);
    graphics_context_set_fill_color(ctx, g_venue[i].club == 2 ? GColorWhite
                                       : g_venue[i].club ? GColorMagenta : GColorOrange);
    graphics_fill_circle(ctx, p, g_venue[i].club == 2 ? 4 : 2);
  }
  // You: a GPS-style arrow, pointing the way you face. Out at sea past the
  // world's edge (the helicopter can get there) you are pinned to the map's
  // edge in your direction rather than drawn off it, and a line runs to the
  // nearest island so there is always a way home.
  GPoint me = MP(g_p.x / TILE, g_p.y / TILE);
  if (me.x < 6 + MAPO) me.x = 6 + MAPO;
  if (me.x > MS(140) + MAPO) me.x = MS(140) + MAPO;
  if (me.y < 6 + MAPO) me.y = 6 + MAPO;
  if (me.y > MS(140) + MAPO) me.y = MS(140) + MAPO;
  {
    int best = island_at(g_p.x, g_p.y);
    GPoint isl = MP(ISLANDS[best].cx, ISLANDS[best].cy);
    int32_t dx = g_p.x - ISLANDS[best].cx * TILE, dy = g_p.y - ISLANDS[best].cy * TILE;
    if (wr_dist(dx, dy) > (ISLANDS[best].sand_r + 4) * TILE) {   // off the island: show the way
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_draw_line(ctx, me, isl);
    }
  }
  int32_t c = fcos(g_p.ang), s = fsin(g_p.ang);
  #define PT(f, l) GPoint((int)(me.x + ((c * (f) - s * (l)) >> 10)), (int)(me.y + ((s * (f) + c * (l)) >> 10)))
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, me, 7);
  graphics_context_set_stroke_color(ctx, GColorGreen);
  for (int k = -1; k <= 1; k++) {                          // a fat arrow: three strokes
    graphics_draw_line(ctx, PT(9, k), PT(-5, 5 + k));
    graphics_draw_line(ctx, PT(9, k), PT(-5, -5 + k));
    graphics_draw_line(ctx, PT(-5, 5 + k), PT(-5, -5 + k));
  }
  graphics_context_set_fill_color(ctx, GColorGreen);
  graphics_fill_circle(ctx, me, 2);
  hud_text(ctx, "CITY", FONT_KEY_GOTHIC_14, MS(ISLAND_CX) + MAPO - 20, MS(ISLAND_CY) + MAPO - 8, 40, 14, GColorWhite);
  hud_text(ctx, "COAST", FONT_KEY_GOTHIC_14, MS(ISLAND_BX) + MAPO - 24, MS(ISLAND_BY) + MAPO - 8, 48, 14, GColorWhite);
  hud_text(ctx, "GHETTO", FONT_KEY_GOTHIC_14, MS(ISLAND_GX) + MAPO - 26, MS(ISLAND_GY) + MAPO - 8, 52, 14, GColorWhite);
#if WR_ROUND
  // Two short lines in the chord the circle leaves at the bottom.
  hud_text(ctx, "orange store  pink club", FONT_KEY_GOTHIC_14, 40, HUD_H - 46, HUD_W - 80, 14, GColorLightGray);
  hud_text(ctx, "white airport", FONT_KEY_GOTHIC_14, 60, HUD_H - 32, HUD_W - 120, 14, GColorLightGray);
#else
  hud_text(ctx, "orange store  pink club  white airport", FONT_KEY_GOTHIC_14, 0, MH - 18, MW, 14, GColorLightGray);
#endif
}

static void menu_draw(GContext *ctx) {
    if (g_menu.map) { draw_map(ctx); return; }
    static const char *ROW[MENU_ROWS] = { "Map", "Mirror", "Volume", "Sound", "Quit" };
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(12 + OX, 26 + OY, MW - 24, 176), 6, GCornersAll);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_round_rect(ctx, GRect(12 + OX, 26 + OY, MW - 24, 176), 6);

    for (int i = 0; i < MENU_ROWS; i++) {
      int y = 34 + OY + i * 32;
      // The phone touches rows directly: no selector bar, only the editing
      // highlight on the volume.
      if (g_menu.row == i && (!WR_ANDROID || g_menu.editing)) {
        graphics_context_set_fill_color(ctx, g_menu.editing ? GColorFolly : GColorDarkGray);
        graphics_fill_rect(ctx, GRect(16 + OX, y, MW - 32, 30), 4, GCornersAll);
      }
      hud_text(ctx, ROW[i], FONT_KEY_GOTHIC_18, 24 + OX, y + 2, 84, 24, GColorWhite);

      // Value readouts: a bar for the volume, a pill for each toggle.
      if (i == MENU_VOLUME) {
        graphics_context_set_stroke_color(ctx, GColorWhite);
        graphics_draw_round_rect(ctx, GRect(108 + OX, y + 9, 64, 12), 5);
        graphics_context_set_fill_color(ctx, GColorPictonBlue);
        graphics_fill_rect(ctx, GRect(109 + OX, y + 10, (g_menu.volume * 62) / 100, 10), 4, GCornersAll);
      } else if (i == MENU_SIDE || i == MENU_SFX) {
        bool on = (i == MENU_SIDE) ? g_menu.cam_left : g_menu.sfx_on;
        graphics_context_set_fill_color(ctx, on ? GColorPictonBlue : GColorDarkGray);
        graphics_fill_rect(ctx, GRect(108 + OX, y + 8, 30, 14), 7, GCornersAll);
        graphics_context_set_fill_color(ctx, GColorWhite);
        graphics_fill_circle(ctx, GPoint((on ? 130 : 116) + OX, y + 15), 5);
      }
    }
    return;                       // menu owns the screen
  }

static void nav(int8_t dir) {
  if (g_menu.map) { g_menu.map = 0; return; }
  if (!g_menu.editing) {
    int r = (int)g_menu.row + dir;
    if (r < 0) r = MENU_ROWS - 1;
    if (r >= MENU_ROWS) r = 0;
    g_menu.row = (uint8_t)r;
    return;
  }
  // Only Volume is a hold-to-edit slider now; the toggles toggle on SELECT.
  if (g_menu.row == MENU_VOLUME) {
    g_menu.volume = (uint8_t)wr_clamp(g_menu.volume - dir * 10, 0, 100);   // UP is louder
    audio_set_volume(g_menu.volume);
  }
}

static void sel(void) {
  if (g_menu.map) { g_menu.map = 0; return; }
  if (g_menu.row == MENU_MAP) { g_menu.map = 1; return; }
  switch (g_menu.row) {
    case MENU_SIDE:
      // Swap the over-the-shoulder framing to the other side: the zoom
      // slider, the stick's corner and the camera yaw bias mirror with it,
      // for one-handed play with the other hand.
      g_menu.cam_left = !g_menu.cam_left;
      break;
    case MENU_SFX:
      g_menu.sfx_on = !g_menu.sfx_on;
      audio_set_enabled(g_menu.sfx_on);
      break;
    case MENU_VOLUME: g_menu.editing = !g_menu.editing; break;
    case MENU_QUIT:   g_menu.quit = 1; break;
    default: break;
  }
}

static int dom_dispatch(int op, int arg) {
  switch (op) {
    case DOP_DRAW_MENU:   menu_draw((GContext *)(intptr_t)arg); return 1;
    case DOP_MENU_NAV:    nav((int8_t)arg); return 1;
    case DOP_MENU_SELECT: sel(); return 1;
    default: return 0;
  }
}

DOMAIN_ENTRY(dom_menu_entry)
