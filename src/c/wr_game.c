#include "wr.h"
#include "wr_domain.h"
// See wr_ent.c.
#define WR_PART __attribute__((noinline))
#include "wr_gfx.h"

Player g_p;
Menu   g_menu;

// Turning is rate-based, not position-based: hold longer and you swing faster,
// let go and friction bleeds it off. This is what makes the tank controls
// feel like steering rather than nudging.
// Movement tuning lives in wr_tune.h: the movement tenants use it too.
#include "wr_tune.h"
// Must stay well inside SPAWN_MIN (6 tiles) or a freshly dispatched unit
// would reset the evasion timer the instant it appears, and escaping the
// top wanted levels would be impossible.
#define COP_SIGHT   (7 * TILE)

// An officer who gets hands on you while you are stopped will drag you out of
// the car and put you down. Keep moving and he never gets a grip.

// True while the vehicle under the player floats.
void fire_bullet_kind(uint16_t ang, uint8_t dmg, uint8_t kind);
static bool on_heli(void) {
  return g_p.in_car && g_ent[g_p.in_car - 1].type == ENT_HELI;
}
static bool airborne(void) {
  return on_heli() && g_ent[g_p.in_car - 1].z > 40;
}

void game_caption(uint8_t cap, uint8_t frames) { g_p.caption = cap; g_p.caption_t = frames; }

// The helicopter's armament. The chain gun chatters while SELECT is held;
// the rockets are one per press and there are only a few of them. Both are
// fired from the aircraft's position along its heading, which from height
// means they walk across the ground ahead of you.
static void heli_fire(void) { domain3_call(DOP_FIRE, 0); }



// The flamethrower does almost no impact damage: 6, not 30. The BURN kills,
// over several seconds, which is the whole point of setting someone alight
// rather than shooting them. At 30 a point-blank pull killed instantly and
// the burning never played at all.
static const uint8_t WEP_COOLDOWN[W_COUNT] = { 9, 13, 7, 18, 4, 2, 32, 22, 4 };

// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// Gun shop interior
//
// Rather than build a second engine for interiors, entering swaps the lot
// cache for four wall lots and the props for counters and cabinets. Drawing,
// collision, ragdolls and particles all carry over untouched.
// ---------------------------------------------------------------------------


void game_init(void) {
  domain_scenery();
  memset(&g_p, 0, sizeof(g_p));
  city_venues_init();
  // The first spawn is a respawn: the dead tenant's, so it is one piece of code.
  g_p.dead = 1; g_p.cine = 100;
  domain_hands(); domain3_call(DOP_ATTACK, 0);
  city_refresh(g_p.x, g_p.y);
  props_refresh();
  pickups_refresh();
  g_p.kills = 0;
  g_p.stolen = 0;
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Pause menu
// ---------------------------------------------------------------------------

// The menu is a PLACE domain: opening it takes the bench from the scenery
// or the room you are in, closing it gives the bench back. The world is not
// drawn while it is up, so nothing that was resident is missed.
static uint8_t s_before_menu;
void menu_toggle(void) {
  g_menu.open = !g_menu.open;
  g_menu.editing = 0;
  if (g_menu.open) { s_before_menu = domain_current(); domain_use(DOM_MENU); audio_hush(); }
  else domain_use(s_before_menu);
  if (g_menu.open) {
    // Drop every held input, or you resume mid-turn at full throttle.
    g_p.turn_dir = 0;
    g_p.stick_on = 0;
    g_p.stick_f = 0;
    g_p.stick_s = 0;
    g_p.zoom_on = 0;
  }
}

void menu_nav(int8_t dir) { domain_call(DOP_MENU_NAV, dir); }
void menu_select(void)     { domain_call(DOP_MENU_SELECT, 0); }

#if WR_ANDROID
// The phone's trackpad turns you directly, by an angle per frame, on top
// of whatever the buttons do. Not in the watch image.
void game_turn_by(int32_t dang) { g_p.ang = (uint16_t)(g_p.ang + dang); }
#endif

void game_input_turn(int8_t dir) { g_p.turn_dir = dir; }

#define ZOOM_STRIP_X (SCR_W - PX(26))

// The pad is measured RADIALLY, not per-axis. Treating each axis separately
// makes a square pad: full deflection on both axes at once gives a magnitude
// of 362 rather than 256, so a diagonal push ran about 1.4x faster than a
// straight one. Clamping the vector length instead makes it properly round.
static void stick_update(int16_t sx, int16_t sy) {
  g_p.stick_x = sx;
  g_p.stick_y = sy;

  int32_t dx = (int32_t)sx - g_p.stick_ox;
  int32_t dy = (int32_t)g_p.stick_oy - sy;      // screen y grows downward
  int32_t r = wr_sqrt(dx * dx + dy * dy);

  if (r <= STICK_DEAD) { g_p.stick_f = 0; g_p.stick_s = 0; return; }

  int32_t mag = ((r - STICK_DEAD) * 256) / (STICK_RANGE - STICK_DEAD);
  if (mag > 256) mag = 256;

  g_p.stick_f = (int16_t)((dy * mag) / r);
  // Behind the wheel the pad is throttle only - steering lives on the buttons.
  g_p.stick_s = g_p.in_car ? 0 : (int16_t)((dx * mag) / r);
}

void game_touch_down(int16_t sx, int16_t sy) {
  // The weapon box, in the corner opposite the zoom slider. Tapping it puts
  // the gun away and brings your fists up, and tapping again takes the gun
  // back out with the same ammunition.
  {
    // The hit area must match the plate that is drawn - 30 x 20 since the
    // name became an icon. It was still 56 x 30, so a tap well outside the
    // visible box silently changed weapon.
    int wx = g_menu.cam_left ? (SCR_W - PX(62)) : PX(6);
    if (sx >= wx && sx <= wx + PX(56) && sy >= PX(24) && sy <= PX(44) && !g_p.dead) {
      weapon_next();
      g_p.touch_used = 1;              // one press, one weapon: moves until liftoff do nothing
      return;
    }
  }
  // One screen edge is the zoom slider - the right by default, the left in
  // mirrored (left-shoulder) mode, so it always lives where the stick is
  // not. Pebble reports one touch point with no finger id, so a genuine
  // two-finger pinch is not possible - this is the single-touch equivalent.
  bool in_strip = WR_ANDROID ? false : g_menu.cam_left ? (sx < SCR_W - ZOOM_STRIP_X)
                                  : (sx >= ZOOM_STRIP_X);
  if (in_strip) {
    // Only re-baseline on a genuinely new touch. Some drags re-report
    // touchdown, and rebasing every event pinned the slider to wherever it
    // already was - which is why it kept springing back.
    if (!g_p.zoom_on) {
      g_p.zoom_on = 1;
      g_p.zoom_ref = sy;
      g_p.zoom_base = g_p.zoom;
    }
    return;
  }
  // Floating pad: wherever the thumb lands becomes centre.
  g_p.stick_ox = sx;
  g_p.stick_oy = sy;
  g_p.stick_on = 1;
  stick_update(sx, sy);
}

void game_touch_move(int16_t sx, int16_t sy) {
  if (g_p.zoom_on) {
    // Drag UP to come in close, DOWN to pull out - the knob follows your
    // thumb rather than the camera distance.
    int32_t d = (int32_t)sy - g_p.zoom_ref;
    g_p.zoom = (int16_t)wr_clamp(g_p.zoom_base + d * 3 * 228 / SCR_H, 0, ZOOM_MAX);
    return;
  }
  if (g_p.touch_used) return;
  if (!g_p.stick_on) { game_touch_down(sx, sy); return; }
  stick_update(sx, sy);
}

void game_touch_up(void) {
  g_p.touch_used = 0;
  g_p.stick_on = 0;
  g_p.zoom_on = 0;
  g_p.stick_f = 0;
  g_p.stick_s = 0;
  // NOTE: g_p.zoom is deliberately untouched. It is a setting, not an input
  // state - lifting your finger must leave the camera where you put it.
}

// ---------------------------------------------------------------------------
// Combat
// ---------------------------------------------------------------------------

// Heat for a crime, IF the police know about it. An officer within eight
// tiles saw it: full heat. Otherwise there is a one-in-three chance
// somebody phoned it in, for half. Killing a cop is always known - his
// radio was open. Once you are already wanted it all counts: they are
// watching you.
void add_heat(uint16_t amount) {
  if (!g_p.wanted && amount < 100) {              // a big enough crime (an explosion) is always noticed
    bool seen = ent_any_cop_near(8 * TILE, false);
    if (!seen && wr_rand_n(3) == 0) return;     // one time in three nobody calls it in
  }
  g_p.heat = (uint16_t)wr_clamp((int32_t)g_p.heat + amount, 0, HEAT_MAX);
  uint8_t nw = (uint8_t)(g_p.heat / HEAT_PER_STAR);
  if (nw > 5) nw = 5;
  if (nw > g_p.wanted) { g_p.wanted = nw; audio_sfx(SFX_WANTED_UP); }
  g_p.evade = 0;   // committing a crime puts you right back on the radar
}

// Killing police too fast breaks them: after the fourth officer inside
// about eight seconds they pull back - no units for a while, and the
// status line says why - and when they come back it is with more.
static void cop_killed(void) {
  // Its own window counter: the chase total (copkills) drives escalation
  // and must never be reset by this.
  g_p.copkill_t = 200;
  if (++g_p.kwin >= 4 && !g_p.lull) {
    g_p.lull = 300;
    g_p.kwin = 0;
    game_caption(CAP_REINF, 120);
  }
}

uint8_t g_cop_fire;   // a police round is doing the damage right now

void kill_entity(Ent *e) {
  bool was_cop = (e->type == ENT_COP);
  e->state = ST_DEAD;
  e->hp = 0;
  e->spd = 0;
  e->anim = 0;
  // Burning to death leaves ash and a charred shell, not a pool.
  if (e->fire) gib_burst_debris(e->x, e->y, 40, 0, 0, 5, ARGB(1,1,1));
  else         gib_burst(e->x, e->y, 46, 0, 0, 6);
  // A dying cry, not the panic yelp they make while running.
  audio_sfx(e->sub == PED_WOMAN ? SFX_DIE_W : SFX_DIE_M);
  if ((was_cop || (uint8_t)e->aux == PA_MILITIA) && e->wep) pickup_drop(e->x, e->y, e->wep);
  if (was_cop && !g_cop_fire) { g_p.hostile = 1; cop_killed(); if (g_p.copkills < 255) g_p.copkills++; }   // not their own crossfire
  money_on_death(e);
  inside_violence(true);
  g_p.kills++;
  add_heat(was_cop ? HEAT_COPKILL : HEAT_KILL);
}

void damage_entity(Ent *e, uint8_t dmg) {
  if (e->state == ST_DEAD) return;
  if (e->type == ENT_PED && (uint8_t)e->aux == PA_MILITIA) g_p.militia_hostile = 1;
  if (e->type == ENT_CAR || e->type == ENT_COPCAR) return;
  // Fire cauterises. A man who is alight throws off embers, not blood - the
  // wound is being burned shut as it is made.
  if (e->fire) gib_burst_debris(e->x, e->y, 44, 0, 0, 3, ARGB(3,2,0));
  else         gib_burst(e->x, e->y, 54, 0, 0, 3);
  if (e->hp <= dmg) kill_entity(e);
  else { e->hp -= dmg; if (e->type == ENT_PED) e->state = ST_FLEE; }
}

void fire_bullet_kind(uint16_t ang, uint8_t dmg, uint8_t kind) {
  for (int i = 0; i < MAX_BULLET; i++) {
    if (g_bullet[i].life) continue;
    Bullet *b = &g_bullet[i];
    // Fire crawls and dies fast; a rocket flies flat and far.
    int32_t spd  = (kind == B_FIRE) ? 26 : (kind == B_ROCKET) ? 46 : (kind == B_GRENADE) ? 16
                 : 55;
    // The grenade's "life" is a fuse, not a range: about a second and a half
    // at 25 fps, so there is time to get clear of your own throw - or to
    // panic about not having.
    int32_t life = (kind == B_FIRE) ?  9 : (kind == B_ROCKET) ? 34 : (kind == B_GRENADE) ? 28
                 : 14;
    b->x = g_p.x + fmul(60, fcos(ang));
    b->y = g_p.y + fmul(60, fsin(ang));
    b->vx = (int16_t)fmul(spd, fcos(ang));
    b->vy = (int16_t)fmul(spd, fsin(ang));
    b->life = (uint8_t)life;
    b->dmg = dmg;
    b->from_cop = 0;
    b->kind = kind;
    return;
  }
}
void fire_bullet(uint16_t ang, uint8_t dmg) {
  fire_bullet_kind(ang, dmg, B_SLUG);
}

// A little auto-aim. On a watch you steer your aim with two buttons, so
// guns snap to whichever person or vehicle sits nearest the MIDDLE of your
// aim - smallest angular offset wins, not smallest distance, so you can
// still pick the far target out of a crowd by pointing at it. The cone is
// about 28 degrees each side and it never fires backwards.

// Haptics, in one place. A tap for a connected punch, a longer buzz for
// gunfire and damage, a proper thump for a crash - and Quiet Time silences
// the lot, same as the synth.
// Armour first, then flesh. A vest does not make you invulnerable, it buys
// you the first hundred points of whatever is coming.
// Cycle to the next weapon you actually have. Fists are always available,
// so the cycle can never strand you with nothing.
// SELECT pressed and released. A tap is the action - attack, a door, a
// pickup; hold it for a third of a second and it is the car door: take
// the nearest vehicle, or get out of the one you are in. So you can stand
// beside a car and hit it with a bat without climbing in. Held-fire
// weapons still fire on a hold when there is no vehicle in reach.
// SELECT pressed and released. Near a vehicle (or in one) the press WAITS
// to see whether it is a tap - the action, so you can still bat a car -
// or a hold, the car door. Anywhere else the press fires at once: no lag
// on the trigger, and no rocket into the car you were about to get into.
static int venue_door_here(void);
// Standing in a room's doorway.
static bool at_room_door(void) {
  return g_p.inside && g_p.y > ROOM_OY + g_in.h - 70 && wr_abs(g_p.x - (ROOM_OX + g_in.w / 2)) < 110;
}
// The nearest vehicle you could take from here, or -1.
static int vehicle_here(void) {
  int idx = ent_nearest(ENT_CAR, 130);
  if (idx < 0) idx = ent_nearest(ENT_COPCAR, 130);
  if (idx < 0) idx = ent_nearest(ENT_AMB, 130);
  if (idx < 0) { idx = ent_nearest(ENT_HELI, 220); if (idx >= 0 && g_ent[idx].z > 0) idx = -1; }
  return (idx >= 0 && g_ent[idx].state != ST_DEAD) ? idx : -1;
}
static bool vehicle_in_reach(void) {
  if (g_p.in_car) return !airborne();      // in the air there is no door: the press is the trigger
  if (g_p.inside) return at_room_door();
  return vehicle_here() >= 0 || venue_door_here() != 0;
}
void game_input_hold(bool on) {
  if (on) {
    if (g_p.dead || g_p.inside || vehicle_in_reach()) { g_p.held = 1; return; }
    game_input_tap();                // fire now
    g_p.firing = 1;                  // and keep firing, for the weapons that do
    g_p.held = 200;                  // not a tap on release
    return;
  }
  if (g_p.held && g_p.held < 8) game_input_tap();
  g_p.held = 0;
  g_p.firing = 0;
}
void game_input_press(void) { game_input_hold(true); }
void game_input_release(void) { game_input_hold(false); }

// A press that did something contextual - opened a door, took a car, left
// the shop - must not also fire the gun in your hand, and must not leave the
// held-fire flag set so that it starts firing on the next frame either.
static void context_used(void) { g_p.firing = 0; }

void weapon_next(void) {
  if (on_heli()) { g_p.heli_wep = (uint8_t)((g_p.heli_wep + 1) % 3); wr_vibe(VIBE_LIGHT); return; }   // from LANDING (3) this goes to 0: lift-off
  if (g_p.in_car && g_ent[g_p.in_car - 1].sub == CAR_TANK && g_ent[g_p.in_car - 1].type == ENT_COPCAR) { g_p.heli_wep = (uint8_t)((g_p.heli_wep + 1) % 2); wr_vibe(VIBE_LIGHT); return; }
  g_p.inv[g_p.weapon % W_COUNT] = g_p.ammo;      // stash what is in hand
  g_p.inv[W_CASH] = g_p.inside ? 1 : 0;          // the money gun exists only indoors
  int w = g_p.weapon;
  for (int i = 0; i < W_COUNT; i++) {
    w = (w + 1) % W_COUNT;
    if (w == W_FIST || g_p.inv[w] > 0) break;
  }
  g_p.weapon = (uint8_t)w;
  g_p.ammo = g_p.inv[w];
  wr_vibe(VIBE_LIGHT);
}

void player_absorb(int32_t dmg) {
#if WR_GOD
  (void)dmg; return;                  // immortal, for testing
#endif
  if (dmg <= 0) return;
  if (g_p.armor) {
    int32_t soak = (dmg < (int32_t)g_p.armor) ? dmg : (int32_t)g_p.armor;
    g_p.armor = (uint8_t)(g_p.armor - soak);
    dmg -= soak;
    if (dmg <= 0) { g_p.hurt_flash = 4; return; }
  }
  // NOTE: this line must never be rewritten to call player_absorb() - it is
  // the one place that actually touches hp, and routing it through itself is
  // infinite recursion. A regex that replaced every hp-subtraction in the
  // codebase with a call to this function caught this line too, and the
  // result hung on the second unit test.
  g_p.hp = (uint8_t)wr_clamp((int32_t)g_p.hp - dmg, 0, HP_MAX);
}

void wr_vibe(uint8_t strength) {
  if (!g_menu.sfx_on) return;
  if (strength == VIBE_LIGHT) { vibes_short_pulse(); return; }
  static const uint32_t MED[]   = { 45 };
  static const uint32_t HEAVY[] = { 90, 40, 120 };
  VibePattern p = { .durations = (strength == VIBE_MED) ? MED : HEAVY,
                    .num_segments = (strength == VIBE_MED) ? 1u : 3u };
  vibes_enqueue_custom_pattern(p);
}

// The trigger. What happens depends on what is in your hands, and that
// code lives on the HANDS bench: only the weapon you hold is loaded. The
// kernel keeps the parts every weapon shares - cooldown, the swing of the
// arm, the noise it makes for the neighbours - and the tenant does the rest.
// Movement lives on the HANDS bench - see src/domains/movement.inc. The
// kernel keeps only the helicopter's cases and the dispatch.
WR_PART static void move_player(void) {
  if (on_heli()) { domain3_call(DOP_MOVE, 0); return; }    // the flying tenant
  if (g_in.pinned) { g_p.spd = 0; g_p.strafe = 0; g_p.phase = 0; return; }
  domain3_call(DOP_MOVE, 0);
}
WR_PART static void update_turn_and_speed(void) {
  if (on_heli()) { domain3_call(DOP_TURN, 0); return; }
  domain3_call(DOP_TURN, 0);
}
static void do_attack(void);
static void enter_or_exit_car(void) {
  if (g_p.in_car && on_heli()) {
    // On the ground and stopped, the hold is the door - and the aircraft
    // stays where you left it, ready for whoever wants it next.
    Ent *h = &g_ent[g_p.in_car - 1];
    if (h->z > 0 || g_p.spd) return;
    h->occupied = 0; h->state = ST_PARK; h->spd = 0;
    g_p.in_car = 0; g_p.spd = 0;
    audio_sfx(SFX_DOOR);
    return;
  }
  if (g_p.in_car) { domain_hands(); domain3_call(DOP_VEHICLE, 0); return; }   // the car tenant's door: out
  domain_board();                                                     // the boarding tenant's: in
  if (!g_p.in_car) do_attack();                                       // nothing to board: the weapon
}

WR_PART static void do_attack(void) {
  if (g_p.attack_cd) return;
  if (g_p.swim) return;              // both hands are busy staying afloat
  g_p.attack_cd = WEP_COOLDOWN[g_p.weapon];
  g_p.anim = 8;               // arm drives forward for eight frames

  if (g_p.weapon == W_CASH) { money_throw(); return; }
  inside_violence(false);
  if (g_p.weapon >= W_PISTOL) {
    g_p.hostile = 1;
    if ((g_frame & 31) == 0 && ent_any_cop_near(8 * TILE, false)) add_heat(25);
  }
  domain_hands();                    // the weapon may have changed since the frame began
  domain3_call(DOP_ATTACK, 0);
}




void game_input_action(void) {
  if (g_p.dead) { domain3_call(DOP_ATTACK, 0); return; }   // the dead tenant: the cinematic, then the respawn

  if (g_p.inside) {
    // Standing in the doorway: leave. Otherwise SELECT is the weapon -
    // and indoors the weapon is mostly money.
    if (at_room_door()) { context_used(); inside_exit(); return; }
    do_attack();
    return;
  }

  // A shop door you are standing at takes priority over anything else. The
  // door is no longer pinned to the +y face: lots sit on a lattice and a
  // door facing a neighbouring building was unenterable. The door side is
  // chosen by city_shop_door() - the first face with walkable ground
  // outside - and the same function places the sign, so they cannot drift
  // apart.
  if (!g_p.in_car) {
    int li = venue_door_here();
    if (li) {
      uint8_t k = g_lot[li - 1].kind;
      int32_t dx, dy, nx, ny;
      city_shop_door(&g_lot[li - 1], &dx, &dy, &nx, &ny);
      context_used();
      inside_enter(lot_is_home(&g_lot[li - 1]) ? IN_HOME : lot_is_airport(&g_lot[li - 1]) ? IN_AIRPORT : k == LOT_CLUB ? IN_CLUB : IN_GUNSHOP, dx, dy, nx, ny);
      return;
    }
  }

  enter_or_exit_car();
}

// The lot (index + 1) whose door you are standing at, or 0.
static int venue_door_here(void) {
  for (int li = 0; li < g_nlot; li++) {
    uint8_t k = g_lot[li].kind;
    bool ap = lot_is_airport(&g_lot[li]);
    if (k != LOT_GUNSHOP && k != LOT_CLUB && !ap && !lot_is_home(&g_lot[li])) continue;
    int32_t dx, dy, nx, ny;
    city_shop_door(&g_lot[li], &dx, &dy, &nx, &ny);
    if (wr_dist(g_p.x - dx, g_p.y - dy) < 150) return li + 1;
  }
  return 0;
}

// The tap. Like game_input_action() but it never touches a vehicle: in
// one it is the trigger (the helicopter's) or nothing; on foot it is the
// weapon or a door. Vehicles are the hold, in vehicle_action(). (The
// harness and the emulator build still use the combined action above.)
void game_input_tap(void) {
  if (!g_p.dead && g_p.inside && domain_call(DOP_ACTION, 0)) return;   // a counter, a bin, a gate
  if (g_p.dead || g_p.inside) { game_input_action(); return; }
  if (g_p.in_car) { if (on_heli()) heli_fire(); else domain2_call(DOP_ATTACK, 0); return; }   // the guard: your tank's guns
  if (venue_door_here()) { game_input_action(); return; }
  do_attack();
}

// The hold: out of the vehicle you are in, or into the nearest one.
// Returns false if there is nothing to do, so the hold can be a trigger.
bool vehicle_action(void) {
  if (g_p.dead) return false;
  // Indoors, the hold at the door leaves too: people who have learned that
  // doors work like cars should not be left wondering.
  if (g_p.inside) { if (at_room_door()) { inside_exit(); return true; } return false; }
  if (airborne()) return false;                        // the hold is the chain gun up there
  if (g_p.in_car) { enter_or_exit_car(); return true; }
  if (venue_door_here()) { game_input_action(); return true; }   // the hold goes in too
  if (vehicle_here() < 0) return false;
  enter_or_exit_car();
  return true;
}

// ---------------------------------------------------------------------------
// Step
// ---------------------------------------------------------------------------



// Flying. Any input lifts the aircraft to its cruising height; leave the
// stick alone for a couple of seconds and it settles onto whatever is
// underneath, which is how you land. Nothing on the ground can stop it
// while it is up.



WR_PART static void update_bullets(void) {
  for (int i = 0; i < MAX_BULLET; i++) {
    Bullet *b = &g_bullet[i];
    g_blame = 0; g_cop_fire = (b->from_cop & B_FROM_COP) != 0;
    if (!b->life) continue;
    b->x += b->vx;
    b->y += b->vy;
    b->life--;

    // A grenade is a fuse, not a projectile: it lobs, slows as it lands,
    // bounces off walls, and goes off when the fuse runs out, wherever it
    // is. It does not care who it passes through on the way.
    if (b->kind == B_GRENADE) {
      if (b->life < 14) { b->vx = (int16_t)((b->vx * 3) / 4); b->vy = (int16_t)((b->vy * 3) / 4); }
      if (city_blocked(b->x, b->y, false)) { b->x -= b->vx * 2; b->y -= b->vy * 2; b->vx = (int16_t)(-b->vx / 2); b->vy = (int16_t)(-b->vy / 2); }
      if (b->life == 0) { g_blame = (b->from_cop == 0); boom_add(b->x, b->y); g_blame = 0; }
      continue;
    }

    if (!(b->from_cop & 2) && city_blocked(b->x, b->y, false)) {
      if (b->kind == B_ROCKET) { g_blame = (b->from_cop == 0); boom_add(b->x, b->y); g_blame = 0; }
      b->life = 0;
      continue;
    }

    if ((b->from_cop & 1) && !airborne() && g_p.in_car) {
      // Rounds into a car go into the bodywork; you are hit only when the
      // car goes up around you.
      if (wr_dist(b->x - g_p.x, b->y - g_p.y) < 90) {
        Ent *pc = &g_ent[g_p.in_car - 1];
        if (pc->hp > b->dmg / 2) pc->hp = (uint8_t)(pc->hp - b->dmg / 2);
        else vehicle_destroy(g_p.in_car - 1, true);
        b->life = 0;
        continue;
      }
    }
    if (((b->from_cop & B_FROM_COP) || ((b->from_cop & B_FROM_MILITIA) && g_p.militia_hostile)) && !airborne()) {
      if (wr_dist(b->x - g_p.x, b->y - g_p.y) < 60) {
        player_absorb(9);
        g_p.hurt_flash = 5;
        wr_vibe(VIBE_MED);
        gib_burst(g_p.x, g_p.y, 50, b->vx, b->vy, 3);
        b->life = 0;
      }
      continue;
    }

    // Gunfire breaks things. Melee smashed fittings and cars flattened them,
    // but bullets passed straight through - so the glass cabinets in the gun
    // shop, the one place built to be shot up, could only be broken with a
    // bat. Rockets and grenades carry their blast radius; a slug just hits
    // what it touches.
    for (int j = 0; j < MAX_PROP; j++) {
      Prop *pr = &g_prop[j];
      if (!pr->alive || pr->loose) continue;
      if (wr_dist(pr->x - b->x, pr->y - b->y) > 44) continue;
      prop_hit(j, b->vx * 3, b->vy * 3,
               (b->kind == B_ROCKET) ? 200 : b->dmg);
      if (b->kind != B_ROCKET && b->kind != B_FIRE) { b->life = 0; break; }
    }
    if (!b->life) continue;

    for (int j = 0; j < MAX_ENT; j++) {
      Ent *e = &g_ent[j];
      if (e->state == ST_DEAD) continue;
      // Militia fire hits the police and their cars, nothing else here.
      if ((b->from_cop & B_FROM_MILITIA) && e->type != ENT_COP && e->type != ENT_COPCAR) continue;
      // Police fire hits militia on foot; the player is handled below.
      if ((b->from_cop & B_FROM_COP) && !((b->from_cop & B_FROM_MILITIA) == 0 &&
          e->type == ENT_PED && (uint8_t)e->aux == PA_MILITIA)) continue;

      g_blame = (b->from_cop == 0);                 // the player's round: their fault
      if (e->type == ENT_CAR || e->type == ENT_COPCAR) {
        // Bodywork soaks up a lot more than a person does, but keep shooting
        // and the tank goes.
        if (wr_dist(e->x - b->x, e->y - b->y) > 95) continue;
        // Whoever is behind the wheel floors it. Shooting at a car should
        // start a chase, not leave it trundling along at the speed limit.
        if (e->type == ENT_CAR) {
          e->state = ST_FLEE;
          e->timer = 0;
          if (e->spd < 22) e->spd = 22;
        }
        // NOTE: no instant detonation here. An earlier rule set fire to 1 on
        // any hit to a burning car - which meant the flamethrower's second
        // tongue blew up what its first tongue had just lit. Gunfire
        // shortens the fuse (below); only a COLLISION sets a burning car off
        // on the spot, which is in car_collisions().
        // Fire catches, exactly as it does on people; and any hit on a car
        // that is ALREADY alight brings the explosion forward, so shooting a
        // burning wreck is a way to time it rather than a waste of rounds.
        if (b->kind == B_FIRE) {
          // Flame lights it and then leaves the fuse alone. Shortening it on
          // every tongue meant a held flamethrower burned a 200-frame fuse
          // to nothing in about a second - the car detonated instantly
          // instead of catching fire, which is the opposite of the point.
          if (!e->fire) vehicle_ignite(j);
        } else if (e->fire > 14) {
          e->fire -= 12;                 // a hit brings the bang ~half a second closer
        }
        if (b->kind == B_ROCKET) {
          // Two separate events. The rocket detonates where it struck - its
          // own blast, damaging everything around it - and the car it hit is
          // thrown: up, out, tumbling, alight, and on a short fuse. It
          // bounces off the road a couple of times before it goes.
          if (e->state != ST_DEAD) {
            if (b->from_cop == 0) add_heat(2 * HEAT_KILL);   // the player's rocket: two bodies' worth
            e->state = ST_DEAD;
            e->spd = 0;
            e->hp = 0;
            vehicle_ignite(j);
            e->fire = (uint8_t)(40 + wr_rand_n(45));   // seconds, not minutes
            e->z = 20;
            e->vz = (int16_t)(22 + wr_rand_n(10));
            e->vx = (int16_t)(b->vx + (int32_t)wr_rand_n(6) - 3);
            e->vy = (int16_t)(b->vy + (int32_t)wr_rand_n(6) - 3);
            e->spin = (int16_t)(2000 + wr_rand_n(3000));
          }
          g_blame = (b->from_cop == 0); boom_add(b->x, b->y); g_blame = 0;   // after the launch: the blast would otherwise wreck it first
          b->life = 0;
          continue;
        }
        uint8_t d = (uint8_t)(b->dmg / 3);
        if (e->hp > d) {
          e->hp -= d;
        } else {
          { g_blame = (b->from_cop == 0); vehicle_destroy(j, true); g_blame = 0; }   // the heat is vehicle_destroy's: two bodies' worth
        }
        b->life = 0;
        break;
      }

      if (e->type != ENT_PED && e->type != ENT_COP) continue;
      if (wr_dist(e->x - b->x, e->y - b->y) < (b->kind == B_FIRE ? 70 : 55)) {
        // Set alight BEFORE taking the damage, so the very first tongue is
        // already a burn. The other order let the opening hit draw blood -
        // invisible while the debris pool was small enough to recycle those
        // droplets before they stained, and plainly visible once the pool
        // was given room.
        if (b->kind == B_FIRE && e->state != ST_DEAD) e->fire = 90;
        // A tongue of flame burns every frame it overlaps someone; at full
        // rate that killed a man in six frames, before he could run. The
        // burn is meant to do the killing, slowly, while he blunders about.
        if (b->kind != B_FIRE || (g_frame & 3) == 0) damage_entity(e, b->dmg);
        if (b->kind == B_ROCKET) boom_add(b->x, b->y);
        // Flame keeps burning through whoever it touches.
        if (b->kind != B_FIRE) b->life = 0;
        break;
      }
    }
  }
  g_cop_fire = 0;
}

WR_PART static void update_pickups(void) {
  for (int i = 0; i < MAX_PICKUP; i++) {
    if (!g_pickup[i].alive) continue;
    if (wr_dist(g_pickup[i].x - g_p.x, g_pickup[i].y - g_p.y) > 90) continue;
    if (g_pickup[i].weapon == PICKUP_ARMOR || g_pickup[i].weapon == PICKUP_CASH) {
      if (g_pickup[i].weapon == PICKUP_ARMOR) g_p.armor = ARMOR_MAX;
      else g_p.cash = (uint16_t)wr_clamp(g_p.cash + g_pickup[i].world, 0, 9999);
      g_pickup[i].alive = 0;
      audio_sfx(SFX_PICKUP);
      continue;                       // neither is a weapon swap
    }
    // Into the bag, not over the top of what you are already carrying.
    {
      uint8_t w = g_pickup[i].weapon % W_COUNT;
      int32_t add = (w == W_SHOTGUN) ? 24 : (w == W_MG)     ? 120
                  : (w == W_FLAME)   ? 180 : (w == W_ROCKET) ? 10  : (w == W_GRENADE) ? 6 : (w == W_PISTOL) ? 48 : 255;
      g_p.inv[g_p.weapon % W_COUNT] = g_p.ammo;       // stash the old one
      int32_t have = g_p.inv[w] + add;
      g_p.inv[w] = (uint8_t)(have > 255 ? 255 : have);
      g_p.weapon = w;
      g_p.ammo = g_p.inv[w];
      g_pickup[i].alive = 0;
      audio_sfx(SFX_PICKUP);
      continue;
    }
    // Doubled across the board. Running dry three streets from the nearest
    // pickup turned a firefight into a walk.
    g_p.ammo = (g_p.weapon == W_SHOTGUN) ? 24
             : (g_p.weapon == W_MG)      ? 120
             : (g_p.weapon == W_FLAME)   ? 180
             : (g_p.weapon == W_ROCKET)  ? 10
             : (g_p.weapon == W_GRENADE) ? 6
             : (g_p.weapon == W_PISTOL)  ? 48 : 0;
    g_pickup[i].alive = 0;
    audio_sfx(SFX_PICKUP);
  }
}

// Escaping: no police within COP_SIGHT and the heat bleeds off fast. Sit in
// their line of sight and it holds where it is.
// Nearest officer on foot, or -1.

// The arrest lives on the THREAT bench, at one to three stars - the slot
// between the taxi (zero) and air support (four and up). At zero stars
// nobody cuffs you; at four they have stopped trying.
WR_PART static void update_arrest(void) { domain2_call(DOP_ARREST, 0); }

WR_PART static void update_wanted(void) {
  // Heat only bleeds off once you have been out of sight long enough for
  // dispatch to lose you. Stay visible and the level holds where it is.
  // Cover. A parking garage has a roof and columns, and the game has always
  // claimed that ducking into one is the cheapest way to shake a tail - but
  // evasion only ever measured distance, so the claim was not true and at
  // four stars, with a helicopter holding station overhead, there was no
  // reliable way to break contact at all. Under a deck the police have to
  // be almost on top of you to keep eyes on, which turns a garage into an
  // actual tactic and gives the wanted system a way down that does not
  // depend on simply out-driving five cars and an aircraft.
  // Time the chase, so a clean getaway can be scored. The clock starts the
  // moment you are wanted and stops when the last star goes.
  if (g_p.wanted >= 3 && g_p.hot < 65000) g_p.hot++;
  if (!g_p.wanted) { g_p.hostile = 0; g_p.copkills = 0; g_p.hot = 0; }

  bool cover = player_under_cover();
  int32_t sight = cover ? (2 * TILE) : COP_SIGHT;
  if (ent_any_cop_near(sight, !cover)) { g_p.evade = 0; return; }
  if (g_p.evade < 0xFFFF) g_p.evade++;
  if (g_p.evade < EVADE_CLEAR) return;

  if (g_p.heat && (g_frame & HEAT_DECAY_MASK) == 0) g_p.heat--;

  uint8_t nw = (uint8_t)(g_p.heat / HEAT_PER_STAR);
  if (nw > 5) nw = 5;
  if (nw < g_p.wanted) {
    g_p.wanted = nw;
    audio_sfx(SFX_ESCAPE);
  }
}

void game_step(void) {
  // Paused: the menu owns the PLACE bench, and nothing below must take it
  // back - which is exactly what domain_scenery() did, every frame, so the
  // pause screen was black with no menu on it.
  if (g_menu.open) { audio_tick(); return; }
  domain_scenery();                   // the island you are on decides the look
  domain_threat();                    // the heat decides who is coming
  domain_hands();                     // and the weapon decides the trigger
  if (g_p.copkill_t && !--g_p.copkill_t) g_p.kwin = 0;          // the reinforcements window closes
  if (g_p.lull && !--g_p.lull) add_heat(HEAT_PER_STAR);         // they are back, with more
  // The simulation owns the clock, not the renderer - a dropped frame must
  // not desynchronise cooldowns, heat decay or the audio gate.
  g_frame++;

  if (g_menu.open) { audio_tick(); return; }

  if (g_p.attack_cd) g_p.attack_cd--;
  if (g_p.anim) g_p.anim--;
  if (g_p.hurt_flash) g_p.hurt_flash--;

  // Stride rate follows how fast he is actually travelling, including
  // sidestepping, so the feet never skate.
  if (!g_p.in_car) {
    int32_t v = wr_abs(g_p.spd) + wr_abs(g_p.strafe);
    if (v) {
      uint8_t before = g_p.phase;
      // Walking backwards runs the cycle in reverse, so the feet never skate.
      g_p.phase = (uint8_t)(g_p.phase + (g_p.spd < 0 ? -(3 + v * 2) : (3 + v * 2)));
      if ((before ^ g_p.phase) & 0x80) audio_sfx(SFX_FOOTSTEP);
    }
  }

  // You press to get in, so you press to get out. Walking through the wall
  // used to dump you on the street, which meant entering and leaving worked
  // by different rules - and it only existed as a rescue from the doorway
  // gap, which is filled in now. If something does throw you clear of the
  // room, put you back inside rather than silently ending the visit: there
  // is no ground out there to stand on.
  if (g_p.inside) {
    inside_update();
    // The room may have let you out just now (a flight lands you on
    // another island): do not clamp a player who is no longer in it.
    if (!g_p.inside) goto outside;
    int32_t lo_x = ROOM_OX + 24, hi_x = ROOM_OX + g_in.w - 24;
    int32_t lo_y = ROOM_OY + 24, hi_y = ROOM_OY + g_in.h - 24;
    if (g_p.x < lo_x || g_p.x > hi_x || g_p.y < lo_y || g_p.y > hi_y) {
      g_p.x = wr_clamp(g_p.x, lo_x, hi_x);
      g_p.y = wr_clamp(g_p.y, lo_y, hi_y);
      g_p.spd = 0;
    }
  }
outside:

  // Held fire, for the weapons that should have it. The flamethrower is the
  // point of this - a flame you have to tap for is not a flamethrower - and
  // the machine gun gets it for the same reason. Everything else stays one
  // press, one shot, because a pistol that empties itself while your thumb
  // rests on the button is not a feature.
  if (g_p.firing && !g_p.dead && !g_p.attack_cd) {
    if (airborne()) { if (g_p.heli_wep == 0) heli_fire(); }    // held: only the chain gun streams
    else if (g_p.in_car && g_ent[g_p.in_car - 1].sub == CAR_TANK && g_ent[g_p.in_car - 1].type == ENT_COPCAR && g_p.heli_wep == 0) do_attack();
    else if (g_p.weapon == W_FLAME || g_p.weapon == W_MG || g_p.weapon == W_CASH) do_attack();
  }
  if (g_p.held && g_p.held < 200) {
    g_p.held++;
    if (g_p.held == 8) vehicle_action();       // the hold: the car door
  }
  taxi_update();
  if (g_p.loading) g_p.loading--;
  if (g_p.caption_t) g_p.caption_t--;
  // Under a parking deck with heat on: say what the deck is for.
  else if (g_p.wanted > 0 && (g_frame & 63) == 0 && player_under_cover())
    game_caption(CAP_GARAGE, 50);


  if (g_p.dead) {
    g_p.hurt_flash = 2;      // hold the vignette through the cinematic
    if (g_p.cine < 60000) g_p.cine++;
    audio_tick();
    return;
  }

  update_turn_and_speed();
  move_player();

  update_bullets();
  update_pickups();
  ent_update();
  props_update();
  update_arrest();
  update_wanted();

  if (g_p.hp == 0 && !g_p.dead) {
    g_p.dead = 1;
    g_p.cine = 0;
    audio_sfx(SFX_BUSTED);
    blood_add(g_p.x, g_p.y, 36);
  }

  audio_tick();
}
