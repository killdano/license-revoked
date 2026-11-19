#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"
static int dom_dispatch(int op, int arg);
#include "wr_shim.h"

// ---------------------------------------------------------------------------
// Dead. Your hands do nothing, so this is the HANDS tenant while the
// cinematic plays: it draws the verdict, and after a moment the button
// puts you back on a road with half your money and half your guns.
// ---------------------------------------------------------------------------
static void respawn(void) {
  if (g_p.residents) g_p.residents--;     // one of them leaves while you are gone
  g_p.carry = 0;
  // Half your cash comes back with you; so does half of what you had paid
  // the gun stores, so the counter offers half the guns it did.
  for (int v = 0; v < 8; v++) g_in.spent[v] = (uint16_t)(g_in.spent[v] / 2);
  // Drop out of any interior first: the position search below asks the world
  // for a road, and while the shop is loaded the world is four walls and a
  // floor, so it found nothing and left the player standing in the sea.
  g_p.inside = 0;
  city_set_interior(false);

  // Somewhere on a road, on either island, with half the money you had.
  g_p.cash /= 2;
  for (int t = 0; t < 400; t++) {
    bool b = wr_rand_n(3) == 0;
    int32_t cx = b ? ISLAND_BX : ISLAND_CX, cy = b ? ISLAND_BY : ISLAND_CY, rr = b ? CITY_BR : CITY_R;
    int32_t tx = cx - rr + (int32_t)wr_rand_n(rr * 2);
    int32_t ty = cy - rr + (int32_t)wr_rand_n(rr * 2);
    if (cell_at(tx, ty) != CELL_ROAD) continue;
    g_p.x = ((tx) * TILE) + TILE / 2;
    g_p.y = ((ty) * TILE) + TILE / 2;
    break;
  }
  g_p.ang = 0; g_p.spd = 0; g_p.turn = 0; g_p.turn_dir = 0;
  g_p.stick_f = 0; g_p.stick_s = 0; g_p.strafe = 0; g_p.stick_on = 0;
  g_p.zoom_on = 0;
  g_p.zoom = 110;   // start mid-slider, not jammed against the near stop
  g_p.phase = 0; g_p.anim = 0;
  g_p.in_car = 0;
  g_p.weapon = W_FIST; g_p.ammo = 0;
  g_p.hp = HP_MAX;
  g_p.wanted = 0; g_p.heat = 0; g_p.evade = 0;
  g_p.dead = 0;
  g_p.cine = 0;
  g_p.armor = 0;
  for (int i = 0; i < W_COUNT; i++) g_p.inv[i] = 0;
  g_p.arrest = 0;
  g_p.busted = 0;
  ent_reset();
  city_refresh(g_p.x, g_p.y);
  props_refresh();
  pickups_refresh();
}

static void draw_verdict(GContext *ctx) {
  if (g_p.cine <= 16) return;
  int hw = SCR_W * 228 / SCR_H;
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 82, hw, 40), 0, GCornerNone);
  hud_text(ctx, g_p.busted ? "FREEDOM REVOKED" : "LIFE REVOKED", FONT_KEY_GOTHIC_28_BOLD,
           0, 84, hw, 32, g_p.busted ? GColorPictonBlue : GColorRed);
}

static int dom_dispatch(int op, int arg) {
  switch (op) {
    // Let the cinematic play before the button works, so a mashed SELECT
    // from the fight does not skip straight past the moment.
    case DOP_ATTACK:   if (g_p.cine > 45) respawn(); return 1;
    case DOP_DRAW_HUD: draw_verdict((GContext *)(intptr_t)arg); return 1;
    default: return 0;
  }
}

DOMAIN_ENTRY(dom_h_dead_entry)
