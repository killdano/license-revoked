#pragma once
// Player movement tuning. Shared by the kernel and the HANDS tenants.
#define TURN_MAX_FOOT   1900
#define TURN_MAX_CAR    1350
#define TURN_ACCEL       190
#define TURN_FRICTION    260

#define SPD_MAX_FOOT     14   // a proper run, not a jog
#define SPD_BACK_FOOT     8    // backing up is slower than walking forward
#define STRAFE_MAX       10    // sidestep, on foot only
#define SPD_MAX_CAR      30
// Boats: a jet ski is quick and skittish, a yacht is a barge.
#define SPD_MAX_BOAT     34
#define SPD_REV_BOAT     10
#define SPD_REV_CAR      13    // reverse gear
#define ACC_FOOT           3
#define ACC_CAR            1
#define DRAG_FOOT          2
#define DRAG_CAR           1

// One body on the ground is worth one star. Once you have shaken them, heat
// bleeds at about 12 a second: roughly 6 seconds per star, so a five-star
// spree takes half a minute of clean driving to lose.

#define HEAT_PER_STAR     80
#define HEAT_MAX         (HEAT_PER_STAR * 6 - 1)
#define HEAT_KILL         60
#define HEAT_STEAL        45
#define HEAT_COPKILL     170
#define HEAT_DECAY_MASK    3   // a star every twelve seconds, once they have lost you

#define GRAB_RANGE      100
#define GRAB_CAR_RANGE  130
#define ARREST_FRAMES    45
#define LANE       92           // lane centre, right of the centreline
