#include "wr.h"
#include "wr_gfx.h"
#include "wr_domain.h"


// ---------------------------------------------------------------------------
// Every model here is a stack of boxes placed with gfx_part(). Nothing is
// stored as geometry - the rigs are assembled from constants each frame, which
// costs a handful of shifts and keeps the app budget free.
//
// Detail is spent where you are looking. A full pedestrian is seven boxes; at
// distance he collapses to three, then one. The player is always full detail.
// ---------------------------------------------------------------------------

// --- character palettes: torso, torso shadow, skin, trousers ---------------
static const uint8_t PEDC[PED_KIND_COUNT][4] = {
  { ARGB(1,1,3), ARGB(0,0,2), ARGB(3,2,1), ARGB(1,1,1) },  // man
  { ARGB(3,1,2), ARGB(2,0,1), ARGB(3,2,1), ARGB(2,0,2) },  // woman
};
static const uint8_t COPC_PED[4]  = { ARGB(0,0,3), ARGB(0,0,1), ARGB(3,2,1), ARGB(0,0,2) };
static const uint8_t SWATC[4]     = { ARGB(0,1,0), ARGB(0,0,0), ARGB(1,1,1), ARGB(0,0,0) };
static const uint8_t GUARDC[4]    = { ARGB(1,2,0), ARGB(0,1,0), ARGB(2,1,0), ARGB(1,1,0) };   // olive, camo, helmeted
static const uint8_t PLAYC[4]    = { ARGB(3,3,3), ARGB(2,2,2), ARGB(3,2,1), ARGB(0,0,1) };
#define C_HAIR ARGB(1,1,0)
#define C_DEADGRAY ARGB(1,0,0)

// --- weapon palettes: body, shadow -----------------------------------------
static const uint8_t WEPBODY[W_COUNT][2] = {
  { ARGB(0,0,0), ARGB(0,0,0) },  // fist (never drawn)
  { ARGB(2,1,0), ARGB(1,1,0) },  // bat
  { ARGB(1,1,1), ARGB(0,0,0) },  // pistol
  { ARGB(2,1,0), ARGB(1,0,0) },  // shotgun
  { ARGB(0,1,0), ARGB(0,0,0) },  // machine gun
  { ARGB(3,1,0), ARGB(2,0,0) },  // flamethrower
  { ARGB(0,2,0), ARGB(0,1,0) },  // rocket launcher
  { ARGB(1,1,0), ARGB(0,1,0) },  // grenade
  { ARGB(0,3,0), ARGB(0,2,0) },  // cash
};
// half-length, half-width, height, forward reach from the hand
static const int8_t WEPDIM[W_COUNT][4] = {
  {  0, 0, 0,  0 },
  { 26, 4, 6, 20 },   // bat
  { 12, 4, 7, 10 },   // pistol
  { 28, 4, 7, 22 },   // shotgun
  { 30, 5, 9, 24 },   // machine gun
  { 26, 7, 11, 20 },  // flamethrower
  { 34, 7, 12, 26 },  // rocket launcher
  {  7, 7, 7, 0 },   // grenade: a ball in the fist
  { 10, 6, 4, 0 },   // cash: a wad of it
};

// --- car palettes: roof, bright flank, dark flank ---------------------------
static const uint8_t CARC[CAR_MODEL_COUNT][3] = {
  { ARGB(3,0,0), ARGB(2,0,0), ARGB(1,0,0) },  // sedan  - red
  { ARGB(3,2,0), ARGB(2,1,0), ARGB(1,1,0) },  // taxi   - yellow
  { ARGB(3,3,3), ARGB(2,2,2), ARGB(1,1,1) },  // van    - white
  { ARGB(0,1,3), ARGB(0,1,2), ARGB(0,0,1) },  // sports - blue
  { ARGB(0,2,1), ARGB(0,1,1), ARGB(0,1,0) },  // truck  - green
  { ARGB(1,2,0), ARGB(0,1,0), ARGB(0,1,0) },  // jeep   - army green
  { ARGB(1,1,1), ARGB(0,0,0), ARGB(0,0,0) },  // bike   - black
  { ARGB(3,1,0), ARGB(2,1,0), ARGB(1,0,0) },  // school bus - orange (yellow is the taxi's)
  { ARGB(2,3,3), ARGB(1,2,2), ARGB(0,1,1) },  // cybertruck - brushed steel with a blue cast
  { ARGB(1,2,0), ARGB(1,1,0), ARGB(0,1,0) },  // tank - olive drab
};
static const uint8_t COPC_CAR[3]  = { ARGB(0,0,2), ARGB(0,0,1), ARGB(0,0,0) };
static const uint8_t WRECKC[3]    = { ARGB(1,1,1), ARGB(0,0,0), ARGB(0,0,0) };
#define C_GLASS ARGB(0,1,1)
#define C_TYRE ARGB(0,0,0)
#define C_HUB ARGB(1,1,1)

// body hl, body hw, body h, cabin hl, cabin hw, cabin h, cabin offset
static const int16_t CARDIM[CAR_MODEL_COUNT][7] = {
  {  84, 44, 30,  40, 38, 26, -10 },  // sedan
  {  84, 44, 30,  40, 38, 28, -10 },  // taxi
  { 100, 50, 38,  58, 44, 34,  -6 },  // van
  {  88, 42, 24,  34, 36, 20, -14 },  // sports
  { 104, 50, 40,  44, 44, 36,  30 },  // truck
  {  80, 46, 42,  46, 42, 24,  -6 },  // jeep - tall and boxy
  {  56, 18, 20,  22, 16, 22, -10 },  // motorcycle
  { 148, 52, 56,  96, 48, 40,  10 },  // school bus - long and tall
  { 112, 52, 26,  62, 46, 22,  -4 },  // cybertruck - low, long, angular
  { 120, 60, 30,  40, 40, 24,   0 },  // tank - a slab with a turret where the cabin is
};

// ---------------------------------------------------------------------------
// Pedestrians
// ---------------------------------------------------------------------------

#define LEG_H    46
#define TORSO_H  38
#define HEAD_H   18

// The player gets detail nobody else does: he is on screen every frame and
// he is the thing you are actually looking at. Jacket panels over the
// torso, a collar, a belt, shoes on the ends of the legs and hair on the
// head - eleven extra boxes, drawn for exactly one character in the scene.
// The player is built out of more than boxes. He is the only character on
// screen every single frame, so he gets a proper rig: a tapered chest over
// tapered hips, a neck, jaw and brow rather than a cube for a head, arms
// split at the elbow, legs split at the knee, hands and feet. Twenty-odd
// parts for one character, which at LOD_FULL and one instance is affordable
// where it would not be for a crowd.

// Flames on a man who is alight: a body of fire that flickers with the frame
// and a lick above his head. Drawn before the figure so he stands inside it.
static void ped_flames(const PedDraw *p) {
  uint8_t hot  = (g_frame & 4) ? ARGB(3,2,0) : ARGB(3,1,0);
  uint8_t tip  = (g_frame & 4) ? ARGB(3,3,1) : ARGB(3,2,0);
  int32_t wob  = (int32_t)(g_frame & 7);
  gfx_taper(p->x, p->y, p->ang, 0, 0, 2, 16, 14, 62 + wob,
            120, 120, 0, 0, hot, hot, ARGB(2,0,0));
  gfx_taper(p->x, p->y, p->ang, 0, 0, 60 + wob, 10, 9, 26,
            40, 40, 0, (uint16_t)(g_frame << 10), tip, tip, hot);
}

void model_ped(const PedDraw *p, int32_t dist) {
  // Burnt to a shell: one flat charcoal palette over the whole rig.
  static const uint8_t BURNTC[4] = { ARGB(0,0,0), ARGB(1,1,1),
                                     ARGB(0,0,0), ARGB(1,0,0) };
  const uint8_t *pal = p->burnt ? BURNTC
                     : p->is_player ? PLAYC
                     : (p->is_cop && p->role == PA_GUARD) ? GUARDC
                     : (p->is_cop && (p->weapon == W_MG || p->role == PA_BOUNCER)) ? SWATC
                     : (p->is_cop && p->role < PA_CLERK) ? COPC_PED
                     : PEDC[p->kind % PED_KIND_COUNT];
  // A dancer part way through her act: skin where the clothes were. And
  // warpaint: the militia are marked by it - dark brown across the skin,
  // which is a choice a person made, not something they were born as.
  uint8_t bare[4];
  if (p->role == PA_MILITIA) {
    memcpy(bare, pal, 4);
    bare[2] = ARGB(1,0,0);
    pal = bare;
  }
  if (p->undress) {
    memcpy(bare, pal, 4);
    bare[0] = pal[2]; bare[1] = ARGB(2,1,0);
    if (p->undress > 1) bare[3] = pal[2];
    pal = bare;
  }

  if (p->cuffed) {
    // Under arrest: kneeling, torso upright, arms pulled behind the back
    // and a bright pair of cuffs at the wrists. Distinct from the corpse
    // pose on purpose - this is the FREEDOM REVOKED tableau.
    static const PartRec CUFFP[] = {
      BOX(-6, 10, 0, 24, 8, 22, 3, 3, C_DEADGRAY), BOX(-6, -10, 0, 24, 8, 22, 3, 3, C_DEADGRAY),
      BOX(0, 0, 20, 16, 15, 48, 1, 1, 0), BOX(2, 0, 68, 10, 10, 18, 2, 2, 2),
      PART(-18, 9, 34, 16, 6, 8, 256, 256, 0, 3000, 0, 0, C_DEADGRAY),
      PART(-18, -9, 34, 16, 6, 8, 256, 256, 0, -3000, 0, 0, C_DEADGRAY),
      BOX(-26, 0, 32, 6, 10, 6, W3, G2, G1),
    };
    gfx_parts(p->x, p->y, p->ang, 0, 0, CUFFP, 7, 1, pal);
    return;
  }

  if (p->dead) {
    // Limp: a long torso, a lolling head and one flung-out arm. Only pool
    // once he has stopped tumbling and is actually on the ground.
    static const PartRec DEADP[] = {
      BOX(0, 0, 0, 42, 15, 13, 1, 1, C_DEADGRAY), BOX(38, 0, 2, 11, 11, 12, 2, 2, C_DEADGRAY),
      PART(-6, 22, 0, 22, 7, 9, 256, 256, 0, 9000, 0, 0, C_DEADGRAY),
      PART(-30, -14, 0, 20, 7, 9, 256, 256, 0, 52000, 3, 3, C_DEADGRAY),
    };
    if (p->z < 6) gfx_ground_quad(p->x, p->y, 36, ARGB(2,0,0));
    gfx_parts(p->x, p->y, p->ang, p->z, 0, DEADP, 4, 1, pal);
    return;
  }

  bool full = p->is_player || dist < LOD_FULL;
  bool mid  = full || dist < LOD_MID;

  if (!mid) {
    // Distant: one box. Cheap and still reads as a person.
    gfx_part(p->x, p->y, p->ang, 0, 0, p->z, 22, 22, 100, 0, pal[2], pal[0], pal[1]);
    return;
  }

  // Walk cycle. Legs swing fore and aft, arms counter-swing, and the whole
  // body lifts a little at mid-stride. Sampled from the same sine table the
  // renderer uses, so this costs one lookup.
  //
  // One rig for everyone. The player used to get a second pass that drew
  // another pair of arms - out of phase with the first, so one pair always
  // looked as if it was running backwards - and another torso over the
  // first. Now the difference between the player and a walker is a palette
  // and a hat.
  int32_t swing = (fsin((uint16_t)(p->phase << 8)) * 15) >> 10;
  int32_t bob   = (wr_abs(swing) * 3) >> 3;
  int32_t base = p->z;
  const uint8_t shoe = ARGB(0,0,0), skin = pal[2];

  // Legs, wider at the hip than the ankle, with shoes. The player's suit
  // shadow is grey, which is no shade for navy trousers; his legs shade
  // themselves.
  const uint8_t legd = p->is_player ? pal[3] : pal[1];
  for (int sd = -1; sd <= 1; sd += 2) {
    int32_t sw = (sd < 0) ? swing : -swing;
    gfx_taper(p->x, p->y, p->ang, sw, sd * 10, base, 7, 7, LEG_H, 300, 300, 0, 0,
              pal[3], pal[3], legd);
    gfx_part(p->x, p->y, p->ang, sw + 4, sd * 10, base, 11, 7, 5, 0, shoe, shoe, shoe);
  }

  // Torso, shoulders out to a waist; head, jaw to crown. At full detail
  // they are painted between the two arms below.
  int32_t tz = base + LEG_H + bob;
  if (!full) {
    gfx_taper(p->x, p->y, p->ang, 0, 0, tz, 11, 16, TORSO_H, 268, 276, 0, 0,
              pal[0], pal[0], pal[1]);
    gfx_taper(p->x, p->y, p->ang, 2, 0, tz + TORSO_H, 10, 10, HEAD_H, 212, 212, 0, 0,
              p->is_player ? ARGB(3,3,3) : C_HAIR, skin, pal[1]);
    return;
  }

  // Arms hang from the shoulder and narrow to the wrist, with a hand at the
  // end. The right one drives forward during an attack; that punch pose is
  // also what puts a held weapon out in front of the character.
  int32_t reach = p->anim ? (18 + p->anim * 2) : 0;
  int32_t az = tz + 6;
  // Parts inside one rig are painted in order, not sorted, so the arm on
  // the far side has to go down before the torso and the near one after
  // it, or you see both arms through a man standing side-on. Which is
  // which comes from the depth either side of him.
  int32_t lx = p->x - fmul(21, fsin(p->ang)), ly = p->y + fmul(21, fcos(p->ang));
  int32_t rx = p->x + fmul(21, fsin(p->ang)), ry = p->y - fmul(21, fcos(p->ang));
  int first = (gfx_depth_at(lx, ly) > gfx_depth_at(rx, ry)) ? -1 : 1;
  for (int k = 0; k < 2; k++) {
    int sd = k ? -first : first;
    int32_t sw = (sd < 0) ? -swing : swing + reach;
    int32_t dz = (sd > 0 && p->anim) ? -4 : 0;
    gfx_taper(p->x, p->y, p->ang, sw, sd * 21, az + dz, 5, 5, 30, 280, 280, 0, 0,
              pal[0], pal[0], pal[1]);
    gfx_part(p->x, p->y, p->ang, sw + 1, sd * 21, az + dz - 5, 5, 5, 6, 0,
             skin, skin, pal[1]);
    if (k == 0) {
      gfx_taper(p->x, p->y, p->ang, 0, 0, tz, 11, 16, TORSO_H, 268, 276, 0, 0,
                pal[0], pal[0], pal[1]);
      gfx_taper(p->x, p->y, p->ang, 2, 0, tz + TORSO_H, 10, 10, HEAD_H, 212, 212, 0, 0,
                p->is_player ? ARGB(3,3,3) : C_HAIR, skin, pal[1]);
    }
  }

  // Whatever is in the right hand.
  if (p->weapon != W_NONE && p->weapon != W_FIST && p->weapon < W_COUNT) {
    const int8_t *d = WEPDIM[p->weapon];
    const uint8_t *wc = WEPBODY[p->weapon];
    gfx_part(p->x, p->y, p->ang,
             swing + reach + d[3], 21, az + 12,
             d[0], d[1], d[2], 0, wc[0], wc[0], wc[1]);
  }

  if (p->burning) ped_flames(p);

}

// ---------------------------------------------------------------------------
// Cars
// ---------------------------------------------------------------------------

void car_half_dims(const Ent *e, int32_t *hl, int32_t *hw) {
  const int16_t *dim = CARDIM[(e->type == ENT_COPCAR && e->sub != CAR_TANK ? CAR_SEDAN : e->sub) % CAR_MODEL_COUNT];
  *hl = dim[0]; *hw = dim[1];
}

void model_car(const CarDraw *c, int32_t dist) {
  const int16_t *dim = CARDIM[c->model % CAR_MODEL_COUNT];
  static const uint8_t AMBC[3] = { ARGB(3,3,3), ARGB(3,3,3), ARGB(2,2,2) };
  const uint8_t *pal = c->wrecked ? WRECKC
                     : c->is_cop  ? COPC_CAR
                     : (c->is_amb && c->model == CAR_VAN) ? AMBC          // an ambulance: white
                     : CARC[c->model % CAR_MODEL_COUNT];

  int32_t bl = dim[0], bw = dim[1], bh = dim[2];
  // Ride height. The body used to start 8 units off the ground on every
  // model, which is a visible gap once the camera drops to street level.
  // Ride height, plus however far off the ground the wreck has been thrown.
  // Every vertical offset in this model is expressed relative to RIDE, so
  // one term lifts the entire car - body, cabin, wheels and lights together.
  int32_t cl = dim[3], cw = dim[4], ch = dim[5], co = dim[6];
  // Ride height: the body sits on the wheels, not on the road.
  const int32_t RIDE = 3 + c->z + wr_clamp((bh * 2) / 3, 12, 20) / 2;

  bool full = dist < LOD_FULL;
  bool mid  = full || dist < LOD_MID;

  if (!mid) {
    gfx_part(c->x, c->y, c->ang, 0, 0, 0, bl, bw, bh + ch, 0,
             pal[0], pal[1], pal[2]);
    return;
  }

  // Is the driver's side (the car's left) the side facing the camera? Compare
  // the depth of a point off each flank; the nearer one is drawn last.
  int32_t lx = c->x - ((fsin(c->ang) * (bw + 40)) >> 10);
  int32_t ly = c->y + ((fcos(c->ang) * (bw + 40)) >> 10);
  bool door_near = gfx_depth_at(lx, ly) < gfx_depth_at(c->x, c->y);

  // Door swing. It rotates about its front edge and swings clear of the body.
  uint16_t dang = (uint16_t)(-(int32_t)c->door * 180);
  int32_t door_out = c->door >> 2;

  if (c->door && !door_near) {
    gfx_part(c->x, c->y, c->ang, -6, -(bw + 3 + door_out), 10,
             26, 4, 26, dang, pal[1], pal[1], pal[2]);
  }

  // The motorcycle was just a small car - which is exactly why it read as
  // a toy car rather than a bike. It gets its own rig: two big wheels fore
  // and aft in line, a tank and seat between them, forks, bars, and a rider
  // sitting astride it.
  if (c->model == CAR_BIKE && !c->wrecked) {
    static const PartRec BIKEP[] = {
      BOX(0, 0, 18, 40, 12, 18, 0, 1, 2),                                        // tank
      BOX(-26, 0, 34, 20, 13, 8, K0, K0, K0),                                    // seat
      PART(40, 0, 30, 6, 5, 26, 256, 256, 0, 2600, G1, G1, K0),                  // forks
      BOX(34, 0, 50, 4, 26, 4, G1, G1, K0),                                      // bars
      BOX(48, 0, 42, 8, 10, 10, ARGB(3,3,2), ARGB(3,3,1), ARGB(2,2,0)),          // headlight
      BOX(-14, 0, 40, 14, 14, 34, ARGB(0,0,1), ARGB(0,0,1), K0),                 // rider
      BOX(-8, 0, 76, 11, 11, 16, G1, G1, K0),                                    // helmet
      PART(14, 9, 44, 20, 5, 5, 256, 256, 0, 1800, ARGB(0,0,1), ARGB(0,0,1), K0),
      PART(14, -9, 44, 20, 5, 5, 256, 256, 0, -1800, ARGB(0,0,1), ARGB(0,0,1), K0),
    };
    gfx_wheel_disc(c->x, c->y, c->ang,  46, 0, 22, 22, C_TYRE, C_HUB);
    gfx_wheel_disc(c->x, c->y, c->ang, -46, 0, 22, 22, C_TYRE, C_HUB);
    gfx_parts(c->x, c->y, c->ang, 0, 0, BIKEP, c->is_amb ? 5 : 9, 2, pal);   // no rider once he is off
    return;
  }

  // Wheels: a block as long and tall as the wheel is round, so it reads
  // from above, with the octagonal disc on the flank you can see, so it
  // reads from the side. The old 16x7 slab was a wheel from the top and a
  // stick from anywhere else.
  int32_t wr = (bh * 2) / 3;
  if (wr < 12) wr = 12;
  if (wr > 20) wr = 20;
  if (mid) {
    int32_t wx = bl - wr - 8, wy = bw - 4;
    for (int i = 0; i < 4; i++) {
      int32_t ox = (i < 2) ? wx : -wx;
      int32_t oy = (i & 1) ? wy : -wy;
      gfx_part(c->x, c->y, c->ang, ox, oy, c->z, wr, 6, 2 * wr - 2, 0,
               C_TYRE, C_TYRE, C_TYRE);
      if (full)
        gfx_wheel_disc(c->x, c->y, c->ang, ox, oy + ((i & 1) ? 6 : -6), wr + c->z, wr,
                       C_TYRE, C_HUB);
    }
  }

  // Body over the wheels, then a cabin that is glass on every side - which
  // is the single thing that makes a box read as a car rather than a crate.
  gfx_taper(c->x, c->y, c->ang, 0, 0, RIDE, bl, bw, bh,
            244, 232, 0, 0, pal[1], pal[1], pal[2]);
  gfx_taper(c->x, c->y, c->ang, co, 0, RIDE + bh, cl, cw, ch,
            206, 226, -6, 0, pal[0], C_GLASS, ARGB(0,0,1));

  if (full && !c->wrecked) {
    // Windscreen: a dark slab across the front of the cabin roof.
    gfx_part(c->x, c->y, c->ang, co + cl - 8, 0, RIDE + bh + ch - 3,
             8, cw - 4, 4, 0, C_GLASS, C_GLASS, C_GLASS);
    // Headlights.
    gfx_part(c->x, c->y, c->ang, bl - 4, -(bw - 12), RIDE + bh / 2,
             4, 7, 8, 0, ARGB(3,3,2), ARGB(3,3,2), ARGB(2,2,1));
    gfx_part(c->x, c->y, c->ang, bl - 4, bw - 12, RIDE + bh / 2,
             4, 7, 8, 0, ARGB(3,3,2), ARGB(3,3,2), ARGB(2,2,1));
  }

  // Lights, trim, fire and the wreck's hulk, in the body's frame with z at
  // the roofline. Alternating colours come through a two-entry palette.
  int32_t roof = RIDE + bh + ch;
  uint8_t flash[2] = { (g_frame & 8) ? ARGB(3,0,0) : ARGB(0,0,3),
                       (g_frame & 8) ? ARGB(0,0,3) : ARGB(3,0,0) };
  if (!c->wrecked) {
    if (c->is_cop) {
      int32_t lw = (cw - 8) / 2;
      gfx_part(c->x, c->y, c->ang, co, -lw, roof, 10, lw - 1, 12, 0, flash[0], flash[0], flash[0]);
      gfx_part(c->x, c->y, c->ang, co,  lw, roof, 10, lw - 1, 12, 0, flash[1], flash[1], flash[1]);
    }
    static const PartRec CYBERP[] = {
      BOX(6, 0, 35, 82, 48, 16, 0, 1, 2), BOX(-20, 0, 49, 58, 44, 12, 1, 2, 2),
      BOX(54, 0, 43, 4, 50, 5, W3, W3, G2),                        // light bar
    };
    if (c->model == CAR_TAXI)       // the roof light: the only yellow car with one
      gfx_part(c->x, c->y, c->ang, co, 0, roof, 12, 22, 8, 0, W3, W3, K0);
    if (c->model == CAR_CYBER)      gfx_parts(c->x, c->y, c->ang, RIDE, 0, CYBERP, 3, 2, pal);
  }
  if (c->fire) {
    // Flames lick off the roof of a burning car and out of a wreck.
    static const PartRec FIREP[] = {
      PART(-4, 0, 0, 18, 14, 24, 130, 130, 0, 0, 0, 0, 1),
      PART(-20, 6, 10, 10, 8, 16, 120, 120, 0, 0, 1, 1, 0),
    };
    uint8_t fp[2] = { (g_frame & 4) ? ARGB(3,2,0) : ARGB(3,1,0),
                      (g_frame & 4) ? ARGB(3,3,1) : ARGB(3,2,0) };
    gfx_parts(c->x, c->y, c->ang, (c->wrecked ? RIDE + bh : roof) + (int32_t)(g_frame & 7) / 2,
              0, FIREP, 2, 2, fp);
  }
  if (c->wrecked)
    gfx_part(c->x, c->y, c->ang, 0, 0, roof, 18, 18, 26, 0, G1, G1, K0);

  if (c->door && door_near) {
    gfx_part(c->x, c->y, c->ang, -6, -(bw + 3 + door_out), 10,
             26, 4, 26, dang, pal[1], pal[1], pal[2]);
  }
}

// ---------------------------------------------------------------------------
// Pickups and explosions
// ---------------------------------------------------------------------------

void model_pickup(int32_t x, int32_t y, uint8_t weapon, uint32_t frame) {
  if (weapon >= W_COUNT) {
    // Cash lies flat and green; a vest is a blue block.
    bool cash = (weapon == PICKUP_CASH);
    // Cash is a stack tall enough to see, blinking bright.
    uint8_t g1 = (frame & 8) ? ARGB(2,3,2) : ARGB(1,3,1);
    gfx_part(x, y, (uint16_t)(x * 91), 0, 0, cash ? 2 : 0, 14, cash ? 10 : 10, cash ? 8 : 10, 0,
             cash ? g1 : ARGB(1,2,3), cash ? ARGB(0,3,0) : ARGB(0,1,3), cash ? ARGB(0,2,0) : ARGB(0,0,2));
    return;
  }
  const uint8_t *wc = WEPBODY[weapon];
  const int8_t *d = WEPDIM[weapon];
  uint16_t spin = (uint16_t)(frame << 9);
  int32_t bob = ((frame + x) & 16) ? 8 : 0;

  // A glowing plinth so it catches the eye, with the actual weapon turning
  // above it - you can tell a shotgun from a knife before you walk over it.
  gfx_part(x, y, spin, 0, 0, 0, 20, 20, 10 + bob, 0,
           ARGB(3,3,0), ARGB(2,2,0), ARGB(1,1,0));
  gfx_part(x, y, spin, 0, 0, 14 + bob, d[0] ? d[0] : 12, d[1] ? d[1] : 5,
           d[2] ? d[2] : 8, 0, wc[0], wc[0], wc[1]);
}

// --- boats -------------------------------------------------------------------
// hull colour, deck, trim
#if WR_BOATS
static const uint8_t BOATC[BOAT_MODEL_COUNT][3] = {
  { ARGB(3,2,0), ARGB(3,3,3), ARGB(2,1,0) },  // jet ski   - orange
  { ARGB(3,3,3), ARGB(0,1,2), ARGB(2,2,2) },  // motorboat - white
  { ARGB(3,3,3), ARGB(3,3,2), ARGB(2,2,2) },  // yacht     - white/cream
  { ARGB(0,0,2), ARGB(3,3,3), ARGB(0,0,1) },  // police    - blue
};
#endif
// half-length, half-width, freeboard
#if WR_BOATS
static const int16_t BOATD[BOAT_MODEL_COUNT][3] = {
  {  48, 20, 18 },
  {  88, 34, 26 },
  { 150, 48, 38 },
  {  94, 34, 26 },
};
#endif

#if WR_BOATS
// Superstructure per hull, in hull palette indices. The hull itself and
// the wake depend on speed and dimensions and are drawn by hand.
static const PartRec BOATP[] = {
  BOX(-6, 0, 18, 20, 12, 14, 2, 0, 2), BOX(20, 0, 28, 4, 16, 5, K0, K0, K0),           // jet ski
  BOX(-12, 0, 26, 30, 26, 22, 1, 0, 2), BOX(16, 0, 46, 6, 22, 12, ARGB(0,1,1), ARGB(0,1,1), ARGB(0,1,1)),
  BOX(-88, 0, 20, 12, 10, 20, G1, K0, K0),                                             // motorboat
  BOX(-30, 0, 38, 60, 38, 26, 1, 0, 2), BOX(4, 0, 64, 34, 28, 20, 1, 0, 2),
  BOX(10, 0, 84, 6, 6, 90, G2, G1, G1),                                                // yacht
  BOX(-12, 0, 26, 30, 26, 22, 1, 0, 2), BOX(16, 0, 46, 6, 22, 12, ARGB(0,1,1), ARGB(0,1,1), ARGB(0,1,1)),
  BOX(-94, 0, 20, 12, 10, 20, G1, K0, K0),                                             // police
};
static const uint8_t BOAT_OFF[BOAT_MODEL_COUNT + 1] = { 0, 2, 5, 8, 11 };

void model_boat(const Ent *e, int32_t dist) {
  uint8_t k = e->sub % BOAT_MODEL_COUNT;
  const uint8_t *c = BOATC[k];
  const int16_t *d = BOATD[k];
  int32_t hl = d[0], hw = d[1], fb = d[2];

  int32_t v = wr_abs(e->spd);
  if (v > 4) {
    for (int i = 1; i <= 3; i++) {
      int32_t back = hl + i * 34;
      int32_t spread = 8 + i * (v / 3);
      uint8_t wc = (i == 1) ? ARGB(3,3,3) : (i == 2) ? ARGB(2,3,3) : ARGB(1,2,3);
      gfx_part(e->x, e->y, e->ang, -back, 0, 2, 18, spread, 3, 0, wc, wc, wc);
    }
  }

  gfx_taper(e->x, e->y, e->ang, -hl / 3, 0, 0, (hl * 2) / 3, hw, fb,
            256, 330, 0, 0, c[1], c[0], c[2]);
  gfx_taper(e->x, e->y, e->ang, hl - hl / 4, 0, 0, hl / 4, hw / 2, fb,
            150, 300, 14, 0, c[1], c[0], c[2]);

  if (dist > LOD_MID) return;
  gfx_parts(e->x, e->y, e->ang, 0, 0, BOATP + BOAT_OFF[k], BOAT_OFF[k + 1] - BOAT_OFF[k], 2, c);
  if (k == BOAT_POLICE) {
    uint8_t lc = (g_frame & 8) ? ARGB(3,0,0) : ARGB(0,0,3);
    gfx_part(e->x, e->y, e->ang, -12, 0, fb + 22, 10, 18, 10, 0, lc, lc, lc);
  }
}
#endif

// --- helicopter --------------------------------------------------------------
// Body, tail boom, tail fin, skids and a rotor disc that actually turns. The
// rotor is two long thin boxes at right angles rather than a modelled disc -
// at this scale that reads perfectly and costs six quads.
// The police helicopter: white over a blue band with a big glass nose, so
// it reads from a long way off. Skids, fuselage, canopy, boom, fin, pilot
// and rotor hub are static; the beacon and the blades move.

// --- street furniture -------------------------------------------------------
// top, bright flank, dark flank, then half-length, half-width, height
static const uint8_t PROPC[PROP_KIND_COUNT][3] = {
  { ARGB(2,2,2), ARGB(1,1,1), ARGB(0,0,0) },  // lamp post
  { ARGB(3,3,3), ARGB(3,0,0), ARGB(2,0,0) },  // stop sign
  { ARGB(0,0,3), ARGB(0,0,2), ARGB(0,0,1) },  // post box
  { ARGB(3,3,2), ARGB(2,2,1), ARGB(1,1,0) },  // cafe table
  { ARGB(2,1,0), ARGB(1,1,0), ARGB(1,0,0) },  // chair
  { ARGB(3,0,0), ARGB(2,0,0), ARGB(1,0,0) },  // hydrant
  { ARGB(1,1,0), ARGB(1,1,0), ARGB(0,1,0) },  // tree trunk
  { ARGB(2,2,1), ARGB(1,1,0), ARGB(1,1,0) },  // palm trunk
  { ARGB(2,2,2), ARGB(1,1,1), ARGB(1,1,1) },  // boulder
  { ARGB(3,1,1), ARGB(3,3,3), ARGB(2,0,0) },  // parasol
  { ARGB(2,1,0), ARGB(1,1,0), ARGB(1,0,0) },  // shop counter
  { ARGB(1,3,3), ARGB(0,2,2), ARGB(0,1,1) },  // display cabinet
  { ARGB(2,2,2), ARGB(1,1,1), ARGB(0,0,0) },  // bridge pylon
  { ARGB(2,1,0), ARGB(1,1,0), ARGB(1,0,0) },  // dock
  { ARGB(1,1,1), ARGB(0,0,0), ARGB(0,0,0) },  // traffic light
  { ARGB(2,2,2), ARGB(1,1,1), ARGB(1,1,2) },  // pigeon
  { ARGB(3,2,0), ARGB(3,3,3), ARGB(2,1,0) },  // works barrier
  { ARGB(2,1,0), ARGB(1,1,0), ARGB(1,0,0) },  // shop door
  { ARGB(1,1,1), ARGB(2,2,2), ARGB(0,0,0) },  // racked gun
  { ARGB(3,0,1), ARGB(2,0,1), ARGB(1,0,0) },  // stage
  { ARGB(3,3,3), ARGB(2,2,3), ARGB(1,1,2) },  // pole
  { ARGB(3,3,3), ARGB(2,2,3), ARGB(1,1,2) },  // mirror ball
};
// int16_t, not int8_t. A signal mast is 250 units tall and a palm 196, and
// both wrapped to small negative numbers in a signed byte - which is why the
// traffic signals were drawn six units BELOW the pavement and the palms were
// stumps. Nothing warns you: the values are in range for the table's
// declared type, they are just not the values you wrote.
static const int16_t PROPD[PROP_KIND_COUNT][3] = {
  {  6,  6, 110 },   // lamp post
  {  5,  5,  76 },   // sign
  { 14, 12,  50 },   // post box
  { 26, 26,  30 },   // table
  { 13, 13,  36 },   // chair
  { 11, 11,  32 },   // hydrant
  { 11, 11,  96 },   // tree
  { 13, 13, 196 },   // palm - tall now, they read as landmarks
  { 22, 19,  26 },   // boulder
  {  5,  5,  74 },   // parasol
  { 60, 18,  46 },   // counter
  { 30, 16,  50 },   // cabinet
  { 26, 26, 120 },   // pylon
  { 90, 22,  16 },   // dock
  {  8,  8, 250 },   // traffic signal mast - 20 feet, arm over the road
  { 10,  6,   9 },   // pigeon
  { 96, 10,  46 },   // works barrier
  { 62, 10, 118 },   // shop door
  { 34,  6,  96 },   // racked gun
  { 300, 110, 30 },  // stage
  {  5,  5, 220 },   // pole
  { 18, 18,  18 },   // mirror ball
};

// Everything a prop is made of, beyond its base box. One table, indexed by
// kind through PROP_OFF; the base box comes from PROPD/PROPC as before.
static const PartRec PROPP[] = {
  // lamp post: two arm sections, head, lens
  BOX( 46, 0, 106, 34, 5, 5, G1, G1, K0), BOX( 78, 0, 98, 32, 5, 5, G1, G1, K0),
  BOX(108, 0, 84, 22, 11, 8, G1, G2, G1), BOX(108, 0, 77, 18, 9, 4, ARGB(3,3,2), ARGB(3,3,2), ARGB(3,3,1)),
  // stop sign face
  BOX(0, 0, 76, 3, 20, 22, ARGB(3,0,0), W3, ARGB(2,0,0)),
  // cafe table top
  BOX(0, 0, 30, 30, 30, 6, W3, G2, G1),
  // tree canopy, three tiers
  PART(0, 0, 82, 30, 30, 34, 330, 330, 0, 6000, ARGB(1,3,1), ARGB(0,2,0), ARGB(0,1,0)),
  PART(0, 0, 112, 30, 30, 28, 210, 210, 0, 20000, ARGB(1,3,1), ARGB(0,2,0), ARGB(0,1,0)),
  PART(0, 0, 136, 18, 18, 22, 90, 90, 0, 40000, ARGB(2,3,1), ARGB(1,2,0), ARGB(0,1,0)),
  // palm crown (the fronds are drawn in a loop)
  PART(0, 0, 196, 10, 10, 12, 150, 150, 0, 0, ARGB(2,2,0), ARGB(1,1,0), ARGB(1,1,0)),
  // parasol canopy
  PART(0, 0, 74, 40, 40, 10, 256, 256, 0, 8000, ARGB(3,1,1), W3, ARGB(2,0,0)),
  // display cabinet top
  BOX(0, 0, 50, 27, 13, 5, ARGB(2,3,3), ARGB(1,2,2), ARGB(0,1,1)),
  // pylon cap
  BOX(0, 0, 120, 34, 34, 16, G2, G1, K0),
  // dock: deck and two mooring posts
  BOX(0, 0, 16, 90, 22, 5, ARGB(2,2,1), ARGB(1,1,0), ARGB(1,1,0)),
  BOX(80, 16, 16, 6, 6, 26, ARGB(1,1,0), ARGB(1,1,0), ARGB(1,0,0)),
  BOX(80, -16, 16, 6, 6, 26, ARGB(1,1,0), ARGB(1,1,0), ARGB(1,0,0)),
  // traffic signal: arm sections, head housing, top lamp (lenses are live)
  BOX(46, 0, 234, 50, 6, 6, G1, G1, K0), BOX(140, 0, 228, 48, 6, 6, G1, G1, K0),
  BOX(182, 0, 188, 11, 12, 44, G1, K0, K0), BOX(92, 0, 244, 20, 10, 7, ARGB(3,3,2), ARGB(3,3,1), ARGB(2,2,0)),
  // pigeon
  BOX(0, 0, 0, 10, 7, 8, G2, G1, ARGB(1,1,2)), BOX(6, 0, 7, 4, 4, 5, ARGB(1,1,2), ARGB(1,1,2), ARGB(0,0,1)),
  BOX(-8, 0, 3, 6, 5, 2, G1, G1, K0),
  // shop door panel and handle
  BOX(0, -3, 6, 52, 4, 104, ARGB(2,2,1), ARGB(2,2,1), ARGB(1,1,0)),
  BOX(-34, -6, 59, 5, 3, 12, ARGB(3,3,2), ARGB(3,3,2), ARGB(2,2,1)),
  // racked gun: barrel, stock, grip
  BOX(0, 0, 70, 30, 4, 5, G1, G2, K0), BOX(-20, 0, 68, 12, 5, 9, ARGB(2,1,0), ARGB(1,1,0), ARGB(1,0,0)),
  BOX(0, 0, 60, 4, 3, 8, G1, G1, K0),
};
// First entry for each kind, in PROP_* order; the count is the next start.
static const uint8_t PROP_OFF[PROP_KIND_COUNT + 1] = {
  0, 4, 5, 5, 6, 6, 6, 9, 10, 10, 11, 11, 12, 13, 16, 20, 23, 23, 25, 28, 28, 28, 28
};

void model_prop(const Prop *p) {
  if (p->kind >= PROP_DOMAIN0) { domain_call(DOP_DRAW_PROP, (int)(p - g_prop)); return; }
  const uint8_t *c = PROPC[p->kind % PROP_KIND_COUNT];
  const int16_t *d = PROPD[p->kind % PROP_KIND_COUNT];
  uint8_t kind = p->kind % PROP_KIND_COUNT;
  int32_t h = d[2], z = p->z;
  int32_t lean = (int32_t)(uint16_t)p->lean / 800;

  if (p->loose) {
    gfx_part(p->x, p->y, p->ang, 0, 0, z, h / 2, d[1], d[0] * 2, 0, c[1], c[1], c[2]);
    return;
  }
  gfx_part(p->x, p->y, p->ang, lean, 0, z, d[0], d[1], h, 0, c[0], c[1], c[2]);

  // The bird hops; everything else stands still.
  if (kind == PROP_BIRD && ((((int32_t)g_frame + (p->x >> 5)) & 31) < 4)) z += 5;
  gfx_parts(p->x, p->y, p->ang, z, lean, PROPP + PROP_OFF[kind],
            PROP_OFF[kind + 1] - PROP_OFF[kind], 2, c);

  // The live bits: lenses, a blinking lamp, the palm's fronds.
  if (kind == PROP_LIGHT) {
    uint8_t gA = traffic_green(0) ? ARGB(0,3,0) : ARGB(3,0,0);
    uint8_t gB = traffic_green(1) ? ARGB(0,3,0) : ARGB(3,0,0);
    gfx_part(p->x, p->y, p->ang, 182 + lean, 0, z + h - 34, 13, 13, 11, 0, gA, gA, gA);
    gfx_part(p->x, p->y, p->ang, 182 + lean, 0, z + h - 58, 13, 13, 11, 0, gB, gB, gB);
  } else if (kind == PROP_DISCO) {
    // Four facets flashing through the palette as it turns.
    uint8_t f = (uint8_t)(0xC0 | ((g_frame << 2) & 0x3F));
    gfx_part(p->x, p->y, (uint16_t)(g_frame << 10), 0, 0, z + 6, 12, 12, 8, 0, f, f, W3);
  } else if (kind == PROP_PALM) {
    for (int f = 0; f < 6; f++)
      gfx_taper(p->x, p->y, p->ang, 26, 0, z + h + 2, 34, 9, 5,
                40, 40, 26, (uint16_t)(f * 10923 + 4000),
                ARGB(1,3,1), ARGB(0,2,0), ARGB(0,1,0));
  }
}

void model_boom(const Boom *b) {
  int32_t t = BOOM_LIFE - b->life;          // 0 at ignition
  int32_t rad = 26 + t * 7;

  // Scorch first, then a fireball that swells and cools.
  gfx_ground_quad(b->x, b->y, rad, ARGB(1,0,0));

  uint8_t hot, mid, cool;
  if (b->life > 16)      { hot = ARGB(3,3,3); mid = ARGB(3,3,0); cool = ARGB(3,1,0); }
  else if (b->life > 10) { hot = ARGB(3,3,0); mid = ARGB(3,1,0); cool = ARGB(2,0,0); }
  else if (b->life > 5)  { hot = ARGB(3,1,0); mid = ARGB(2,0,0); cool = ARGB(1,0,0); }
  else                   { hot = ARGB(1,1,1); mid = ARGB(1,1,1); cool = ARGB(0,0,0); }

  uint16_t spin = (uint16_t)(t * 2600);
  gfx_part(b->x, b->y, spin, 0, 0, 0,      rad,        rad,        30 + t * 3, 0, mid, mid, cool);
  gfx_part(b->x, b->y, spin, 0, 0, 26 + t * 2, (rad * 3) / 4, (rad * 3) / 4, 26 + t * 3, 8000, hot, mid, cool);
  gfx_part(b->x, b->y, spin, 0, 0, 52 + t * 5, rad / 2,    rad / 2,    22 + t * 2, 16000, hot, hot, mid);
}
