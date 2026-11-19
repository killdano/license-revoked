// A motorcycle in a crash: the rider comes off, hard - thrown, tumbling,
// bleeding, and the bike goes on without him. Compiled into every
// island's scenery tenant; the kernel's crash code finds the collision and
// asks (DOP_BIKE_HIT: bike index, other index, closing speed).
// Air support. It has no road to follow, so it just closes on the player and
// holds station overhead, dropping a SWAT officer on a rope every so often.
// Somewhere flat to put a helicopter down, ahead of the player so the
// landing happens where he is looking. Returns false if the block is all
// walls and water.
static void bike_hit(int bi, int ci, int32_t closing) {
  Ent *bk = &g_ent[bi], *cr = &g_ent[ci];
  int32_t vcx = (fcos(cr->ang) * cr->spd) >> 10, vcy = (fsin(cr->ang) * cr->spd) >> 10;
  if (bk->state == ST_DRIVE && !bk->occupied) {
    int s = free_slot();
    if (s >= 0) {
      Ent *r = &g_ent[s];
      memset(r, 0, sizeof(Ent));
      r->type = ENT_PED; r->sub = PED_MAN; r->state = ST_DEAD;
      r->x = bk->x; r->y = bk->y; r->ang = bk->ang;
      r->vx = (int16_t)(vcx + vcx / 2); r->vy = (int16_t)(vcy + vcy / 2);
      r->vz = 14; r->z = 12; r->spin = (int16_t)(3000 + wr_rand_n(3000));
      gib_burst(r->x, r->y, 40, r->vx, r->vy, 8);
      money_on_death(r);
      g_p.kills++;
      add_heat(60);                            // a kill
      audio_sfx(SFX_RUNOVER);
    }
    bk->state = ST_FLEE;                       // riderless
  }
  bk->ang = cr->ang;
  bk->spd = (int16_t)wr_clamp(closing * 2, 8, 40);
  bk->wep = TURN_NONE;
  crash_damage(bi, (uint8_t)wr_clamp(closing, 15, 30));
  cr->spd = (int16_t)((cr->spd * 7) / 8);
  if (g_p.in_car && (int)g_p.in_car - 1 == ci) { g_p.spd = cr->spd; wr_vibe(VIBE_MED); }
  gib_burst_debris(bk->x, bk->y, 24, vcx / 2, vcy / 2, 6, ARGB(1,1,1));
  audio_sfx(SFX_SMASH);
}
