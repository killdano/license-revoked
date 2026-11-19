// The aircraft models: the police helicopter and the airliner. Included by
// the air domain (unoccupied helicopters) and the flying tenant (yours).
static const PartRec HELIP[] = {
  BOX(6, -34, 0, 60, 5, 5, K0, K0, K0), BOX(6, 34, 0, 60, 5, 5, K0, K0, K0),
  BOX(20, 0, 4, 4, 34, 10, K0, K0, K0), BOX(-14, 0, 4, 4, 34, 10, K0, K0, K0),
  PART(0, 0, 14, 62, 30, 44, 180, 170, -14, 0, W3, W3, ARGB(2,2,3)),
  BOX(-4, 0, 26, 50, 31, 10, ARGB(0,0,3), ARGB(0,0,3), ARGB(0,0,2)),
  PART(54, 0, 20, 22, 24, 30, 140, 140, 10, 0, ARGB(0,2,3), ARGB(0,2,3), ARGB(0,1,2)),
  PART(-90, 0, 32, 44, 6, 12, 140, 140, -14, 0, W3, W3, ARGB(2,2,3)),
  BOX(-128, 0, 34, 6, 5, 34, W3, W3, ARGB(2,2,3)),
  BOX(34, 0, 30, 8, 9, 20, ARGB(0,0,1), ARGB(0,0,1), K0),
  BOX(0, 0, 58, 4, 4, 8, K0, K0, K0),
};

// The airliner: a long fuselage, wings, a tail, engines under the wings.
// Shadow on the ground like the helicopter's.
static const PartRec PLANEP[] = {
  // ox   oy  oz  hl  hw   h  ts  tl  tw dang  colours (top, bright, dark)
  {   0,   0,  6, 66,  7,  8,  0, 64, 64,  0, { ARGB(3,3,3), ARGB(2,2,2), ARGB(1,1,1) } },  // fuselage
  {  -4,   0,  7,  9, 62,  2,  0, 64, 64,  0, { ARGB(3,3,3), ARGB(2,2,2), ARGB(1,1,1) } },  // wings
  { -56,   0,  8,  7, 22,  2,  0, 64, 64,  0, { ARGB(3,0,0), ARGB(2,0,0), ARGB(1,0,0) } },  // tailplane
  { -58,   0, 10,  7,  2, 16,  0, 64, 64,  0, { ARGB(3,0,0), ARGB(2,0,0), ARGB(1,0,0) } },  // fin
  {  -4, -22,  3, 11,  4,  4,  0, 64, 64,  0, { ARGB(1,1,1), ARGB(1,1,1), ARGB(0,0,0) } },  // engines
  {  -4,  22,  3, 11,  4,  4,  0, 64, 64,  0, { ARGB(1,1,1), ARGB(1,1,1), ARGB(0,0,0) } },
  {  62,   0,  6,  6,  5,  5,  0, 40, 40,  0, { ARGB(0,0,2), ARGB(1,2,3), ARGB(0,1,2) } },  // nose, cockpit glass
};

