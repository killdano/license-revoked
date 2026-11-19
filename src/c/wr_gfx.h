#pragma once
#include "wr.h"

// ---------------------------------------------------------------------------
// Shared rasterizer surface.
//
// wr_render.c owns the camera, the span filler and the draw list. wr_model.c
// builds articulated models on top of gfx_part(). Splitting them keeps the
// renderer readable now that a pedestrian is seven boxes instead of one.
// ---------------------------------------------------------------------------

// Pebble's GColor8 is 0b11RRGGBB - two bits per channel, 64 colours. Every
// shade in this game is one of these constants, so shading at draw time is a
// table lookup and never a calculation.
#define ARGB(r, g, b) ((uint8_t)(0xC0 | ((r) << 4) | ((g) << 2) | (b)))

typedef struct { int16_t sx, sy; int32_t depth; uint8_t ok; } Proj;

Proj    gfx_project(int32_t wx, int32_t wy, int32_t wz);
int32_t gfx_depth_at(int32_t wx, int32_t wy);
void    gfx_fill_poly(const Proj *p, int n, uint8_t col);
bool    gfx_all_ok(const Proj *p, int n);
int32_t gfx_area2(const Proj *p, int n);
void    gfx_ground_quad(int32_t cx, int32_t cy, int32_t half, uint8_t col);
// A polygon given in world space, clipped properly against the near plane and
// then against the screen before rasterising. This is the path for anything
// big enough to reach past the edges of the view - ground, streets, building
// walls. gfx_project() clamps stray screen coordinates, which is fine for a
// pedestrian's forearm but bends the edges of a street-sized quad into
// screen-crossing wedges; these never clamp anything.
void    gfx_world_poly(const int32_t (*w)[3], int n, uint8_t col);
int     gfx_clip_world_poly(const int32_t (*w)[3], int n, Proj *out);
// A flat rectangle on the ground at an arbitrary angle. Roads are drawn with
// this so a diagonal street is one continuous band instead of a staircase of
// axis-aligned tiles.
void    gfx_ground_band(int32_t cx, int32_t cy, uint16_t ang,
                        int32_t half_len, int32_t half_wid, uint8_t col);

// The only primitive the model code needs: one box placed in an entity's local
// frame. Limbs, wheels, doors, gun barrels and car bodies are all just boxes
// with different offsets and yaws.
//
//   cx, cy, ang   entity origin and facing
//   ox            forward offset  (+ toward the nose)
//   oy            sideways offset (+ to the entity's right)
//   oz            height of the box's underside
//   hl, hw, h     half-length, half-width, height
//   dang          extra yaw of this part relative to the entity
void gfx_part(int32_t cx, int32_t cy, uint16_t ang,
              int32_t ox, int32_t oy, int32_t oz,
              int32_t hl, int32_t hw, int32_t h,
              uint16_t dang,
              uint8_t top, uint8_t bright, uint8_t dark);

// ---------------------------------------------------------------------------
// Models
// ---------------------------------------------------------------------------

#define W_NONE 0xFF

typedef struct {
  int32_t  x, y;
  uint16_t ang;
  uint8_t  kind;       // PED_MAN / PED_WOMAN
  uint8_t  is_cop;
  uint8_t  phase;      // walk cycle, 0..255
  uint8_t  anim;       // attack frames remaining
  uint8_t  weapon;     // W_* held in the right hand, or W_NONE
  uint8_t  dead;
  uint8_t  is_player;
  uint8_t  cuffed;     // arrested: kneeling, hands behind the back
  uint8_t  undress;    // dancer: 0 dressed, 1 top off, 2 everything off
  uint8_t  burnt;      // died on fire: charred, not bloodied
  uint8_t  burning;    // still alive and alight
  int16_t  z;          // off the ground while ragdolling
  uint8_t  role;       // PA_* indoors: picks the palette
} PedDraw;

typedef struct {
  int32_t  x, y;
  uint16_t ang;
  uint8_t  model;      // CAR_*
  uint8_t  is_cop;
  uint8_t  phase;      // wheel spin
  uint8_t  door;       // driver door swing, 0..64
  uint8_t  wrecked;
  uint8_t  hp;
  uint8_t  is_amb;     // motorcycle: riderless
  uint8_t  fire;       // burning
  int16_t  z;          // thrown into the air by a blast
} CarDraw;

// A wheel drawn as a wheel: an octagonal tyre disc with a hub, in the
// vertical plane along the car's axis. Only the side facing the camera is
// drawn - from the other flank the tread box does the silhouette.
// A frustum: same cost as gfx_part(), but the top face can be a different
// size and offset from the bottom. tl/tw are 8.8 fractions (256 = no taper).
// Bounded integer/string formatting, so the HUD never needs snprintf.
// Draw fills as a checkerboard, for see-through occluders.
void gfx_set_stipple(bool on);
int  wr_draw_capacity(void);

int gfx_utoa(char *dst, int cap, uint32_t v);
int gfx_puts(char *dst, int cap, const char *src);

// A model part in a table: a box or frustum in the model's own frame, 13
// bytes. Geometry is stored at half scale for vehicles and props (scale 2)
// and full scale for people. tl/tw are in quarters (64 = no taper). A colour
// byte below 0x10 indexes the palette given at draw time; anything else is
// a literal GColor8. Every gfx_part() call costs 40-60 bytes of argument
// setup; a table entry costs 13 and no code.
typedef struct {
  int8_t  ox, oy, oz, hl, hw, h, ts;
  uint8_t tl, tw, dang;
  uint8_t c[3];
} PartRec;
#define PART(ox,oy,oz,hl,hw,h,tl,tw,ts,dang,c0,c1,c2) \
  { (int8_t)((ox)/2), (int8_t)((oy)/2), (int8_t)((oz)/2), (int8_t)(((hl)+1)/2), \
    (int8_t)(((hw)+1)/2), (int8_t)(((h)+1)/2), (int8_t)((ts)/2), (uint8_t)((tl)/4), \
    (uint8_t)((tw)/4), (uint8_t)((uint16_t)(dang) >> 8), { (c0), (c1), (c2) } }
#define BOX(ox,oy,oz,hl,hw,h,c0,c1,c2) PART(ox,oy,oz,hl,hw,h,256,256,0,0,c0,c1,c2)
// Colour shorthands for the tables.
#define W3 ARGB(3,3,3)
#define K0 ARGB(0,0,0)
#define G1 ARGB(1,1,1)
#define G2 ARGB(2,2,2)
void gfx_parts(int32_t x, int32_t y, uint16_t ang, int32_t z, int32_t lean,
               const PartRec *p, int n, int scale, const uint8_t *pal);
uint8_t gfx_shade(uint16_t normal, uint8_t bright, uint8_t dark);
bool gfx_in_view(int32_t x, int32_t y);
extern uint16_t g_light;
void gfx_taper(int32_t cx, int32_t cy, uint16_t ang,
               int32_t ox, int32_t oy, int32_t oz,
               int32_t hl, int32_t hw, int32_t h,
               int32_t tl, int32_t tw, int32_t tshift,
               uint16_t dang,
               uint8_t top, uint8_t bright, uint8_t dark);

void gfx_wheel_disc(int32_t cx, int32_t cy, uint16_t ang,
                    int32_t ox, int32_t oy, int32_t cz, int32_t r,
                    uint8_t tyre, uint8_t hub);

void model_ped(const PedDraw *p, int32_t dist);
void model_car(const CarDraw *c, int32_t dist);
void model_pickup(int32_t x, int32_t y, uint8_t weapon, uint32_t frame);
void model_boom(const Boom *b);
void model_prop(const Prop *p);
void model_boat(const Ent *e, int32_t dist);

// Level-of-detail thresholds in world subunits (256 = one tile). Full rigs are
// expensive, so only what you are actually looking at gets one; the player is
// always full detail regardless.
#define LOD_FULL  1500
#define LOD_MID   2500
