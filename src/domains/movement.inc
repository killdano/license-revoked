// ---------------------------------------------------------------------------
// The player's movement, boarding and driving. ONE source, compiled twice:
// with DRIVING 0 into each weapon tenant of the HANDS bench (walking,
// swimming, getting into a vehicle), and with DRIVING 1 into the car
// tenant (driving, crashing, getting out). The compiler drops the half
// that cannot run, so neither tenant carries the other's code, and the
// kernel carries neither. The helicopter is flown by the air domain and
// its door is handled by the kernel; boats are compiled out.
// ---------------------------------------------------------------------------
#include "wr_tune.h"
#ifndef DRIVING
#error "movement.inc needs DRIVING defined as 0 or 1"
#endif
#ifndef NO_VEHICLE_ACTION
#define NO_VEHICLE_ACTION() ((void)0)
#endif
// BOARD_ONLY: just the door (getting in), for the boarding tenant; NO_DOOR:
// everything but the door, for the weapon tenants, which never board -
// the kernel swaps in the boarding tenant for the hold.
#ifndef BOARD_ONLY
#define BOARD_ONLY 0
#endif
#ifndef NO_DOOR
#define NO_DOOR 0
#endif
#if !BOARD_ONLY
#define kit(i) ((void)0)              // only the boarding tenant hands out the aircraft's kit
#endif
#ifndef HELI
#define HELI 0
#endif
#define on_heli() (HELI)
#define on_boat() (false)

// Boats and everything else have opposite ideas about what is passable.
#if !HELI && !BOARD_ONLY
// Is (x,y) inside a car's footprint, padded by `pad`?
static bool in_car_box(const Ent *e, int32_t x, int32_t y, int32_t pad) {
  int32_t hl, hw; car_half_dims(e, &hl, &hw); hl += pad; hw += pad;
  int32_t c = fcos(e->ang), s = fsin(e->ang);
  int32_t rx = x - e->x, ry = y - e->y;
  int32_t along = (rx * c + ry * s) >> 10, side = (ry * c - rx * s) >> 10;
  return wr_abs(along) < hl && wr_abs(side) < hw;
}
static bool blocked_for_player(int32_t x, int32_t y) {
  if (on_boat()) return water_blocked(x, y);
  // On foot, open water is not a wall - you swim. Everything else about
  // collision is unchanged, so the shoreline, the buildings and the props
  // all still stop you exactly as they did.
  if (!g_p.in_car && cell_at_point(x, y) == CELL_WATER) return false;
  if (g_p.jump) return false;                       // in the air: nothing to hit
  if (!g_p.in_car)
    for (int i = 0; i < MAX_ENT; i++) {
      Ent *e = &g_ent[i];
      if (e->type != ENT_CAR && e->type != ENT_COPCAR && e->type != ENT_AMB) continue;
      // A car's footprint, as a box in its own frame: you walk round it.
      // Only stepping INTO the box is blocked; if you are already in it
      // (thrown clear of a wreck, say) any step out is allowed, or you
      // would be stuck inside it for good.
      if (!in_car_box(e, x, y, 10)) continue;
      if (in_car_box(e, g_p.x, g_p.y, 10)) continue;        // already inside: let him out
      return true;
    }
  return city_blocked(x, y, g_p.in_car != 0);
}
#endif


#if !BOARD_ONLY
static void update_turn_and_speed(void) {
  const bool driving = DRIVING; (void)driving;
  const bool afloat = false; (void)afloat;
  int32_t tmax = driving ? TURN_MAX_CAR : TURN_MAX_FOOT;

  // Steering is buttons only, in a car and on foot alike.
  int32_t want = g_p.turn_dir;

  if (want) {
    g_p.turn = (int16_t)wr_clamp(g_p.turn + want * TURN_ACCEL, -tmax, tmax);
  } else {
    if (g_p.turn > 0)      g_p.turn = (int16_t)(g_p.turn > TURN_FRICTION ? g_p.turn - TURN_FRICTION : 0);
    else if (g_p.turn < 0) g_p.turn = (int16_t)(g_p.turn < -TURN_FRICTION ? g_p.turn + TURN_FRICTION : 0);
  }

  // A car only turns when it is actually rolling, and the yaw rate follows
  // SIGNED velocity - so backing up swings the nose the other way, the same
  // as reversing a real car. Normalising against the reverse top speed means
  // full reverse steers as hard as full forward, just mirrored, instead of
  // going vague because reverse gear is slower.
  int32_t applied = g_p.turn;
#if WR_ANDROID
  // The phone's steering wheel: a position, not a button. When it is held
  // (or still returning to centre) it sets the turn directly - a wheel at
  // half lock is half the car's steering authority - in place of the
  // button integrator above.
  if (driving && g_p.wheel) applied = (g_p.wheel * tmax) / 256;
#endif
  const bool heli = false; (void)heli;
  if (heli) {
    // Answers the cyclic at any speed, like a boat but crisper.
  } else if (afloat) {
    // Steering by thrust: it answers the helm at any speed, and reversing
    // still swings the bow the other way.
    int32_t v = g_p.spd;
    if (g_p.stick_f > 0 && v >= 0 && v < 10) v = 10;
    if (g_p.stick_f < 0 && v <= 0 && v > -8) v = -8;
    applied = (applied * v) / SPD_MAX_BOAT;
  } else if (driving) {
    int32_t ref = (g_p.spd < 0) ? SPD_REV_CAR : SPD_MAX_CAR;
    int32_t v = g_p.spd;
    // Wedged against scenery, a blocked move zeroes the speed every frame.
    // Without a floor on steering authority the car could never turn out of
    // the corner it is stuck in, so full throttle keeps a little grip even at
    // a standstill. Not enough to pivot a parked car with no input.
    if (g_p.stick_f > 0 && v >=  0 && v <  7) v =  7;
    if (g_p.stick_f < 0 && v <=  0 && v > -7) v = -7;
    applied = (applied * v) / ref;
  }
  g_p.ang = (uint16_t)(g_p.ang + applied);

  // Throttle is analogue now: the stick asks for a speed and we accelerate
  // toward it, so cars keep their momentum and people feel responsive.
  int32_t fmax = heli ? SPD_MAX_HELI : afloat ? SPD_MAX_BOAT : driving ? SPD_MAX_CAR  : SPD_MAX_FOOT;
#if DRIVING && !HELI
  if (g_p.in_car && g_ent[g_p.in_car - 1].sub == CAR_TANK && g_ent[g_p.in_car - 1].type == ENT_COPCAR) fmax = 14;   // a tank is not quick
#endif
  int32_t bmax = heli ? 10           : afloat ? SPD_REV_BOAT : driving ? SPD_REV_CAR  : SPD_BACK_FOOT;

  // Hull character. A jet ski will outrun anything, a yacht will not.
  if (afloat) {
    static const int8_t HULL[BOAT_MODEL_COUNT] = { 100, 78, 55, 92 };
    int32_t k = HULL[g_ent[g_p.in_car - 1].sub % BOAT_MODEL_COUNT];
    fmax = (fmax * k) / 100;
    bmax = (bmax * k) / 100;
  }

  // Off the tarmac a car simply cannot reach its road top speed. Capping the
  // TARGET rather than hacking at the current speed means throttle still
  // responds normally, you just top out lower - and you coast down to it over
  // about a second instead of hitting a wall.
  int32_t road_f = fmax, road_b = bmax;
  if (driving && !afloat && !heli) {
    uint8_t surf = cell_at_world(g_p.x, g_p.y);
    if (surf == CELL_SAND)      { fmax = (fmax * 60) / 100; bmax = (bmax * 60) / 100; }
    else if (surf != CELL_ROAD) { fmax = (fmax * 75) / 100; bmax = (bmax * 75) / 100; }
  }
  int32_t target = (g_p.stick_f >= 0)
                 ? ( g_p.stick_f * fmax) >> 8
                 : (-((-g_p.stick_f) * bmax)) >> 8;

  int32_t acc  = driving ? ACC_CAR : ACC_FOOT;
  int32_t drag = driving ? DRAG_CAR : DRAG_FOOT;
  int32_t step = (target == 0) ? drag : acc;

  // Above the surface's top speed, only rolling resistance (in
  // move_player) brings you down: the throttle step used to do it too, and
  // that was the wall at the edge of the sand. The clamp is to the ROAD
  // limits so the speed you carried onto the beach is not chopped off.
  if (g_p.spd < target)      g_p.spd = (int16_t)wr_clamp(g_p.spd + step, -road_b, road_f);
  else if (g_p.spd > target && g_p.spd <= fmax && g_p.spd >= -bmax)
                             g_p.spd = (int16_t)wr_clamp(g_p.spd - step, -road_b, road_f);
  else if (g_p.spd > target && target > 0 && g_p.spd > fmax) { /* coasting down to it */ }
  else if (g_p.spd > target) g_p.spd = (int16_t)wr_clamp(g_p.spd - step, -road_b, road_f);

  // Sidestepping is a person-only move; it responds immediately.
  if (driving) {
    g_p.strafe = 0;
  } else {
    int32_t st = (g_p.stick_s * STRAFE_MAX) >> 8;
    g_p.strafe = (int16_t)wr_clamp(st, -STRAFE_MAX, STRAFE_MAX);
  }
}
#endif
#if !HELI && !BOARD_ONLY



#if !DRIVING
// Hit by a car. You take its speed and go over the bonnet: up, along, and
// down again, bouncing on the tarmac and leaving blood at every impact,
// with the damage the speed deserves. Nothing you press does anything
// until you have stopped moving. Cars are solid on foot, so this is the
// only way one gets to touch you.
static void knock_start(const Ent *car) {
  int32_t spd = wr_abs(car->spd);
  int32_t c = fcos(car->ang), s = fsin(car->ang);
  int32_t dir = car->spd >= 0 ? 1 : -1;
  g_p.knock = 1;
  g_p.kvx = (int16_t)((spd * c * dir * 5) >> 12);       // most of the car's speed, along it
  g_p.kvy = (int16_t)((spd * s * dir * 5) >> 12);
  int32_t rx = g_p.x - car->x, ry = g_p.y - car->y;
  int32_t side = (ry * c - rx * s) >> 10;                  // thrown a little to the side you were on
  g_p.kvx = (int16_t)(g_p.kvx - ((s * (side > 0 ? 6 : -6)) >> 10));
  g_p.kvy = (int16_t)(g_p.kvy + ((c * (side > 0 ? 6 : -6)) >> 10));
  g_p.kz = 8; g_p.kvz = (int16_t)(18 + spd);
  g_p.kspin = (uint16_t)(4000 + wr_rand_n(4000));
  g_p.spd = 0; g_p.strafe = 0;
  player_absorb(spd * 2);
  gib_burst(g_p.x, g_p.y, 30, g_p.kvx, g_p.kvy, 6);
  audio_sfx(SFX_RUNOVER);
  wr_vibe(VIBE_HEAVY);
}

static bool knock_step(void) {
  if (!g_p.knock) return false;
  int32_t nx = g_p.x + g_p.kvx, ny = g_p.y + g_p.kvy;
  if (city_blocked(nx, ny, false)) { g_p.kvx = (int16_t)(-g_p.kvx / 2); g_p.kvy = (int16_t)(-g_p.kvy / 2); gib_burst(g_p.x, g_p.y, 20, 0, 0, 3); }
  else { g_p.x = nx; g_p.y = ny; }
  if (g_p.kz > 0 || g_p.kvz > 0) { g_p.kz = (int16_t)(g_p.kz + g_p.kvz); g_p.kvz = (int16_t)(g_p.kvz - 6); }   // gravity only while off the ground
  g_p.ang = (uint16_t)(g_p.ang + g_p.kspin / 8);
  if (g_p.kz <= 0) {
    g_p.kz = 0;
    if (g_p.kvz < -20) {                                   // a real landing: bounce, blood, hurt
      g_p.kvz = (int16_t)(-g_p.kvz / 3);                   // (a third: a half hops forever)
      gib_burst(g_p.x, g_p.y, 4, g_p.kvx / 2, g_p.kvy / 2, 4);
      player_absorb(4);
      audio_sfx(SFX_PUNCH);
    } else g_p.kvz = 0;
    g_p.kvx = (int16_t)((g_p.kvx * 3) / 4); g_p.kvy = (int16_t)((g_p.kvy * 3) / 4);
    g_p.kspin = (uint16_t)((g_p.kspin * 3) / 4);
    if (!g_p.kvz && wr_abs(g_p.kvx) + wr_abs(g_p.kvy) < 2) { g_p.knock = 0; g_p.kvx = g_p.kvy = 0; g_p.knock_grace = 50; }   // up you get; two seconds' grace
  }
  return true;
}

#endif

static void move_player(void) {
  const bool driving = DRIVING; (void)driving;
#if !DRIVING
  if (knock_step()) return;
  if (g_p.knock_grace) g_p.knock_grace--;
  // A moving car whose box you are in has just hit you - unless you have
  // only just got up, in which case it does not get to do it again yet.
  for (int i = 0; g_p.knock_grace == 0 && i < MAX_ENT; i++) {
    Ent *e = &g_ent[i];
    if ((e->type != ENT_CAR && e->type != ENT_COPCAR && e->type != ENT_AMB) || e->state == ST_DEAD || wr_abs(e->spd) < 8) continue;
    if (in_car_box(e, g_p.x, g_p.y, 8)) { knock_start(e); return; }
  }
#endif
  if (g_in.pinned) { g_p.spd = 0; g_p.strafe = 0; g_p.phase = 0; return; }

  // Swimming. Slower than a walk, and you cannot fight while you are doing
  // it - which is the whole point of the trade: the water is a way out of
  // a chase and a way onto a moored boat, not a way to win a gunfight.
  if (!driving) {
    g_p.swim = (cell_at_point(g_p.x, g_p.y) == CELL_WATER) ? 1 : 0;
    if (g_p.swim) {
      int32_t cap = 5;
      if (g_p.spd > cap) g_p.spd = (int16_t)cap;
      if (g_p.spd < -cap) g_p.spd = (int16_t)-cap;
      if (g_p.strafe > 3) g_p.strafe = 3;
      if (g_p.strafe < -3) g_p.strafe = -3;
      // A wake, so it reads as swimming rather than walking on water.
      if ((g_frame & 7) == 0 && (g_p.spd || g_p.strafe))
        gib_burst_debris(g_p.x, g_p.y, 6, 0, 0, 1, ARGB(2,2,3));
    }
  } else {
    g_p.swim = 0;
  }
  if (!g_p.spd && !g_p.strafe) return;

  int32_t c = fcos(g_p.ang), s = fsin(g_p.ang);
  // Forward is the facing direction; strafe is perpendicular to it, so on
  // foot you can circle a target while still aiming at it. Each term is
  // rounded symmetrically - see fmul() - or a diagonal heading walks you off
  // the centreline a unit at a time.
  int32_t dx = fmul(g_p.spd, c) - fmul(g_p.strafe, s);
  int32_t dy = fmul(g_p.spd, s) + fmul(g_p.strafe, c);

  int32_t nx = g_p.x + dx, ny = g_p.y + dy;

  // The island's scenery gets first refusal on a driving move: the ghetto
  // owns the collapsed bridge, and answers 1 when the car is in the air or
  // has just gone into the sea.
#if DRIVING && !HELI
  if (drive(dx, dy)) return;              // the bridge jump (this tenant's own)
#endif

  if (blocked_for_player(nx, ny)) {
    // Slide along one axis so you scrape walls instead of sticking to them.
    if (!blocked_for_player(nx, g_p.y))      { g_p.x = nx; }
    else if (!blocked_for_player(g_p.x, ny)) { g_p.y = ny; }
    else {
      if (driving && wr_abs(g_p.spd) > 14) audio_sfx(on_boat() ? SFX_SMASH : SFX_CRASH);
      g_p.spd = 0;
      g_p.strafe = 0;
      return;
    }
    if (driving) g_p.spd = (int16_t)(g_p.spd / 2);
  } else {
    g_p.x = nx; g_p.y = ny;
  }

  if (driving) {
    Ent *car = &g_ent[g_p.in_car - 1];
    car->x = g_p.x; car->y = g_p.y; car->ang = g_p.ang; car->spd = g_p.spd;
    int32_t hc = fcos(g_p.ang), hs = fsin(g_p.ang);

    // Off the tarmac you lose grip. This used to slam the speed to a hard cap
    // the instant a wheel left the road, which felt like hitting a wall; now
    // it is rolling resistance, so you wash off speed over a second or so and
    // can still carry momentum across a kerb or along a beach.
    uint8_t surf = cell_at_world(g_p.x, g_p.y);
    if (surf != CELL_ROAD) {
      // Rolling resistance, applied on alternate frames. A whole unit every
      // frame is 25 a second against a top speed of 30 - that stopped you
      // dead in about a second, which is exactly the wall this was meant to
      // remove. Half that still costs you real speed over a long beach run
      // without feeling like a handbrake.
      // Bleed the excess off gradually. The top speed itself is lowered in
      // update_turn_and_speed; this is just how fast you settle down to it.
      int32_t cap = (surf == CELL_SAND) ? (SPD_MAX_CAR * 60) / 100
                                        : (SPD_MAX_CAR * 75) / 100;
      if ((g_frame & 1) == 0) {
        if (g_p.spd > cap)       g_p.spd--;
        else if (g_p.spd < -cap) g_p.spd++;
      }
    }

    // Street furniture. Hitting it at speed shears it off, scatters debris
    // and costs you bodywork; a gentle nudge just rocks it.
    {
      int32_t v = wr_abs(g_p.spd);
      if (v > 4) {
        for (int i = 0; i < MAX_PROP; i++) {
          Prop *pr = &g_prop[i];
          if (!pr->alive || pr->loose) continue;
          if (wr_dist(pr->x - g_p.x, pr->y - g_p.y) > 120) continue;
          prop_hit(i, (hc * v) >> 10, (hs * v) >> 10, v);
          Ent *pc = &g_ent[g_p.in_car - 1];
          uint8_t dmg = (uint8_t)wr_clamp(v / 8, 1, 10);      // street furniture dents, it does not wreck
          if (pc->hp > dmg) pc->hp -= dmg;
          else vehicle_destroy(g_p.in_car - 1, true);
          g_p.spd = (int16_t)((g_p.spd * 7) / 8);
        }
      }
    }

    // Shop windows give way if you hit them hard enough.
    if (wr_abs(g_p.spd) > 12) {
      int li = lot_at(g_p.x, g_p.y);
      if (li >= 0 && g_lot[li].kind == LOT_STORE && g_lot[li].glass < 200) {
        g_lot[li].glass = 255;
        gib_burst_debris(g_p.x, g_p.y, 60, (hc * g_p.spd) >> 10,
                         (hs * g_p.spd) >> 10, 10, 0xDB /* glass */);
        audio_sfx(SFX_GLASS);
        g_p.spd = (int16_t)((g_p.spd * 2) / 3);
      }
    }

    // Running people down. This is the main way to attract attention.
    if (wr_abs(g_p.spd) > 6) {
      for (int i = 0; i < MAX_ENT; i++) {
        Ent *e = &g_ent[i];
        if (e->type != ENT_PED && e->type != ENT_COP) continue;
        if (e->state == ST_DEAD) continue;
        if (wr_dist(e->x - g_p.x, e->y - g_p.y) < 135) {
          // Throw the body off the bonnet along our heading, with a bit of
          // lift and a tumble. The ragdoll takes it from here.
          int32_t v = wr_abs(g_p.spd);
          kill_entity(e);
          audio_sfx(SFX_RUNOVER);
          wr_vibe(VIBE_MED);
          // Over the bonnet, not under the bumper: half again the carry, a
          // real launch, and a tumble that keeps going. The extra height is
          // what buys the extra bounces on the way down.
          e->vx = (int16_t)((hc * (v + v / 2)) >> 10);
          e->vy = (int16_t)((hs * (v + v / 2)) >> 10);
          e->vz = (int16_t)(13 + v / 2);
          e->z  = 10;
          e->spin = (int16_t)(2600 + wr_rand_n(4200));
          gib_burst(e->x, e->y, 40, e->vx, e->vy, 8);
        }
      }
    }
  }
}
#endif
#if !HELI && !NO_DOOR


static void enter_or_exit_car(void) {
  // Each tenant has only its own door: the car tenant gets out, the foot
  // tenants get in. `driving` is a compile-time constant here, so the other
  // half is not in the blob.
  if (on_heli()) {
    // On the ground and stopped, the hold is the door - and the aircraft
    // stays where you left it, ready for whoever wants it next. In the
    // air the hold is the trigger, handled before we get here.
    Ent *h = &g_ent[g_p.in_car - 1];
    if (h->z > 0 || g_p.spd) return;
    h->occupied = 0; h->state = ST_PARK; h->spd = 0;
    g_p.x = h->x + fmul(110, fcos((uint16_t)(h->ang + 16384)));
    g_p.y = h->y + fmul(110, fsin((uint16_t)(h->ang + 16384)));
    g_p.in_car = 0; g_p.spd = 0; g_p.strafe = 0;
    audio_sfx(SFX_DOOR);
    return;
  }
  if (DRIVING) {
    Ent *car = &g_ent[g_p.in_car - 1];
    car->occupied = 0;
    car->spd = 0;
    car->door = 64;
    // Parked, not abandoned to the traffic AI. Stepping out used to hand the
    // car straight back to `update_car()`, which found an empty vehicle in a
    // lane and drove it away - so the car you left at the kerb was gone when
    // you turned round. ST_PARK is the state the bays already use: a real
    // vehicle, collidable and stealable, with nobody driving it.
    if (car->state != ST_DEAD) car->state = ST_PARK;
    // The driver's scratch (a junction point in vx/vy) must not be read
    // as the park anchor, or the car pins itself to a garbage position
    // sixty-four times too far away - that was "the car disappears".
    car->vx = (int16_t)(car->x >> 6); car->vy = (int16_t)(car->y >> 6);
    car->vz = 0; car->wep = TURN_NONE; car->spin = 0;
    audio_sfx(SFX_DOOR);

    if (ent_is_boat(car->type)) {
      // Find a shore to step onto. If there is none within reach you simply
      // cannot get out - stepping off in open water would leave the player
      // standing on the sea, which nothing else in the game has a concept of.
      bool landed = false;
      for (int r = 1; r <= 14 && !landed; r++) {
        for (int k = 0; k < 12; k++) {
          uint16_t a = (uint16_t)(k * 5461);
          int32_t tx = car->x + fmul(r * 80, fcos(a));
          int32_t ty = car->y + fmul(r * 80, fsin(a));
          if (!city_blocked(tx, ty, false) && cell_at_point(tx, ty) != CELL_WATER) {
            g_p.x = tx; g_p.y = ty; landed = true; break;
          }
        }
      }
      if (!landed) {
        // Stay aboard, and undo the door so it does not look like the input
        // was swallowed.
        car->occupied = 1;
        car->door = 0;
        return;
      }
    } else {
      // Step out beside the car.
      g_p.x = car->x + fmul(90, fcos((uint16_t)(car->ang + 16384)));
      g_p.y = car->y + fmul(90, fsin((uint16_t)(car->ang + 16384)));
    }

    g_p.in_car = 0;
    g_p.spd = 0;
    g_p.strafe = 0;
    return;
  }

  // You have to be right on the door. Anything looser and standing near a
  // parked car would swallow every attack input.
  int idx = ent_nearest(ENT_CAR, 115);
  bool cop_car = false;
  if (idx < 0) { idx = ent_nearest(ENT_COPCAR, 115); cop_car = true; }
  // Boats have a longer reach: you step aboard from a dock or off the shore,
  // and demanding you stand on the water first would be unplayable.
  // Treading water beside a hull, you can reach further than you can from a
  // jetty - and needing to line yourself up to within 190 units while being
  // pushed about by your own swimming is a miserable way to steal a boat.
  int32_t reach = g_p.swim ? 300 : 190;
  if (idx < 0) { idx = ent_nearest(ENT_BOAT, reach); cop_car = false; }
  if (idx < 0) { idx = ent_nearest(ENT_COPBOAT, reach); cop_car = true; }
  // A helicopter on the ground can be taken like anything else.
  if (idx < 0) {
    idx = ent_nearest(ENT_HELI, 220); cop_car = true;
    if (idx >= 0 && g_ent[idx].z > 0) idx = -1;
  }
  if (idx < 0) { NO_VEHICLE_ACTION(); return; }     // the combined action: nothing to board, so the weapon

  Ent *car = &g_ent[idx];
  if (car->state == ST_DEAD) return;     // no getting into a wreck
  bool was_parked = (car->state == ST_PARK);          // nobody at the wheel
  bool police = (car->type == ENT_COPCAR || car->type == ENT_HELI);
  if (car->type == ENT_HELI) {
    car->state = ST_DRIVE; g_p.heli_wep = 0;
    kit(idx);                              // the aircraft's kit (the boarding tenant's own)
  }
  if (car->type == ENT_COPCAR && car->sub == CAR_TANK) {
    g_p.armor = ARMOR_MIL; g_p.heli_wep = 0;   // military grade: it comes with the tank
    audio_sfx(SFX_PICKUP);
  }
  car->occupied = 1;
  car->door = 64;
  if (was_parked) car->state = ST_DRIVE;              // back in service
  g_p.in_car = (uint8_t)(idx + 1);
  // A boat you have just untied is usually pointing at the jetty it was
  // moored to, and land stops a hull dead - so pushing the stick forward
  // did nothing at all and the boat felt broken. Cast off facing open
  // water: sweep the compass and take the first heading with water ahead.
  if (car->type == ENT_BOAT || car->type == ENT_COPBOAT) {
    int32_t look = 3 * TILE;
    if (cell_at_point(car->x + ((fcos(car->ang) * look) >> 10),
                      car->y + ((fsin(car->ang) * look) >> 10)) != CELL_WATER) {
      for (int t = 1; t <= 16; t++) {
        uint16_t cand = (uint16_t)(car->ang + (uint16_t)(t * 4096));
        if (cell_at_point(car->x + ((fcos(cand) * look) >> 10),
                          car->y + ((fsin(cand) * look) >> 10)) == CELL_WATER) {
          car->ang = cand;
          break;
        }
      }
    }
  }
  g_p.ang = car->ang;
  g_p.spd = 0;
  g_p.stolen++;
  // Jacking a patrol car gets you their kit. It is the only armour you can
  // get without finding a gun shop, and it makes taking a cop car worth the
  // extra heat it costs you.
  if (cop_car && g_p.armor < ARMOR_MAX) g_p.armor = ARMOR_MAX;

  // Haul whoever is in it out onto the road: he lands beside the car,
  // sprawls, and picks himself up. The player's arm swings with it.
  {
    int di = -1;
    for (int q = 0; q < MAX_ENT; q++)
      if (g_ent[q].type == ENT_NONE) { di = q; break; }
    if (di >= 0 && car->state != ST_DEAD && !was_parked) {
      Ent *d2 = &g_ent[di];
      memset(d2, 0, sizeof(Ent));
      // Out of a patrol car or the helicopter comes an officer, armed and
      // straight into the chase; out of anything else, whoever was driving.
      d2->type = police ? ENT_COP : ENT_PED;
      d2->sub = police ? PED_MAN : (uint8_t)wr_rand_n(PED_KIND_COUNT);
      d2->wep = police ? W_PISTOL : W_NONE;
      d2->hp = 100;
      d2->state = police ? ST_CHASE : ST_FLEE;
      d2->ang = (uint16_t)(car->ang + 16384);
      d2->x = car->x - ((fsin(car->ang) * 70) >> 10);
      d2->y = car->y + ((fcos(car->ang) * 70) >> 10);
      d2->z = 16;                      // thrown clear, tumbling
      d2->vx = (int16_t)((-fsin(car->ang) * 7) >> 10);
      d2->vy = (int16_t)(( fcos(car->ang) * 7) >> 10);
      d2->vz = 7;
      d2->spin = (int16_t)(1800 + wr_rand_n(2200));
      d2->timer = 1;
    }
  }
  g_p.anim = 10;                       // the pulling-out swing
  audio_sfx(SFX_DOOR);
  wr_vibe(VIBE_LIGHT);
  audio_sfx(SFX_CAR_START);
  add_heat(cop_car ? HEAT_STEAL * 2 : HEAT_STEAL);
  if (car->type == ENT_CAR && car->sub == CAR_TAXI) game_caption(CAP_TAXI, 70);
  if (car->type == ENT_COPCAR) {
    // There is a gun in the rack: a pistol or a shotgun, and you are
    // holding it.
    uint8_t w = wr_rand_n(2) ? W_SHOTGUN : W_PISTOL;
    g_p.inv[g_p.weapon % W_COUNT] = g_p.ammo;
    g_p.weapon = w;
    g_p.ammo = (uint8_t)(g_p.inv[w] + (w == W_SHOTGUN ? 12 : 24));
    audio_sfx(SFX_PICKUP);
  }

  // The garage trick: under a deck the police cannot see which car you
  // leave in. Take a different one and the heat is gone - after the theft
  // itself has been counted, or it would come straight back.
  {
    int li = lot_at(car->x, car->y);
    if (li >= 0 && g_lot[li].kind == LOT_GARAGE && g_p.wanted > 0 && !cop_car) {
      g_p.wanted = 0; g_p.heat = 0; g_p.evade = EVADE_CLEAR + 1;
      game_caption(CAP_LOST, 60);
      audio_sfx(SFX_ESCAPE);
    }
  }
}

#endif
