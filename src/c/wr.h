#pragma once
#include <pebble.h>

// ---------------------------------------------------------------------------
// Wrist Revoked - shared header
//
// Units:
//   World positions are int32 "subunits". 256 subunits = 1 city tile.
//   Angles are uint16 (0..65535 = full turn), so they wrap for free.
//   Trig returns Q10 (-1024..1024).
// ---------------------------------------------------------------------------

// The watch's screen. A phone build may ask for a bigger framebuffer; the
// pixel constants below scale with it (PX), so the picture keeps its
// framing and just has more pixels in it.
#ifndef WR_ANDROID
#define WR_ANDROID 0
#endif

// Pebble Round 2 (gabbro): a 260x260 circle. The domain blobs are built
// with -DWR_ROUND=1 -DSCR_W=260 -DSCR_H=260 by the wscript (they see the
// host-check pebble.h, not the SDK's platform macros); the image gets it
// from the SDK. Everything round-only sits behind WR_ROUND, so the Time 2
// build compiles exactly what it did before.
#if !defined(WR_ROUND) && defined(PBL_PLATFORM_GABBRO) && !WR_ANDROID
#define WR_ROUND 1
#endif
#ifndef WR_ROUND
#define WR_ROUND 0
#endif
#if WR_ROUND
#ifndef SCR_W
#define SCR_W 260
#endif
#ifndef SCR_H
#define SCR_H 260
#endif
#endif

#ifndef SCR_H
#define SCR_H 228
#endif
#if WR_ANDROID
// The phone's width follows the screen's aspect at run time - a wide
// screen shows more world, at the same scale, rather than a stretched one.
// The framebuffer is allocated at the maximum.
extern int g_scr_w;
#define SCR_W g_scr_w
#define SCR_W_MAX 1600
#else
#ifndef SCR_W
#define SCR_W 200
#endif
#endif
#define PX(v) ((v) * SCR_H / 228)

// The HUD's drawing space. On the Time 2 and the phone it is the watch's
// 200x228 (a phone scales the calls); on the round watch it is the panel
// itself, and the layout follows the bezel (see draw_hud). HUD_OX/HUD_OY
// centre a 200x228 layout (the pause menu, the map) in the circle.
#if WR_ROUND
#define HUD_W  SCR_W
#define HUD_H  SCR_H
#define HUD_L(v) (v)
#define HUD_OX ((SCR_W - 200) / 2)
#define HUD_OY ((SCR_H - 228) / 2)
#define HUD_RC (SCR_W / 2)            // the circle's centre and radius
#define HUD_IN_DY 22                  // a room's own line, below the weapon's name
#else
#define HUD_H 228
#define HUD_W (SCR_W * 228 / SCR_H)
#define HUD_L(v) ((v) * 228 / SCR_H)
#define HUD_OX 0
#define HUD_OY 0
#define HUD_IN_DY 0
#endif

#define TILE      256     // subunits per city tile
#define TILE_SH   8       // log2(TILE)

// Camera. Pitch is no longer fixed: it slides with zoom. Pulled in close the
// camera looks up the street; pushed out it tips toward straight overhead.
// That is the one mapping that makes both ends useful - a close top-down view
// shows nothing, and a distant low view is all rooftops.
// Fully in it is a GTA 3 chase camera: down at street level, almost level with
// the horizon, close enough behind him to count the polygons. Fully out it is
// near top-down.
#define PITCH_NEAR   10   // degrees below horizontal, fully zoomed in
#define PITCH_FAR    66   // ...and fully zoomed out
extern int32_t g_cam_cp, g_cam_sp;   // cos/sin of the current pitch, Q10

// Distance and height are NOT fixed - the rig eases back as you pick up speed.
// Standing still you are zoomed right in on your character; at full pelt in a
// car it pulls out so you still get a useful look up the road. Character
// height on screen goes from ~33 px parked to ~20 px flat out.
#define CAM_BACK_NEAR  300
#define CAM_BACK_FAR   900
#define CAM_H_NEAR     140   // right down at shoulder height
#define CAM_H_FAR     1150
#define CAM_EASE         3     // shift: higher = lazier zoom

#define FOCAL     PX(270)
#define NEAR_Z    80

// Resident Evil 4 style over-the-shoulder framing, biased for a left thumb.
// Two things happen at once: the projection centre slides right so the
// character sits clear of the thumb, and the whole camera yaws a few degrees
// so he is angled across the frame rather than staring straight up it. The
// yaw is what stops the offset feeling like the camera is just mis-aimed.
#if WR_ANDROID
#define PROJ_CX   (PX(154) + (SCR_W - PX(200)) / 2)   // the watch's offset from the middle, whatever the width
#elif WR_ROUND
#define PROJ_CX   (SCR_W / 2 + PX(26))   // a circle has no room at its sides: nearer the middle
#else
#define PROJ_CX   PX(154)     // character sits right of centre, with room to his right
#endif

// Where the character should sit vertically, as a fraction of screen height.
// PROJ_CY is NOT a constant: pitch slides with zoom, and a fixed centre would
// swing him from the bottom edge to the top as you zoom. Each frame we solve
// for the centre that puts him exactly here instead.
// Where he sits vertically also slides: down the frame when the camera is low
// (leaving sky above) and back up when it goes overhead.
#define PLAYER_Y_NEAR PX(182)
#define PLAYER_Y_FAR  PX(148)
extern int32_t g_proj_cy;
extern int32_t g_horizon;      // screen y of the true horizon
#define CAM_YAW_OFF 4600  // ~25 degrees; positive swings his heading left

#define DRAW_AHEAD  11    // tiles of city drawn in front
#define DRAW_BEHIND 3
#define DRAW_SIDE   7

#define FRAME_MS  40      // 25 fps target

// Virtual analogue stick. Floating: wherever your thumb lands becomes centre,
// so you never hunt for a pad you cannot see under your finger. It still
// parks in the corner when idle so it stays discoverable.
#if WR_ROUND
#define STICK_HOME_X PX(60)   // inside the circle, clear of the bezel
#define STICK_HOME_Y PX(165)
#else
#define STICK_HOME_X PX(38)   // resting position when nobody is touching it
#define STICK_HOME_Y PX(190)
#endif
#define STICK_DEAD    PX(4)
#define STICK_RANGE  PX(26)   // px from centre to full deflection

// Shake the police for this many frames and dispatch stops sending units,
// which is what makes escaping actually possible.
#define EVADE_CLEAR 250      // ten seconds out of sight before the heat starts to bleed

// ---------------------------------------------------------------------------
// Fixed point / trig
// ---------------------------------------------------------------------------

extern const int16_t WR_SIN[256];
int32_t wr_muldiv(int32_t a, int32_t b, int32_t c);

static inline int32_t fsin(uint16_t a) { return WR_SIN[(a >> 8) & 255]; }
static inline int32_t fcos(uint16_t a) { return WR_SIN[((a >> 8) + 64) & 255]; }

static inline int32_t wr_abs(int32_t v) { return v < 0 ? -v : v; }

// Multiply by a Q10 trig value and scale back, rounding symmetrically.
//
// A plain >>10 is an arithmetic shift, which rounds toward negative infinity.
// On a 45-degree heading the x and y components have equal magnitude but
// opposite signs, so the negative one loses a unit and the positive one does
// not - the object creeps sideways off the line it is supposed to be
// travelling along, about one unit per frame. Over a long straight that walks
// a car right off the road.
static inline int32_t fmul(int32_t v, int32_t f) {
  int32_t p = v * f;
  return (p >= 0) ? (p >> 10) : -((-p) >> 10);
}
static inline int32_t wr_clamp(int32_t v, int32_t lo, int32_t hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// Cheap approximate length. Max error ~4%, no sqrt, no divide.
static inline int32_t wr_dist(int32_t dx, int32_t dy) {
  dx = wr_abs(dx); dy = wr_abs(dy);
  return (dx > dy) ? (dx + (dy >> 1)) : (dy + (dx >> 1));
}

// Integer square root. The analogue stick needs a real magnitude, not the
// octagonal wr_dist() approximation, or the pad ends up subtly egg-shaped.
static inline int32_t wr_sqrt(int32_t v) {
  if (v <= 0) return 0;
  int32_t r = v, last;
  do { last = r; r = (r + v / r) >> 1; } while (r < last);
  return last;
}

uint32_t wr_rand(void);
static inline uint32_t wr_rand_n(uint32_t n) { return wr_rand() % n; }

// ---------------------------------------------------------------------------
// City
//
// Nothing about the city is stored. cell_at() is a pure function of the tile
// coordinate, so the whole island is an algorithm rather than 4.6 KB of grid.
// Buildings are generated as *lots* - one solid box each, covering several
// tiles - which is what stops blocks looking like a row of hollow shells.
// ---------------------------------------------------------------------------

enum {
  CELL_ROAD = 0,
  CELL_PAVE,      // sidewalk, walkable, cars allowed but slow
  CELL_BLD,       // inside a building footprint
  CELL_PARK,      // grass
  CELL_SAND,      // beach
  CELL_WATER,     // off the island
};

#define DIAG_EVERY  11                // tiles between diagonal avenues
#define DIAG_PERIOD (DIAG_EVERY * TILE)

// Roads have a real width in world units, measured perpendicular to their
// centreline - not "this tile is road". A one-tile-wide diagonal line of
// axis-aligned tile quads renders as a staircase of separate squares, which
// is exactly the checkerboard it used to look like. Bands get drawn as long
// rotated quads instead.
#define ROAD_HALF   250               // centreline to kerb, subunits
#define PAVE_HALF   470               // ...to the back of the pavement
#define ROAD_LINES  3                 // streets drawn either side of the player
#define DASH_STEP   (TILE * 3)        // spacing of centre-line dashes
#define DASH_LEN    (TILE / 2)        // half-length of one dash
#define BLOCK_T     ROAD_EVERY
// Two islands. The main one, and a smaller one across the water, joined by a
// highway that runs clean across both and becomes a bridge over the gap.
#define ISLAND_CX   40
#define ISLAND_CY   40
#define CITY_R      24                // city extends this many tiles from centre
#define SAND_R      28                // beach out to here, then water

#define ISLAND_BX   102
#define ISLAND_BY   54
#define CITY_BR     17
#define SAND_BR     21

// The third island: the ghetto. South of the city across open water. Its
// bridge is the collapsed one (see DESIGN.md); until that lands it is a
// landmass you reach by boat.
#define ISLAND_GX   46
#define ISLAND_GY   108
// The secret island: far out, small, no roads, no bridge. A runway and a house.
#define ISLAND_SX   118
#define ISLAND_SY   118
#define CITY_SR     8
#define SAND_SR     11
#define CITY_GR     16
#define SAND_GR     20

#define CITY_MAX    ((ISLAND_GY + SAND_GR + 8) * TILE)

// Every island the world knows about. cell_at_point() finds the nearest
// centre; scenery, police tuning and the militia key off the index.
typedef struct { int16_t cx, cy; uint8_t city_r, sand_r; } Island;
enum { ISL_CITY = 0, ISL_COAST, ISL_GHETTO, ISL_SECRET, N_ISLAND };
extern const Island ISLANDS[N_ISLAND];
int island_at(int32_t x, int32_t y);      // index of the nearest island

// Bridges. Each runs along the line between two island centres, from t0 to
// t1 in 1024ths of that line. A bridge may have a GAP - a stretch with no
// deck, gap0..gap1 - which is the collapsed span to the ghetto: hit it fast
// enough and you clear it; slow, and you are in the water with the wrecks
// of everyone who tried before you. Bridge 0 is the causeway, so every
// caller that only knew one bridge still means the same thing.
typedef struct { uint8_t a, b; uint16_t t0, t1, gap0, gap1, ang; } Bridge;
enum { BR_CAUSEWAY = 0, BR_GHETTO, N_BRIDGE };
extern const Bridge BRIDGES[N_BRIDGE];
// The bridge whose corridor holds (x,y), or -1. lat is signed offset from the
// centreline (+left of a->b travel), t the position along it in 1024ths.
int  bridge_find(int32_t x, int32_t y, int32_t *lat, int32_t *t);
// Fire one round from a weapon at (x,y) toward aim. flags are from_cop bits.
void ent_fire(int32_t x, int32_t y, uint16_t aim, uint8_t wep, uint8_t flags);
int  ent_nearest_of(uint8_t type, int32_t x, int32_t y, int32_t range, uint8_t aux_or_0);
bool bridge_in_gap(int b, int32_t t);
uint16_t bridge_angle_of(int b);
int32_t  bridge_height_of(int b, int32_t along);

// The highway. Its centreline runs between the two island centres, so it is a
// main road on land and a bridge over the water without being two systems.
// Where the causeway begins and ends, as thousandths along the line between
// the island centres. The water gap runs from about 0.43 to 0.59 of that
// line; these add roughly three tiles of ramp at each end so the deck meets
// the shore rather than stopping over the beach.
// Measured along the line, by probing terrain 400 units to the SIDE of the
// bridge so the deck's own tiles do not answer the question. Land ends at
// about 462 on the near side; on the far side the coast wobble takes a bite
// out of island B and solid ground does not resume until 705 - well past its
// nominal sand radius. Setting T1 by the radius alone ended the deck over
// open sea, which is a road that stops in the water.
#define BRIDGE_T0   436
#define BRIDGE_T1   826

// Deck height at mid-span. The causeway lifts off the shore, arches over the
// channel and comes back down, so a boat can pass underneath it. Held as a
// function of position along the span rather than stored anywhere.
#define BRIDGE_RISE 150

#define BRIDGE_HALF 300               // deck half-width, subunits
#define BRIDGE_KERB 420               // ...to the outside of the barrier

// The coastal ring road: a two-lane carriageway hugging the shore, with a
// strip of pavement on its landward side.
#define RING_HALF   256
#define RING_PAVE   140

// ---------------------------------------------------------------------------
// Roads, described once.
//
// Three kinds of road - the two diagonal street families, the coastal ring
// and the causeway - and one description of them. The terrain classifier,
// the renderer, the junction markings and the traffic AI all read roads
// through road_here(), so they cannot disagree about where the tarmac is.
// That disagreement is what put kerbs across carriageways and crosswalks
// on straight roads.
// ---------------------------------------------------------------------------
enum { RD_DIAG_U = 0, RD_DIAG_V, RD_RING, RD_BRIDGE };
typedef struct {
  uint8_t  kind;
  uint16_t head;    // canonical direction along the road
  int16_t  lat;     // offset from the centreline, positive to the LEFT of head
  int16_t  half;    // carriageway half-width
  int16_t  pave;    // ...and to the back of the pavement
  int32_t  cx, cy;  // ring only: the island centre...
  int32_t  base;    // ...and its unwobbled city radius, subunits
} Road;
// Every road within pavement reach of a point (0..4: causeway, ring, one street of each family).
int  road_here(int32_t x, int32_t y, Road *out);
// The road a vehicle pointing `pref` is on: the best-aligned one under it.
bool road_at(int32_t x, int32_t y, uint16_t pref, Road *out);
// The point `d` further along the road from (x, y) in the direction `hd`,
// sitting `lat` to the left of the canonical heading. Straight roads step
// straight; the ring follows its own wobbled arc, which is what keeps a
// probe on the tarmac round a tight bend.
void road_point(const Road *rd, int32_t x, int32_t y, uint16_t hd, int32_t d,
                int32_t lat, int32_t *ox, int32_t *oy);
// The four diagonal arm headings, and which of them are live at a crossing.
extern const uint16_t ROAD_ARM[4];
uint8_t city_junction_arms(int32_t jx, int32_t jy);
bool road_paint_ok(int32_t x, int32_t y, uint8_t kind);   // the dash rule, for the paint scan

// True if this point is on the highway, and how far along it (0..1024).
bool on_bridge(int32_t x, int32_t y, int32_t *dist, int32_t *along);
// How high the deck sits at a given point along the span: 0 at both shores,
// BRIDGE_RISE in the middle.
int32_t bridge_height(int32_t along);
uint16_t bridge_angle(void);
// The coastline's angular wobble, in subunits, for the direction (dx, dy) from
// an island centre. Scale-invariant, so subunit deltas work too. The renderer
// needs it to stop the street bands at the same wobbled shoreline the terrain
// classifier uses.
int32_t city_coast_wobble(int32_t dx, int32_t dy);

// cell_at_point() classifies an exact world position. cell_at() is just that
// sampled at a tile centre, and is only for drawing terrain patches.
//
// Everything that MOVES has to use the point version. A road is a band with a
// width, so sampling it at tile centres staircases the edges - which had AI
// cars deciding they were off-road every other tile and stopping dead on the
// diagonals.
uint8_t cell_at_point(int32_t x, int32_t y);
uint8_t cell_at(int32_t tx, int32_t ty);
static inline uint8_t cell_at_world(int32_t x, int32_t y) {
  return cell_at_point(x, y);
}

// Building lot kinds.
enum { LOT_SOLID = 0, LOT_GARAGE, LOT_STORE, LOT_GUNSHOP, LOT_CLUB, LOT_KIND_COUNT };

typedef struct {
  int32_t x0, y0, x1, y1;   // footprint in subunits
  int16_t h;                // height in subunits
  uint8_t mat;
  uint8_t kind;             // LOT_*
  uint8_t glass;            // storefront: 0 intact, 255 fully shattered
} Lot;

// The airport is a fixed spot on each island that has one: the lot that
// contains that spot, whatever kind the generator gave it. One per island,
// always, which a hash of lot corners could not promise on a small island.
static inline bool island_has_airport(int isl) { return isl == ISL_GHETTO || isl == ISL_COAST; }
bool lot_is_airport(const Lot *L);
bool lot_is_home(const Lot *L);      // in wr_city.c; domains reach it through the context

// Lots for the blocks around the player, rebuilt only when he crosses into a
// new block. Everything - drawing, collision, prop placement - reads this.
#define LOT_CACHE_B  5                       // 5x5 blocks
// Pool sizes are measured, not guessed. A Pebble app header stores the
// loaded image size in a SIXTEEN-BIT field, so text + data + bss must come
// in under 65,536 bytes - a much harder wall than the 128 KB heap budget,
// and the one that actually stopped the first real build at 67,365 bytes.
// Peaks observed over 9,000 frames of five-star play, and over the whole
// map across six world seeds:
//
//   lots  30 of 150      entities 39 of 80
//   props 40 of  84      draw list 58 of 300
//
// These are sized at roughly 1.5x the measured peak, which leaves real
// headroom while returning about 4 KB of the RAM image.
// Size switches. The app image has a hard 64 KB ceiling and this project
// has now hit it four times, with the pools trimmed to about 25% above their
// measured peaks - there is very little left to take from data. These drop
// the three largest blocks of purely decorative CODE instead. They are
// compile-time constants rather than #ifdef so the code still has to compile
// and stay correct; the optimiser removes the bodies.
#ifdef WR_LEAN
#define DECOR_MURALS   0     // the Mirror app wall ads
#define DECOR_SETBACKS 0     // tower crowns and beacon masts
#define DECOR_ROOFTOPS 0     // stair heads and units on flat roofs
#else
#define DECOR_MURALS   1
#define DECOR_SETBACKS 1
#define DECOR_ROOFTOPS 1
#endif

#define MAX_LOT      32
extern Lot g_lot[MAX_LOT];
extern int g_nlot;

void city_refresh(int32_t px, int32_t py);   // call once per frame
// Move a point onto the nearest street centreline, and report the heading of
// that street. Traffic uses this so it spawns in a lane pointing down it
// rather than across it.
void road_snap(int32_t *x, int32_t *y, uint16_t *ang);
void city_set_interior(bool on);
bool city_blocked(int32_t x, int32_t y, bool is_car);
bool car_lane_blocked(int32_t x, int32_t y);
// Boats are the mirror image of everything else: open water is passable and
// land is not.
bool water_blocked(int32_t x, int32_t y);
int  lot_at(int32_t x, int32_t y);           // index into g_lot, or -1

// Parks and junctions, as geometry rather than tiles. A park painted tile by
// tile has square edges against the smooth diagonal pavement, which is what
// made them read as odd grey squares dropped into the grass; drawn as the
// block's own diamond it lines up with the streets exactly.
#define MAX_PARKQ 12
int  city_park_quads(int32_t px, int32_t py, int32_t (*out)[4][2], int max);
// Junction centres near a point, for signals and road markings.
#define MAX_JUNC 9
// Parking bays along the seafront: points on the sand side of the coastal
// ring road, spaced out, with a heading square to the kerb. Deterministic
// from position, so bays and their meters never wander between frames.
int  city_junctions(int32_t px, int32_t py, int32_t (*out)[3], int max);   // {x, y, arms}

// Parked cars in a parking garage. Deterministic from the lot itself, so the
// renderer and the collision test agree without storing anything: out[i] is
// { x, y, ang }. Returns how many were written.
#define GARAGE_SLOTS 6
int  city_garage_slots(const Lot *L, int32_t (*out)[3], int max);

// ---------------------------------------------------------------------------
// Street furniture
// ---------------------------------------------------------------------------

enum { PROP_LAMP = 0, PROP_SIGN, PROP_POSTBOX, PROP_TABLE, PROP_CHAIR,
       PROP_HYDRANT, PROP_TREE, PROP_PALM, PROP_ROCK, PROP_PARASOL,
       PROP_COUNTER, PROP_CABINET, PROP_PYLON, PROP_DOCK, PROP_LIGHT,
       PROP_BIRD, PROP_BARRIER, PROP_DOOR,
       PROP_WALLGUN, PROP_STAGE, PROP_POLE, PROP_DISCO, PROP_KIND_COUNT };
// Kinds from here up belong to the island's scenery domain: it places them
// (DOP_PROP) and draws them (DOP_DRAW_PROP). They are 255 hp and solid.
#define PROP_DOMAIN0 200
extern uint8_t g_park_col, g_lot_col;  // the scenery's colours for parks and block ground

// Things you cannot simply drive through until they have been knocked loose.
// Palms and boulders deliberately are NOT in this list: the beach should be
// drivable, and a scattering of solid obstacles on open sand felt like the
// terrain itself was stopping you.
static inline bool prop_solid(uint8_t kind) {
  if (kind >= PROP_DOMAIN0) return true;
  return kind == PROP_LAMP || kind == PROP_TREE ||
         kind == PROP_POSTBOX || kind == PROP_COUNTER ||
         kind == PROP_CABINET || kind == PROP_BARRIER ||
         kind == PROP_POLE;
  // Signal masts are deliberately NOT solid. There are now two on every
  // junction corner, which is exactly where you turn, and as hard
  // obstacles they made a high-speed getaway impossible - a five-star
  // escape stopped being achievable at all. They shear off their base like
  // any other post instead, which is what the physics is for.
  // Pylons and docks are deliberately absent: they sit in the water beside
  // the deck, and making them solid would fence the bridge in.
}

extern const uint8_t PROP_HP[PROP_KIND_COUNT];
#define MAX_PROP 42
typedef struct {
  int32_t x, y;
  int16_t z;
  int16_t vx, vy, vz;
  uint16_t ang;
  int16_t spin;
  int16_t lean;        // how far it has been knocked over, 0..16384
  uint8_t kind;
  uint8_t hp;
  uint8_t alive;
  uint8_t loose;       // 1 once it has snapped off its base
} Prop;
extern Prop g_prop[MAX_PROP];

void props_refresh(void);
void pickups_refresh(void);
void props_update(void);
void prop_hit(int i, int32_t vx, int32_t vy, int32_t force);

// ---------------------------------------------------------------------------
// Entities
// ---------------------------------------------------------------------------

enum { ENT_NONE = 0, ENT_PED, ENT_CAR, ENT_COP, ENT_COPCAR, ENT_HELI,
       ENT_BOAT, ENT_COPBOAT, ENT_AMB, ENT_CYCLIST };

extern uint32_t g_frame;

// Traffic lights. One global cycle: family 0 (the x+y streets) has green in
// one half, family 1 in the other. Uniform phase across the city is
// deliberate - it reads as coordination, and it means a car's stop test and
// the lamp a prop draws can never disagree.
#define LIGHT_CYCLE 150
static inline bool traffic_green(int fam) {
  return (int)((g_frame / LIGHT_CYCLE) & 1) == fam;
}

// Boats use the `sub` field the same way cars use it.
enum { BOAT_JET = 0, BOAT_MOTOR, BOAT_YACHT, BOAT_POLICE, BOAT_MODEL_COUNT };

// Domains: code loaded from resources into an arena at runtime, instead of
// living in the app image. OFF until enough of the game has been moved out
// to pay for the arena - see DESIGN.md for the arithmetic. With it off, the
// domains are compiled in statically and the game behaves exactly as it did.
#ifndef WR_DOMAINS
#define WR_DOMAINS 0
#endif
// Testing: the player takes no damage and is never arrested. Set in the
// wscript. Never ship with it on.
#ifndef WR_GOD
#define WR_GOD 0
#endif

#if !WR_DOMAINS
#undef WR_MONOLITH
#define WR_MONOLITH 1
#endif

// Boats - four hulls, the marinas and the marine unit - are a compile-time
// option (about 2.5 KB). On by default since the linker map showed where
// the real fat was; -DWR_BOATS=0 is the first thing to flip if a build
// lands over the ceiling.
#ifndef WR_BOATS
#define WR_BOATS 0
#endif
static inline bool ent_is_boat(uint8_t type) {
  return WR_BOATS && (type == ENT_BOAT || type == ENT_COPBOAT);
}

// Marinas: fixed points on each island's shore where boats are moored.
#define MARINA_COUNT 4
extern const int16_t MARINA_TX[MARINA_COUNT];
extern const int16_t MARINA_TY[MARINA_COUNT];
int  nearest_marina(int32_t x, int32_t y, int32_t *dist);

// Ped subtypes. GTA 1 populated its streets with adults only; same here.
enum { PED_MAN = 0, PED_WOMAN, PED_KIND_COUNT };

enum { CAR_SEDAN = 0, CAR_TAXI, CAR_VAN, CAR_SPORTS, CAR_TRUCK,
       CAR_JEEP, CAR_BIKE, CAR_BUS, CAR_CYBER, CAR_TANK, CAR_MODEL_COUNT };

enum { ST_IDLE = 0, ST_WALK, ST_FLEE, ST_DRIVE, ST_CHASE, ST_DEAD, ST_ROPE,
       ST_PARK };   // a parked car: real, stealable, and it stays put

#define HELI_ALT   320   // subunits above the street: low enough to stay in frame ahead of you
#define HELI_WANTED  4   // stars before air support turns up

// Peds, traffic, police on foot, patrol cars, the helicopter, moored boats and
// the marine unit all draw from this. At five stars on the water the old cap
// of 30 was already full before a single police boat could be created.
#define MAX_ENT 42

typedef struct {
  int32_t  x, y;        // subunits
  uint16_t ang;
  int16_t  spd;         // subunits per frame
  int16_t  aux;         // per-type scratch: ambulance = corpse index + 1
  uint8_t  type;        // ENT_*
  uint8_t  sub;         // model / kind
  uint8_t  hp;
  uint8_t  state;       // ST_*
  uint8_t  timer;
  uint8_t  occupied;    // car: 1 if the player is driving it
  uint8_t  phase;       // walk cycle / wheel spin, 0..255
  uint8_t  door;        // car: driver door swing, 0..64
  uint8_t  anim;        // frames left of an attack animation
  uint8_t  wep;         // cop: what he carries, and drops when killed
  uint8_t  hit_cd;      // frames until this vehicle can take crash damage again
  uint8_t  fire;        // burning: counts down to the explosion
  int16_t  z;           // height off the ground (ragdolls, only)
  int16_t  vx, vy, vz;  // ragdoll velocity
  int16_t  spin;        // ragdoll tumble rate
} Ent;

extern Ent g_ent[MAX_ENT];

// ---------------------------------------------------------------------------
// Weapons and pickups
// ---------------------------------------------------------------------------

// W_MG is SWAT issue only - the one way to get one is to take it off a body.
enum { W_FIST = 0, W_BAT, W_PISTOL, W_SHOTGUN, W_MG,
       W_FLAME, W_ROCKET, W_GRENADE, W_CASH, W_COUNT };

#define MAX_PICKUP 12
typedef struct {
  int32_t x, y;
  uint8_t weapon;
  uint8_t alive;
  uint8_t world;     // 1 = placed by the city, 0 = dropped by a body
} Pickup;
extern Pickup g_pickup[MAX_PICKUP];

#define MAX_BULLET 18
// A grenade is a slug with a fuse: it arcs out slowly and goes off when the
// fuse runs down, whether or not it hit anything. That is the whole appeal -
// you can bounce one off a wall and round a corner.
enum { B_SLUG = 0, B_FIRE, B_ROCKET, B_GRENADE };

typedef struct {
  int32_t x, y;
  int16_t vx, vy;
  uint8_t life;
  uint8_t dmg;
  uint8_t from_cop;  // bit 0: police; bit 1: fired from the air (clears walls)
  uint8_t kind;      // B_*
} Bullet;
extern Bullet g_bullet[MAX_BULLET];

#define MAX_GIB 24
typedef struct {
  int32_t x, y;
  int16_t z;
  int16_t vx, vy, vz;
  uint8_t life;
  uint8_t size;
  uint8_t col;       // GColor8 byte - blood is red, debris is whatever broke
  uint8_t stains;    // GIB_BLOOD leaves a pool; GIB_CASH becomes a pickup; GIB_BILL pays
} Gib;
enum { GIB_NONE = 0, GIB_BLOOD, GIB_CASH, GIB_BILL };
extern Gib g_gib[MAX_GIB];

// Same physics either way; only the colour and whether it stains differ.
void gib_burst(int32_t x, int32_t y, int32_t z,
               int32_t vx, int32_t vy, int n);
void gib_burst_debris(int32_t x, int32_t y, int32_t z,
                      int32_t vx, int32_t vy, int n, uint8_t col);

#define MAX_BLOOD 10
typedef struct {
  int32_t x, y;
  uint8_t life;
  uint8_t size;
} Blood;
extern Blood g_blood[MAX_BLOOD];

#define MAX_BOOM 4
#define BOOM_LIFE 22
typedef struct {
  int32_t x, y;
  uint8_t life;      // counts down; drives fireball size and colour
} Boom;
extern Boom g_boom[MAX_BOOM];

void boom_add(int32_t x, int32_t y);
// Set a car alight. Most wrecks burn for a while before they go up; a
// burning car that gets hit again goes up immediately.
void vehicle_ignite(int idx);
// Haptics. One place, so the whole game buzzes consistently and Quiet Time
// is honoured in a single check.
enum { VIBE_LIGHT = 0, VIBE_MED, VIBE_HEAVY };
void wr_vibe(uint8_t strength);

// Every point of damage the player takes goes through here, so armour is
// impossible to bypass by forgetting about it at one of the call sites -
// there are seven of them across two files, and the first version of this
// missed two.
#define ARMOR_MAX  100
#define ARMOR_MIL  250   // military grade: what the tank comes with
#define HP_MAX     150
#define PICKUP_ARMOR 255      // sentinel in Pickup.weapon: a vest, not a gun
#define PICKUP_CASH  254      // ...or money; Pickup.world holds the amount
void player_absorb(int32_t dmg);
void weapon_next(void);      // cycle to the next thing you are carrying
void game_input_hold(bool on);
bool vehicle_action(void);
void game_input_tap(void);   // SELECT pressed / released
bool audio_engine_active(void);

// Destroy a vehicle. Always go through this: it also throws the player out if
// the wreck happens to be the one they are sitting in, which several call
// sites used to forget.
void vehicle_destroy(int idx, bool explode);
extern uint8_t g_blame;                // the player is damaging a car right now
extern uint8_t g_cop_fire;             // a police round is doing the damage right now
void drive_along(Ent *e, bool seek, int32_t tx, int32_t ty, int32_t lane, int32_t cruise, bool police);
void ent_spawn(uint8_t type);

// ---------------------------------------------------------------------------
// Player / game state
// ---------------------------------------------------------------------------

typedef struct {
  int32_t  x, y;
  uint16_t ang;
  int16_t  spd;
  int16_t  turn;          // angular velocity, accelerates while held
  int8_t   turn_dir;      // -1 left, +1 right, 0 none
  int16_t  stick_f;       // virtual stick, Q8: +256 full forward, -256 full back
  int16_t  stick_s;       // virtual stick, Q8: +256 full right, -256 full left
  int16_t  strafe;        // current sideways speed (on foot only)
  int16_t  stick_ox;      // where the thumb landed - the stick's centre
  int16_t  stick_oy;
  int16_t  stick_x;       // where the thumb is now, screen px
  int16_t  stick_y;
  uint8_t  stick_on;
  uint8_t  zoom_on;       // a finger is on the zoom strip
  int16_t  zoom_ref;      // where that drag started
  int16_t  zoom_base;
  int16_t  zoom;          // 0 = closest, 256 = furthest
  uint8_t  in_car;        // index+1 into g_ent, 0 = on foot
  uint8_t  weapon;
  uint8_t  ammo;
  uint8_t  hp;
  uint8_t  wanted;        // 0..5
  uint16_t heat;          // drives wanted up/down
  uint16_t evade;         // frames since a cop last had eyes on you
  uint16_t kills;
  uint16_t stolen;
  uint8_t  attack_cd;
  uint8_t  phase;         // walk cycle
  uint8_t  anim;          // attack animation frames
  uint8_t  hurt_flash;
  uint8_t  arrest;        // frames an officer has had hands on you
  uint8_t  busted;        // 1 if the last takedown was an arrest, not a death
  uint8_t  inside;        // 1 while in the gun shop interior
  uint8_t  dead;
  uint8_t  armor;         // body armour: soaks damage before health does
  uint8_t  firing;        // SELECT is being held down
  // What you are carrying. One entry per weapon: rounds for a gun, 255 for
  // a melee weapon you simply own. g_p.ammo mirrors inv[weapon] while it is
  // in your hands and is written back when you switch.
  uint8_t  inv[W_COUNT];
  uint8_t  swim;          // in the water on foot
  uint16_t cine;          // frames since death/arrest; drives the cinematic
  uint8_t  heli_wep;      // flying: 0 = chain gun, 1 = rockets, 2 = LAND
  uint8_t  caption;       // a HUD message (CAP_*) and...
  uint8_t  caption_t;     // ...how long it has left
  uint16_t cash;          // dollars in hand
  uint8_t  held;          // frames SELECT has been down; 8 makes it a hold
  uint8_t  loading;       // frames the LOADING banner has left
  uint8_t  jump;          // frames left in the air over the collapsed bridge
  uint8_t  militia_hostile; // you attacked them; they attack you
  uint8_t  touch_used;      // this touch did its thing on the way down; ignore its moves
  // Knocked flying by a car: frames left, velocity, height and lift, tumble.
  uint8_t  knock;
  uint8_t  knock_grace;     // frames after a knockdown during which cars pass through you
  uint8_t  copkill_t, kwin; // the reinforcements rule: its window, and kills inside it
  uint16_t lull;            // frames of no police units: they are regrouping
  uint8_t  tier;            // police escalation: 0 the force, 1 air support, 2 the national guard
  uint8_t  residents;       // the secret island's population; persists across deaths and the app closing
  uint8_t  carry;           // carrying someone (a rescued dancer) to the island
  int16_t  kvx, kvy, kz, kvz;
  uint16_t kspin;
#if WR_ANDROID
  int16_t  wheel;           // the phone's steering wheel, -256..256, 0 = centred / not in use
#endif
  uint8_t  hostile;       // you have used a gun or killed an officer this chase
  uint8_t  copkills;      // officers killed this chase (the reinforcements rule counts it in a window too)
  uint16_t hot;           // frames spent at three stars or more this chase
} Player;

enum { CAP_NONE = 0, CAP_SWAT, CAP_LOST, CAP_GARAGE, CAP_HELI, CAP_UNLOCK, CAP_ALARM, CAP_OUT, CAP_CHAIR, CAP_FARE, CAP_DROP, CAP_PAID, CAP_PRICES, CAP_TAXI, CAP_TAKE, CAP_TICKET, CAP_NOGUNS, CAP_GATE, CAP_BOARDING, CAP_REINF, CAP_SAFE, CAP_STAY, CAP_RESCUE, CAP_SEATED, CAP_LANDED, CAP_COUNT };
void game_caption(uint8_t cap, uint8_t frames);
void add_heat(uint16_t amount);
#define SPD_MAX_HELI 34
#define HELI_ROCKETS 6
// Set by ent_update each frame: a police helicopter is airborne nearby.
extern uint8_t g_heli_up;

extern Player g_p;

void wr_seed(uint32_t s);
void ent_reset(void);
void ent_update(void);
int  ent_nearest(uint8_t type, int32_t range);
// A vehicle's half-length and half-width, from its model.
void car_half_dims(const Ent *e, int32_t *hl, int32_t *hw);
// Kernel functions the tenants reach through the context; declared here so
// the monolith (host, Android) sees them too.
void hud_text(GContext *ctx, const char *s, const char *font, int x, int y, int w, int h, GColor col);
void hud_marker(GContext *ctx, int32_t x, int32_t y, int32_t z, GColor col);
void fire_bullet_kind(uint16_t ang, uint8_t dmg, uint8_t kind);
int free_slot(void); int free_slot_evict(void);
void crash_damage(int idx, uint8_t dmg);
uint16_t angle_to(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
bool ent_any_cop_near(int32_t range, bool include_air);
bool player_under_cover(void);
void blood_add(int32_t x, int32_t y, uint8_t size);
void pickup_drop(int32_t x, int32_t y, uint8_t weapon);

// ---------------------------------------------------------------------------
// Pause menu
// ---------------------------------------------------------------------------

enum { MENU_MAP = 0, MENU_SIDE, MENU_VOLUME, MENU_SFX, MENU_QUIT, MENU_ROWS };
// The zoom slider runs past the old far stop: the camera keeps rising and
// backing off past 256, the pitch stops steepening there.
#define ZOOM_MAX 380

typedef struct {
  uint8_t open;
  uint8_t row;
  uint8_t editing;
  uint8_t volume;     // 0..100
  uint8_t sfx_on;
  uint8_t cam_left;   // over-the-LEFT-shoulder framing, slider on the left edge
  uint8_t map;        // the menu is showing the map
  uint8_t quit;       // set when the player picks Quit
} Menu;
extern Menu g_menu;

void menu_toggle(void);
void menu_nav(int8_t dir);
void menu_select(void);

// ---------------------------------------------------------------------------
// Interiors
// ---------------------------------------------------------------------------

// The gun shop is a separate little scene. Rather than build a second engine
// for it, entering swaps the lot cache for four wall lots and the props for
// counters and cabinets - so drawing, collision and physics all carry over
// unchanged.
// ---------------------------------------------------------------------------
// Interiors: the gun store and the strip club (wr_inside.c).
// ---------------------------------------------------------------------------
#define ROOM_OX   (240 * TILE)
#define ROOM_OY   (240 * TILE)
enum { IN_NONE = 0, IN_GUNSHOP, IN_CLUB, IN_AIRPORT, IN_HOME };
// Roles for people indoors, in the low byte of Ent.aux. A dancer keeps her
// clothing level in bits 8-9 and, when she is busy with a patron, PA_BUSY
// and his index in bits 10-15.
enum { PA_NONE = 0, PA_DANCER, PA_BOUNCER, PA_PATRON, PA_CLERK, PA_CUSTOMER };
#define PA_BUSY 0x200
typedef struct {
  uint8_t  kind;          // IN_*
  uint8_t  venue;         // which door, 0..7: banned[] and took[] are per venue
  int32_t  w, h;          // room size, subunits
  int32_t  door_x, door_y;
  int8_t   nx, ny;        // outward normal of the door face
  uint16_t meter;         // club: the show, 0..400; runs down by itself
  uint8_t  level;         // club: 0..4
  uint8_t  paid;          // club: you have thrown money this visit
  uint8_t  dancer;        // club: index + 1 of the dancer at your chair
  uint8_t  pinned;        // club: you are in the chair; the stick does nothing
  uint8_t  angry;         // club: the door staff are walking you out
  uint8_t  hostile;       // club: you hit the door staff
  uint8_t  alarm;         // either: the police are on their way
  uint16_t stay;          // frames without paying
  uint16_t credit;        // store: dollars thrown this visit
  uint8_t  unlocked;      // store: cabinets opened this visit (bitmask)
  uint8_t  banned;        // clubs you have been thrown out of, by venue
  uint8_t  took[8];       // store stock smashed out of its cabinet, by venue: gone
  uint16_t spent[8];      // store: dollars paid this life, by venue
  uint8_t  stage;         // airport: ticketing, security, terminal, cabin
  uint8_t  ticket;        // airport: you have one
  uint16_t wait;          // airport: frames in the current room
  uint8_t  leave;         // a room asked to be left; the kernel does it after the call
  uint8_t  after;         // ...and then: 1 = snap to the road (landed), 2 = the airliner
} Inside;
extern Inside g_in;
void inside_enter(uint8_t kind, int32_t dx, int32_t dy, int32_t nx, int32_t ny);
void inside_exit(void);
void inside_exit_now(void);
void inside_leave(void);
void inside_update(void);
void inside_money(int32_t x, int32_t y);
void inside_violence(bool killed);
bool inside_person(Ent *e);
void money_burst(int32_t x, int32_t y, int n);
void money_on_death(const Ent *e);
void money_throw(void);
void pickup_cash(int32_t x, int32_t y, uint8_t value);

// Driver junction decisions, kept in Ent.wep (wr_ent.c).
enum { TURN_NONE = 0, TURN_STRAIGHT, TURN_LEFT, TURN_RIGHT, TURN_BACK };

// The taxi: steal a cab, pick up a fare, drop them where they ask.
#ifndef WR_TAXI
#define WR_TAXI 1
#endif
enum { PA_FARE = 6, PA_MILITIA = 7, PA_GUARD = 8 };   // Ent.aux: waiting for the cab; the ghetto's armed faction; a soldier
// Bullet.from_cop bits: 1 police, 2 fired from a vehicle, 4 the militia.
#define B_FROM_COP     1
#define B_FROM_MILITIA 4
typedef struct {
  uint8_t fare;                // index + 1 of the person waiting, 0 = none
  uint8_t aboard;              // 1 while they are in the back
  uint8_t wait;                // frames until the next fare is found
  int32_t dx, dy;              // where they want to go
} Taxi;
extern Taxi g_taxi;
void taxi_update(void);
void city_shop_door(const Lot *L, int32_t *dx, int32_t *dy,
                    int32_t *nx, int32_t *ny);
// Every club and gun store on the map (positions >> 2), for the HUD.
#define MAX_VENUE 18
typedef struct { int16_t x, y; uint8_t club; } Venue;   // club: 0 store, 1 club, 2 airport
extern Venue g_venue[MAX_VENUE];
extern int   g_nvenue;
void city_venues_init(void);
// The old gun-shop names, for the harnesses.
#define shop_enter(dx, dy, nx, ny) inside_enter(IN_GUNSHOP, (dx), (dy), (nx), (ny))
#define shop_exit() inside_exit()
#define ROOM_W (g_in.w)
#define ROOM_H (g_in.h)

void game_init(void);
void game_step(void);
void game_input_turn(int8_t dir);
#if WR_ANDROID
void game_turn_by(int32_t dang);
#endif
void game_input_action(void);
void game_touch_down(int16_t sx, int16_t sy);
void game_touch_move(int16_t sx, int16_t sy);
void game_touch_up(void);

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void render_frame(GContext *ctx, GRect bounds);

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------

enum {
  SFX_PUNCH = 0, SFX_BAT, SFX_PISTOL, SFX_SHOTGUN,
  SFX_SCREAM_M, SFX_SCREAM_W, SFX_CAR_START, SFX_CRASH,
  SFX_PICKUP, SFX_BUSTED, SFX_WANTED_UP, SFX_ESCAPE,
  SFX_DIE_M, SFX_DIE_W, SFX_EXPLODE, SFX_DOOR,
  SFX_SMASH, SFX_ARREST, SFX_FOOTSTEP, SFX_FLAME, SFX_ROCKET, SFX_GLASS,
  SFX_RUNOVER,
  SFX_COUNT
};

void audio_init(void);
void audio_sfx(uint8_t id);
void audio_tick(void);
void audio_hush(void);     // engine, sirens, ambience; feeds the PCM stream
void audio_shutdown(void);
void audio_set_volume(uint8_t v);
void audio_set_enabled(bool on);
