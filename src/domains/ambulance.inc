// ---------------------------------------------------------------------------
// The ambulance. Compiled into the THREAT tenants that are resident at zero
// to three stars (the taxi at zero, the arrest at one to three), so an
// ambulance turns out whenever a body settles somewhere and the streets are
// not yet a war zone; from four stars there is no tenant for it, and the
// kernel sends any ambulance on the street home. It drives to the nearest
// settled body, holds over it with the lights going, and the body is gone
// when it pulls away. You can take it like any car. Restored from the
// tree as it was before the money system (commit 9de498e), unchanged in
// behaviour, with its drawing here too.
// ---------------------------------------------------------------------------
static int find_corpse(int32_t ax, int32_t ay) {
  int best = -1; int32_t bd = 0x7FFFFFFF;
  for (int i = 0; i < MAX_ENT; i++) {
    Ent *e = &g_ent[i];
    if (e->type != ENT_PED && e->type != ENT_COP) continue;
    if (e->state != ST_DEAD || e->z || e->vx || e->vy) continue;
    int32_t d = wr_dist(e->x - ax, e->y - ay);
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}

// The ambulance. It ignores the lights - it is allowed to - drives to the
// nearest settled body, sits over it for a moment with the lights going,
// and the body is gone when it pulls away. `wep` holds its target slot + 1,
// which is a borrowed field, but the alternative was growing every entity
// by two bytes for one vehicle.
static void update_amb(Ent *e) {
  if (e->state == ST_DEAD) return;

  int tgt = (int)e->aux - 1;
  if (tgt < 0 || g_ent[tgt].state != ST_DEAD ||
      (g_ent[tgt].type != ENT_PED && g_ent[tgt].type != ENT_COP)) {
    // No call, or the body has gone: look for another every so often,
    // and drive off in the meantime.
    if (e->state == ST_FLEE && (g_frame & 31)) { drive_along(e, false, 0, 0, LANE, 14, true); return; }
    tgt = find_corpse(e->x, e->y);
    e->aux = (int16_t)(tgt + 1);
    if (tgt < 0) { e->state = ST_FLEE; drive_along(e, false, 0, 0, LANE, 14, true); return; }
    e->state = ST_DRIVE; e->timer = 0;
  }

  Ent *t = &g_ent[tgt];
  int32_t d = wr_dist(e->x - t->x, e->y - t->y);
  // The crew walks the last stretch: a van cannot drive onto a pavement or
  // into a park, which is where people mostly die, and a body deep in a
  // block can be three tiles from the nearest kerb.
  if (d > 3 * TILE) {
    // On the way. `timer` counts the trip; ten seconds without arriving
    // and the crew fetches the body on foot if it is in sight, or gives up.
    drive_along(e, true, t->x, t->y, LANE, 17, true);
    if (e->type == ENT_NONE) return;
    if (e->timer >= 250) {
      if (d < 3 * TILE) { blood_add(t->x, t->y, 20); t->type = ENT_NONE; audio_sfx(SFX_DOOR); }
      e->aux = 0; e->state = ST_FLEE;
    }
  } else {
    // On scene: hold with the lights going, then the body is recovered.
    // A charred body leaves ash where it lay, not a pool.
    e->spd = 0;
    if (e->timer < 200) e->timer = 200;
    if (++e->timer == 255) {
      if (t->fire) gib_burst_debris(t->x, t->y, 8, 0, 0, 3, ARGB(1,1,1));
      else blood_add(t->x, t->y, 20);
      t->type = ENT_NONE;
      e->aux = 0;
      audio_sfx(SFX_DOOR);
    }
  }
}

// Someone phones it in eventually: if a body has settled somewhere and no
// ambulance is already out, one turns up.
static void amb_dispatch(void) {
  if ((g_frame & 255) != 0) return;
  for (int i = 0; i < MAX_ENT; i++) if (g_ent[i].type == ENT_AMB) return;
  if (find_corpse(g_p.x, g_p.y) >= 0) ent_spawn(ENT_AMB);
}

// A white van with a red stripe down each flank, a cross on the roof and a
// strobing light.
static void draw_amb(const Ent *e) {
  CarDraw c = { e->x, e->y, e->ang, CAR_VAN, 0, e->phase, e->door, (uint8_t)(e->state == ST_DEAD), e->hp, 1, e->fire, e->z };
  model_car(&c, 0);
  if (e->state == ST_DEAD) return;
  gfx_part(e->x, e->y, e->ang, 0, 0, 44, 60, 33, 8, 0, ARGB(3,0,0), ARGB(3,0,0), ARGB(2,0,0));
  gfx_part(e->x, e->y, e->ang, -6, 0, 78, 16, 5, 5, 0, ARGB(3,0,0), ARGB(3,0,0), ARGB(3,0,0));
  gfx_part(e->x, e->y, e->ang, -6, 0, 78, 5, 16, 5, 0, ARGB(3,0,0), ARGB(3,0,0), ARGB(3,0,0));
  uint8_t la = (g_frame & 8) ? ARGB(3,0,0) : ARGB(3,3,3);
  gfx_part(e->x, e->y, e->ang, 40, 0, 84, 6, 10, 8, 0, la, la, la);
}
