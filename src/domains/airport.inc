// The airport, one source, two tenants (see dom_airport.c / dom_airside.c):
// AIRSIDE 0 is the lobby and security; AIRSIDE 1 the terminal and the cabin.
// The kernel swaps them at the security gate (g_in.after == 3).
#ifndef AIRSIDE
#error "airport.inc needs AIRSIDE"
#endif

#if WR_MONOLITH
extern int32_t s_eye_x, s_eye_y;
int free_slot_evict(void);
#endif

// ---------------------------------------------------------------------------
// The airport.
//
// Four rooms in sequence, each replacing the last in the same arena:
//
//   ticketing   - $50 on the counter is a ticket; the door at the back
//                 needs one.
//   security    - a bin to dump your weapons in; the guard at the gate
//                 stops anyone still carrying a gun.
//   terminal    - seats, passengers, a wait; then NOW BOARDING and the
//                 gate opens.
//   cabin       - rows of seats and passengers, the cockpit door at the
//                 front. Sit down and the flight lands you at the other
//                 island's airport. Or punch through to the cockpit,
//                 deal with the pilots, press the action at the controls,
//                 and the room drops away: you are flying the airliner
//                 over the world.
//
// The airliner itself is a helicopter entity with a different sub - the
// air domain flies and draws it - so nothing about flight lives here.
// ---------------------------------------------------------------------------

// Room sizes per stage. The cabin is a fuselage: narrow and long.
static const int16_t STAGE_W[4] = { 4 * TILE, 4 * TILE, 5 * TILE, TILE + TILE / 2 };
static const int16_t STAGE_H[4] = { 4 * TILE, 4 * TILE, 4 * TILE, 6 * TILE };
#define AW (STAGE_W[g_in.stage])
#define AH (STAGE_H[g_in.stage])
#define TICKET_PRICE 50
#define TERMINAL_WAIT 250
#define FLIGHT_WAIT   300

enum { STG_TICKETS = 0, STG_SECURITY, STG_TERMINAL, STG_CABIN };
enum { PA_PASSENGER = PA_CUSTOMER, PA_TSA = PA_BOUNCER, PA_PILOT = PA_CLERK };

typedef struct { uint8_t what, a, b; int16_t x, y; uint16_t ang; } Fit;
#define FP(kind, tier, x, y, ang) { 0, kind, tier, x, y, ang }
#define FM(role, wep, x, y, ang)  { 1, role, wep, x, y, ang }
#define FW(role, wep, x, y, ang)  { 2, role, wep, x, y, ang }
#define FB(x, y)                  { 3, PA_TSA, W_PISTOL, x, y, 49152 }

static const uint8_t WALL[3] = { ARGB(2,2,2), ARGB(1,2,3), ARGB(1,1,1) };

#if !AIRSIDE
static const Fit TICKETS[] = {
  FP(PROP_DOOR, 0, 512, 1024, 0),                                 // in, at the bottom
  FP(PROP_COUNTER, 0, 512, TILE - 10, 0),                          // the desk
  FM(PA_CLERK, W_FIST, 470, 50, 16384),  FW(PA_CLERK, W_FIST, 560, 50, 16384),
  FP(PROP_DOOR, 1, 1024 - 60, 20, 16384),                          // the way through: back right
  FM(PA_PASSENGER, W_FIST, 380, 500, 16384), FW(PA_PASSENGER, W_FIST, 380, 640, 16384),   // the queue
  FM(PA_PASSENGER, W_FIST, 640, 560, 16384),
};
static const Fit SECURITY[] = {
  FP(PROP_DOOR, 0, 512, 1024, 0),
  FP(PROP_CABINET, 0, 90, TILE + 40, 0),                           // the bin for weapons
  FB(512 + 150, 150),                                              // the agent at the arch
  FB(512 - 190, 260),                                              // the agent at the belt
  FP(PROP_DOOR, 1, 1024 - 60, 20, 16384),
  FM(PA_PASSENGER, W_FIST, 512, 560, 16384), FW(PA_PASSENGER, W_FIST, 512, 700, 16384),   // in line
  FM(PA_PASSENGER, W_FIST, 512, 840, 16384),
};
#endif
#if AIRSIDE
static const Fit TERMINAL[] = {
  FP(PROP_DOOR, 0, 640, 1024, 0),
  FM(PA_PASSENGER, W_FIST, 300, 396, 49152), FW(PA_PASSENGER, W_FIST, 480, 396, 49152),
  FM(PA_PASSENGER, W_FIST, 840, 396, 49152), FW(PA_PASSENGER, W_FIST, 380, 576, 49152),
  FM(PA_PASSENGER, W_FIST, 700, 576, 49152), FM(PA_PASSENGER, W_FIST, 960, 756, 49152),
  FW(PA_CLERK, W_FIST, 1100, 120, 16384),                          // the gate agent
  FP(PROP_DOOR, 1, 1180, 20, 0),                                   // the gate, top right, by the window
};
static const Fit CABIN[] = {
  FM(PA_PASSENGER, W_FIST, 120, 500, 49152),  FW(PA_PASSENGER, W_FIST, 264, 500, 49152),
  FM(PA_PASSENGER, W_FIST, 120, 700, 49152),  FW(PA_PASSENGER, W_FIST, 264, 900, 49152),
  FM(PA_PASSENGER, W_FIST, 120, 1100, 49152), FM(PA_PASSENGER, W_FIST, 264, 1300, 49152),
  FP(PROP_COUNTER, 2, 192, 250, 0),                                // the cockpit door: breakable
  FM(PA_PILOT, W_FIST, 150, 110, 49152),  FM(PA_PILOT, W_FIST, 234, 110, 49152),
};
#endif
#if AIRSIDE
static const Fit *FITS[4]  = { 0, 0, TERMINAL, CABIN };
static const uint8_t NFIT[4] = { 0, 0, sizeof TERMINAL / sizeof TERMINAL[0], sizeof CABIN / sizeof CABIN[0] };
#else
static const Fit *FITS[4]  = { TICKETS, SECURITY, 0, 0 };
static const uint8_t NFIT[4] = { sizeof TICKETS / sizeof TICKETS[0], sizeof SECURITY / sizeof SECURITY[0], 0, 0 };
#endif

static void put_ped(uint8_t type, uint8_t sub, uint8_t role, uint8_t wep,
                    int32_t x, int32_t y, uint16_t ang) {
  for (int i = 0; i < MAX_ENT; i++) {
    Ent *e = &g_ent[i];
    if (e->type != ENT_NONE) continue;
    memset(e, 0, sizeof(Ent));
    e->type = type; e->sub = sub; e->aux = role; e->wep = wep;
    e->hp = role == PA_TSA ? 150 : 100;
    e->state = ST_IDLE;
    e->x = x; e->y = y; e->ang = ang;
    e->timer = (uint8_t)wr_rand_n(200);
    return;
  }
}


// --- dressing --------------------------------------------------------------------
// Everything that makes a room the room it is. Tables where a thing is a
// fixed shape, a few loops where it repeats. Drawn after the walls each
// frame (DOP_DRAW_LOT is per wall; the dressing rides on the first wall).
//                         ox    oy  oz  hl  hw   h  ts  tl  tw dang  top       bright    dark
#if !AIRSIDE
static const PartRec POST[] = {   // a queue post with a rope tab; laid out in a loop
  {   0,   0,  0,  5,  5, 42,  0, 64, 64,  0, { ARGB(2,2,2), ARGB(1,1,1), ARGB(0,0,0) } },
  {   0,   0, 42, 10, 10,  4,  0, 64, 64,  0, { ARGB(3,3,0), ARGB(3,3,0), ARGB(2,2,0) } },
};
static const PartRec BOARD[] = {  // departures board on the back wall
  {      0,    0,  55,  60,   2,  30,  0, 64, 64,  0, { ARGB(0,0,0), ARGB(0,0,0), ARGB(0,0,0) } },
  {  -35,   -2,  75,  20,   0,   3,  0, 64, 64,  0, { ARGB(3,3,0), ARGB(3,3,0), ARGB(3,3,0) } },
  {    10,   -2,  75,  20,   0,   3,  0, 64, 64,  0, { ARGB(0,3,0), ARGB(0,3,0), ARGB(0,3,0) } },
  {  -35,   -2,  65,  20,   0,   3,  0, 64, 64,  0, { ARGB(3,3,3), ARGB(3,3,3), ARGB(3,3,3) } },
  {    10,   -2,  65,  20,   0,   3,  0, 64, 64,  0, { ARGB(3,0,0), ARGB(3,0,0), ARGB(3,0,0) } },
};
static const PartRec SCANNER[] = { // the walk-through arch, and the x-ray belt to its left
  {  -30,    0,   0,   4,  10,  60, 0, 64, 64,  0, { ARGB(2,2,2), ARGB(2,2,2), ARGB(1,1,1) } },
  {    30,    0,   0,   4,  10,  60, 0, 64, 64,  0, { ARGB(2,2,2), ARGB(2,2,2), ARGB(1,1,1) } },
  {      0,    0,  60,  35,  10,   7,  0, 64, 64,  0, { ARGB(2,2,2), ARGB(2,2,2), ARGB(1,1,1) } },
  {      0,   -7,  50,   4,   1,   4,  0, 64, 64,  0, { ARGB(0,3,0), ARGB(0,3,0), ARGB(0,3,0) } },   // the green light
  { -100,    0,   0,  55,  11,  17,  0, 64, 64,  0, { ARGB(1,1,1), ARGB(1,1,1), ARGB(0,0,0) } },   // belt
  { -100,    0,  17,  55,   9,   1,  0, 64, 64,  0, { ARGB(0,0,0), ARGB(0,0,0), ARGB(0,0,0) } },   // the belt's rubber
  {  -80,    0,  18,  20,  15,  30,  0, 64, 64,  0, { ARGB(2,2,3), ARGB(2,2,3), ARGB(1,1,2) } },   // the x-ray tunnel
  { -120,    0,  18,   7,   6,   3,  0, 64, 64,  0, { ARGB(2,2,2), ARGB(2,2,2), ARGB(1,1,1) } },   // a tray on the belt
};
#endif
#if AIRSIDE
static const PartRec SEATROW[] = { // a row of four joined seats: bench, back, two armrests' feet
  {   0,   0, 16, 110, 16,  8,  0, 64, 64,  0, { ARGB(0,0,2), ARGB(0,0,2), ARGB(0,0,1) } },
  {   0,  12, 24, 110,  4, 26,  0, 64, 64,  0, { ARGB(0,0,2), ARGB(0,0,2), ARGB(0,0,1) } },
  { -80,   0,  0,   3, 12, 16,  0, 64, 64,  0, { ARGB(1,1,1), ARGB(1,1,1), ARGB(0,0,0) } },
  {  80,   0,  0,   3, 12, 16,  0, 64, 64,  0, { ARGB(1,1,1), ARGB(1,1,1), ARGB(0,0,0) } },
};
static const PartRec PLANE_OUT[] = { // the airliner at the gate, seen through the window
  {      0,    0,   5,  65,   6,   7,  0, 64, 64,  0, { ARGB(3,3,3), ARGB(2,2,2), ARGB(1,1,1) } },
  {   -5,    0,   6,   7,  45,   1,  0, 64, 64,  0, { ARGB(3,3,3), ARGB(2,2,2), ARGB(1,1,1) } },
  {  -55,    0,   8,   5,   1,  17,  0, 64, 64,  0, { ARGB(3,0,0), ARGB(2,0,0), ARGB(1,0,0) } },
  {  -55,    0,   7,   5,  17,   1,  0, 64, 64,  0, { ARGB(3,0,0), ARGB(2,0,0), ARGB(1,0,0) } },
};
static const PartRec CONSOLE[] = { // the cockpit: a sloped console with instruments, two seats
  {      0,    0,   0,  80,  15,  20,  0, 64, 64,  0, { ARGB(1,1,1), ARGB(1,1,1), ARGB(0,0,0) } },
  {      0,   -3,  20,  80,   9,  13,  0, 40, 64,  0, { ARGB(0,0,0), ARGB(1,1,1), ARGB(0,0,0) } },
  {  -25,   -4,  33,   8,   1,   4,  0, 64, 64,  0, { ARGB(0,3,0), ARGB(0,3,0), ARGB(0,2,0) } },   // instruments
  {    25,   -4,  33,   8,   1,   4,  0, 64, 64,  0, { ARGB(0,3,0), ARGB(0,3,0), ARGB(0,2,0) } },
  {      0,   -4,  33,  11,   1,   4,  0, 64, 64,  0, { ARGB(3,1,0), ARGB(3,1,0), ARGB(2,0,0) } },
  {  -30,   25,   0,   8,   8,  15,  0, 64, 64,  0, { ARGB(0,0,1), ARGB(0,0,1), ARGB(0,0,0) } },   // seats
  {    30,   25,   0,   8,   8,  15,  0, 64, 64,  0, { ARGB(0,0,1), ARGB(0,0,1), ARGB(0,0,0) } },
};
static const PartRec CABINROW[] = { // a pair of seats each side of the aisle, with headrests
  { -100,  0, 16, 24, 30, 10,  0, 64, 64,  0, { ARGB(0,0,2), ARGB(0,0,2), ARGB(0,0,1) } },
  { -100, 20, 26, 24,  5, 30,  0, 64, 64,  0, { ARGB(0,0,2), ARGB(0,0,2), ARGB(0,0,1) } },
  {  100,  0, 16, 24, 30, 10,  0, 64, 64,  0, { ARGB(0,0,2), ARGB(0,0,2), ARGB(0,0,1) } },
  {  100, 20, 26, 24,  5, 30,  0, 64, 64,  0, { ARGB(0,0,2), ARGB(0,0,2), ARGB(0,0,1) } },
};

#endif

static void dress(void) {
  int32_t x0 = ROOM_OX, y0 = ROOM_OY, w = g_in.w, h = g_in.h;
  switch (g_in.stage) {
#if !AIRSIDE
    case STG_TICKETS:
      for (int i = 0; i < 6; i++) gfx_parts(x0 + 300 + (i & 1) * 160, y0 + 320 + (i >> 1) * 220, 0, 0, 0, POST, 2, 1, NULL);
      gfx_parts(x0 + w / 2, y0 + 30, 0, 0, 0, BOARD, 5, 2, NULL);
      break;
    case STG_SECURITY:
      gfx_parts(x0 + w / 2 + 40, y0 + 150, 16384, 0, 0, SCANNER, 8, 2, NULL);
      for (int i = 0; i < 4; i++) { gfx_parts(x0 + w / 2 - 110, y0 + 420 + i * 150, 0, 0, 0, POST, 2, 1, NULL);
                                    gfx_parts(x0 + w / 2 + 110, y0 + 420 + i * 150, 0, 0, 0, POST, 2, 1, NULL); }
      break;
#else
    case STG_TERMINAL: {
      for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++)
        gfx_parts(x0 + 300 + c * 300, y0 + 400 + r * 180, 0, 0, 0, SEATROW, 4, 1, NULL);
      // The window wall at the back: sky, the apron, and the plane at the gate.
      const int32_t sky[4][3] = { { x0 - 40, y0 - 40, 260 }, { x0 + w + 40, y0 - 40, 260 }, { x0 + w + 40, y0 - 40, 70 }, { x0 - 40, y0 - 40, 70 } };
      gfx_world_poly(sky, 4, ARGB(1,2,3));
      const int32_t apron[4][3] = { { x0 - 40, y0 - 40, 70 }, { x0 + w + 40, y0 - 40, 70 }, { x0 + w + 40, y0 - 40, 0 }, { x0 - 40, y0 - 40, 0 } };
      gfx_world_poly(apron, 4, ARGB(1,1,1));
      gfx_parts(x0 + w / 2 + 100, y0 - 160, 3000, 0, 0, PLANE_OUT, 4, 4, NULL);
      for (int32_t o = 120; o < w - 100; o += 260)               // the mullions
        gfx_part(x0 + o, y0 - 20, 0, 0, 0, 0, 5, 5, 260, 0, ARGB(1,1,1), ARGB(1,1,1), ARGB(0,0,0));
      break;
    }
    case STG_CABIN: {
      for (int r = 0; r < 6; r++) gfx_parts(x0 + w / 2, y0 + 480 + r * 200, 0, 0, 0, CABINROW, 4, 1, NULL);
      // overhead bins along both sides
      gfx_part(x0 + 30, y0 + h / 2 + 100, 0, 0, 0, 150, 22, h / 2 - 150, 40, 0, ARGB(2,2,2), ARGB(2,2,2), ARGB(1,1,1));
      gfx_part(x0 + w - 30, y0 + h / 2 + 100, 0, 0, 0, 150, 22, h / 2 - 150, 40, 0, ARGB(2,2,2), ARGB(2,2,2), ARGB(1,1,1));
      // a line of small windows each side, sky through them
      for (int32_t o = 380; o < h - 60; o += 130) {
        gfx_part(x0 + 4, y0 + o, 0, 0, 0, 80, 4, 24, 30, 0, ARGB(2,3,3), ARGB(2,3,3), ARGB(1,2,3));
        gfx_part(x0 + w - 4, y0 + o, 0, 0, 0, 80, 4, 24, 30, 0, ARGB(2,3,3), ARGB(2,3,3), ARGB(1,2,3));
      }
      gfx_parts(x0 + w / 2, y0 + 40, 0, 0, 0, CONSOLE, 7, 2, NULL);
      // outside: clouds, on the sky the floor has become (see DOP_ENTER)
      for (int i = 0; i < 6; i++) {
        int32_t cx = x0 + ((i & 1) ? w + 300 : -300) + ((i * 97) & 127), cy = y0 + 200 + i * 220 + ((g_frame / 2 + i * 40) & 255);
        gfx_ground_quad(cx, cy, 90, ARGB(3,3,3));
      }
      break;
    }
#endif
    default: break;
  }
}

// Build the current stage's room. Called on entry and again at each door.
static void build(void) {
  const Fit *f = FITS[g_in.stage]; int n = NFIT[g_in.stage];
  g_in.w = AW; g_in.h = AH;
  g_lot_col = g_in.stage == STG_CABIN ? ARGB(1,2,3) : ARGB(2,2,2);   // sky outside the fuselage; grey floors elsewhere
  int32_t x0 = ROOM_OX, y0 = ROOM_OY, x1 = x0 + AW, y1 = y0 + AH;
  const int32_t T = 40;
  const int32_t wall[4][4] = {
    { x0 - T, y0 - T, x1 + T, y0 }, { x0 - T, y0, x0, y1 }, { x1, y0, x1 + T, y1 }, { x0 - T, y1, x1 + T, y1 + T },
  };
  g_nlot = 0;
  for (int i = 0; i < 4; i++) {
    Lot *L = &g_lot[g_nlot++];
    L->x0 = wall[i][0]; L->y0 = wall[i][1]; L->x1 = wall[i][2]; L->y1 = wall[i][3];
    L->h = 3 * TILE; L->mat = 2; L->kind = LOT_SOLID; L->glass = 0;
  }
  // Everyone from the last room goes; the fittings too.
  for (int i = 0; i < MAX_ENT; i++) if (g_ent[i].type == ENT_PED || g_ent[i].type == ENT_COP) g_ent[i].type = ENT_NONE;
  memset(g_prop, 0, sizeof(Prop) * MAX_PROP);
  int np = 0;
  for (int i = 0; i < n; i++, f++) {
    int32_t x = x0 + f->x, y = y0 + f->y;
    if (f->what == 0) {
      Prop *p = &g_prop[np++];
      p->x = x; p->y = y; p->kind = f->a; p->ang = f->ang;
      p->hp = (f->a == PROP_COUNTER && f->b == 2) ? 60 : 255;   // the cockpit door breaks; nothing else does
      p->alive = 1; p->spin = f->b;
    } else {
      put_ped(f->what == 3 ? ENT_COP : ENT_PED, f->what == 2 ? PED_WOMAN : PED_MAN, f->a, f->b, x, y, f->ang);
    }
  }
  g_in.wait = 0;
  // Each room is entered from its front door, except the cabin, which you
  // board from the gate at the top: the seats face forward, so do you.
  g_p.x = x0 + AW / 2;
  g_p.y = g_in.stage == STG_CABIN ? y0 + AH - 60 : y0 + AH - TILE / 2;
  g_p.ang = 49152;
  g_p.spd = 0; g_p.strafe = 0;
}

static bool near_prop(uint8_t kind, uint8_t spin, int32_t range, Prop **out) {
  for (int i = 0; i < MAX_PROP; i++) {
    Prop *p = &g_prop[i];
    if (!p->alive || p->loose || p->kind != kind || p->spin != spin) continue;   // loose = knocked down
    if (wr_dist(p->x - g_p.x, p->y - g_p.y) < range) { if (out) *out = p; return true; }
  }
  return false;
}

static bool carrying_a_gun(void) {
  for (int w = W_PISTOL; w < W_CASH; w++) if (g_p.inv[w]) return true;
  return g_p.weapon >= W_PISTOL && g_p.weapon < W_CASH;
}

// The plane. Asked for here; made by the kernel once the room is gone (a
// domain must not unload itself, and the room's entities are cleared on
// the way out - the plane has to be spawned after that).
static void take_the_plane(void) {
  g_in.after = 2;
  inside_leave();                         // a request: honoured after this call returns
}

// The action button, indoors. Returns 1 if the airport used it.
static int action(void) {
  Prop *p;
  switch (g_in.stage) {
#if !AIRSIDE
    case STG_TICKETS:
      if (near_prop(PROP_DOOR, 1, 120, &p)) {
        if (!g_in.ticket) { game_caption(CAP_TICKET, 60); return 1; }
        g_in.stage = STG_SECURITY; build(); return 1;
      }
      return 0;
    case STG_SECURITY:
      if (near_prop(PROP_CABINET, 0, 110, &p)) {
        // The bin. Everything with a trigger goes in it.
        for (int w = W_PISTOL; w < W_CASH; w++) g_p.inv[w] = 0;
        if (g_p.weapon >= W_PISTOL && g_p.weapon < W_CASH) { g_p.weapon = W_FIST; g_p.ammo = 0; }
        audio_sfx(SFX_SMASH);
        return 1;
      }
      if (near_prop(PROP_DOOR, 1, 120, &p)) {
        if (carrying_a_gun()) { game_caption(CAP_NOGUNS, 60); return 1; }
        // Through to airside: the other tenant. Asked for here, done by the
        // kernel once this call has returned.
        g_in.stage = STG_TERMINAL; g_in.after = 3; return 1;
      }
      return 0;
#else
    case STG_TERMINAL:
      if (near_prop(PROP_DOOR, 1, 120, &p)) {
        if (g_in.wait < TERMINAL_WAIT) return 1;          // not yet
        g_in.stage = STG_CABIN; build(); game_caption(CAP_SEATED, 70); return 1;
      }
      return 0;
    case STG_CABIN:
      // The cockpit door, still up: hit it (the tap is the weapon, below).
      // Checked before the seats, because the front row is in reach too.
      if (near_prop(PROP_COUNTER, 2, 110, &p)) return 0;
      // A seat: the rows are dressing, so a tap anywhere along them takes
      // the nearest one, and the flight happens to you.
      if (!g_in.pinned && g_p.y >= ROOM_OY + 400) {
        int32_t row = (g_p.y - ROOM_OY - 480 + 100) / 200; if (row < 0) row = 0; if (row > 5) row = 5;
        int32_t side = g_p.x < ROOM_OX + g_in.w / 2 ? -100 : 100;
        g_in.pinned = 1; g_in.wait = 0; g_p.x = ROOM_OX + g_in.w / 2 + side; g_p.y = ROOM_OY + 480 + row * 200 - 10;
        return 1;
      }
      // Through it: the controls are at the front wall.
      if (g_p.y < ROOM_OY + 60) {
        bool pilots = false;
        for (int i = 0; i < MAX_ENT; i++)
          if (g_ent[i].type == ENT_PED && (uint8_t)g_ent[i].aux == PA_PILOT && g_ent[i].state != ST_DEAD) pilots = true;
        if (pilots) return 0;                              // they are still in the seats
        take_the_plane(); return 1;
      }
      return 0;
#endif
  }
  return 0;
}

static void update(void) {
  if (g_in.wait < 65000) g_in.wait++;
  if (g_in.stage == STG_TERMINAL && g_in.wait == TERMINAL_WAIT) { game_caption(CAP_BOARDING, 80); audio_sfx(SFX_PICKUP); }
  if (g_in.stage == STG_CABIN && g_in.pinned) {
    // Seated. The flight is a wait, and then you are somewhere else.
    if (g_in.wait >= FLIGHT_WAIT) {
      g_in.pinned = 0;
      int here = island_at(g_in.door_x, g_in.door_y);
      int there = here == ISL_GHETTO ? ISL_COAST : ISL_GHETTO;
      // Land at the other island: find its airport door. Cheap version -
      // the island's centre road; the terminal is nearby.
      // The room is left by the kernel after this call returns (a domain
      // must not unload itself); it then puts you on the other island's
      // nearest road, since the door position is just its centre.
      g_in.door_x = ISLANDS[there].cx * TILE; g_in.door_y = ISLANDS[there].cy * TILE;
      g_in.after = 1;
      inside_leave();
      game_caption(CAP_LANDED, 70);
    }
  }
}

static bool person(Ent *e) {
  if (e->state == ST_DEAD) return false;
  uint8_t role = (uint8_t)e->aux;
  if (e->type == ENT_COP) return false;                  // the guard is the police's
  if (g_in.hostile || e->hp < 100) {
    // Panic: for the door.
    e->ang = angle_to(e->x, e->y, ROOM_OX + g_in.w / 2, ROOM_OY + g_in.h - 20);
    e->x += fmul(6, fcos(e->ang)); e->y += fmul(6, fsin(e->ang));
    return true;
  }
  e->ang = role == PA_PILOT ? 49152 : e->ang;
  return true;
}

static void money(void) {
  if (g_in.stage != STG_TICKETS) return;
  g_in.credit += 10;
  if (g_in.credit >= TICKET_PRICE && !g_in.ticket) { g_in.ticket = 1; audio_sfx(SFX_PICKUP); game_caption(CAP_GATE, 110); }
}

static void violence(bool killed) {
  (void)killed;
  g_in.hostile = 1;
  // In the cabin a dead pilot is the plan; anywhere else it is the alarm.
  if (g_in.stage != STG_CABIN) g_in.alarm = 1;
}

#if WR_MONOLITH
void draw_room_lot(int i, const uint8_t *pal);
void money_throw_in(void);
#else
static void draw_room_lot(int i, const uint8_t *pal) {
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
// A bill toward the counter, as in the store.
static void money_throw_in(void) {
  int32_t vx = fmul(30, fcos(g_p.ang)), vy = fmul(30, fsin(g_p.ang));
  money_burst(g_p.x + vx, g_p.y + vy, 1);
  g_p.cash = (uint16_t)(g_p.cash > 10 ? g_p.cash - 10 : 0);
  money();
}
#endif

#if !AIRSIDE
// The HUD's PAID line: what is on the counter so far.
static void draw_hud_in(GContext *ctx) {
  char line[16]; int n = 0;
  line[n++] = 'P'; line[n++] = 'A'; line[n++] = 'I'; line[n++] = 'D'; line[n++] = ' '; line[n++] = '$';
  n += gfx_utoa(line + n, 6, g_in.credit); line[n] = 0;
  hud_text(ctx, line, FONT_KEY_GOTHIC_14_BOLD, 64, 32, 72, 16, GColorYellow);
}
#endif

static int dom_dispatch(int op, int arg) {
  switch (op) {
    case DOP_ENTER:    build(); if (g_in.stage == STG_TICKETS) game_caption(CAP_TICKET, 110); return 1;   // the price, up front
    case DOP_UPDATE:   update(); return 1;
    case DOP_PERSON:   return person(&g_ent[arg]) ? 1 : 0;
    case DOP_MONEY:    money(); return 1;
    case DOP_THROW:    money_throw_in(); return 1;
    case DOP_ACTION:   return action();
    case DOP_VIOLENCE: violence(arg != 0); return 1;
    case DOP_DRAW_LOT: draw_room_lot(arg, WALL); if (arg == 0) dress(); return 1;
#if !AIRSIDE
    case DOP_DRAW_HUD: if (g_in.stage == STG_TICKETS) draw_hud_in((GContext *)(intptr_t)arg); return 1;
#endif
    default: return 0;
  }
}

#if AIRSIDE
DOMAIN_ENTRY(dom_airside_entry)
#else
DOMAIN_ENTRY(dom_airport_entry)
#endif
