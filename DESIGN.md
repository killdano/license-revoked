# Where this is going

The 65,535-byte ceiling is on `text + data + bss` - everything the firmware
copies into RAM at launch. The image is 95% code, so the ceiling has always
been a ceiling on features: it is what cost the boats.

Domains lift it. A domain is a piece of the game compiled and linked on its
own, stored as a **resource** in flash (251 KB spare there), and copied into
an **arena** - a reserved block of RAM - when it is needed. The arena only
has to be as large as the biggest domain, not the sum of them. Everything
here rests on three things proven on real hardware before any of it was
written (`exec42/`, `ovl2/`, both kept in the repo):

1. the watch executes code from its own RAM - `bss`, heap and stack;
2. that code can call back into the app through a function pointer;
3. a blob loaded from a resource relocates and runs, and the same arena can
   be reloaded with a different domain.

## The arithmetic, which is the whole design

Break-even is one line: **the code you move out must exceed arena + loader.**

v4.0 moved only the interiors out and lost the bet:

    interiors removed          -2,180
    arena + loader             +3,217
                               -------
                               +1,037   worse

The arena is sized by the LARGEST domain, so two small domains barely help.
It pays from the third tenant on, because the arena does not grow. That is
the entire reason for the plan below - not novelty, arithmetic.

## Three arenas, one per axis of exclusivity

Three, because these vary independently: you can be at five stars anywhere.

- **PLACE** (~3 KB) - where you physically are. Club, store, hospital,
  seafood restaurant, the airport's four rooms, hangars, and each island's
  SCENERY (which building types exist, their palettes and props). Only one
  can ever be needed: inside a room the outside is not drawn, and you are on
  one island at a time. This is the arena with the most tenants and the best
  ratio.
- **TRADE** (~1 KB) - how you are earning. Taxi, ambulance, repossession.
  You are in one vehicle.
- **THREAT** (~3 KB) - how hard the police are trying. Air support
  (helicopter + SWAT), national guard, supers. Stages replace each other.
  On the secret island there are no police at all, so this arena is free -
  and holds the island's residents instead.

The taxi/SWAT rule the owner spotted: at one star the fare bails, TRADE
unloads, THREAT loads. Enforceable, not hopeful.

## The blocker - DONE

The code was not in modules, it was in three enormous functions - a third of
the image in three symbols, because every helper was inlined into them:

| | before | after |
|---|---|---|
| `render_frame` | 8,644 | 2,212 |
| `ent_update` | 6,100 | 636 |
| `game_step` | 3,364 | under 600 |

Two changes, no behaviour change: `WR_PART` (`__attribute__((noinline))`) on
the per-entity updates, the player's movement and input, and the HUD, so
functions that are called once stay addressable instead of being folded in;
and the terrain and road passes lifted out of `render_frame` into
`draw_terrain()` and `draw_roads()`. Cost: **80 bytes** for a codebase with
parts. Renderer cost unchanged (0.20 ms/frame on the host), full suite green,
150k fuzzed frames clean.

`update_heli` (924), `car_collisions` (1,384), `draw_lot` (1,706) and the
rest are now real, measurable, movable functions. That was the gate.

## Status

Done: the decomposition, and the **PLACE arena with three tenants** - city
scenery (3,312 bytes), the club (2,456) and the store (1,036), all living in
resources rather than the image. Verified pixel-identical against the
previous build over a four-frame drive, full suite green, 0.23 ms/frame.

    kernel, with all three outside the image     58,166
    ...plus the 3,328-byte arena and the loader  61,694

The arena is sized to the largest domain and **does not grow**. Coast,
industrial and ghetto scenery, the hospital, the restaurant and the
airport's four rooms are now free of image cost - which is the whole point
of the exercise and the thing that makes the island plan buildable.

## Lessons from the first overlay on hardware

- **The arena must be generated, never hand-written.** The SDK's gcc emitted
  a scenery blob 28 bytes larger than the one it was sized against; the
  loader refused it, `domain_use()` returned false, and every building
  silently vanished. The wscript now derives the arena from the blobs it has
  just built, and a failed load puts SCENERY FAILED TO LOAD on the screen.
- **A domain only draws what it owns.** Moving `draw_lot` into the scenery
  domain left the interiors with nobody to draw their walls, because the
  scenery is not resident indoors. Each interior draws its own now - which
  is better anyway: a wall is a box, not a building, and each venue colours
  its own.
- Loading is a few milliseconds. The banner was solving a problem that does
  not exist and has been removed.

## Order of work

1. **Decomposition.** Break the three functions apart, harness watching. No
   behaviour change.
2. **Boats and air support as domains.** The arena starts paying; boats come
   back after being cut since v3.6.
3. **Scenery domains.** Coast and industrial islands with their own building
   types (below).
4. **The airport and flight.**
5. **The secret island, the trades, the rescue.**

## The THREAT arena — v5.8, built

A second bench. Air support - the helicopter, the door gunner, SWAT on the
ropes, and the controls for the one you steal - is `dom_t_air.c`,
resident from four stars and while any aircraft exists. And the taxi is
`dom_t_taxi.c`, resident only at zero stars: a fare needs no heat and the
helicopter needs four, so the two can never be wanted together, which is
what makes the bench pay from day one. Between one and three stars it is
empty; that is where the squad tier and the national guard will go.

The loader is one function with two arenas (`load_into()`), each sized by
the wscript from its own group of blobs (`dom_*.c` and `dom_t_*.c`). On
the way, a real bug: the shim's context pointer `H` is a 4-byte static in
every domain, and the blob format was not carrying statics - it had been
landing in the arena's slack by luck. Blobs now carry `.data` and `.bss`,
zero-filled, so a domain may keep state.

## Islands

**Status:** the world is an island TABLE (`ISLANDS[]`, `island_at()`) and
a bridge TABLE (`BRIDGES[]`, `bridge_find()`), with a gap as a property of a
bridge. The ghetto is at (46,108), south of the city, joined by the
collapsed bridge: deck from t=420 to 720 along the centre line with no
deck from 556 to 600. In `cell_at_point()` the gap is water; the road
model and the painter split each bridge at its gap; pylons come from the
same loop for every bridge.

**The jump** (`move_player()`): a car entering the gap at JUMP_SPD (22) or
better leaves the deck for JUMP_FRAMES (36) on a parabola to JUMP_RISE
(140), with a shadow on the water below and the camera pulled out (+110 on
the zoom). It lands on whatever is under it at the end - the far stub or
the sea. Too slow at the lip and it drives off. Either way the water takes
the car: `car_drowns()` leaves it as a wreck in the gap, not burning, and
you swimming beside it. Wrecks accumulate. `tools/hostcheck/jump_test.c`
covers both outcomes.

**v5.4:** the ghetto has its own scenery domain (`dom_ghetto.c`): brick,
board and soot palettes, nothing taller than two storeys, no garages, no
glass. The jump moved into it, out of the kernel - the collapsed bridge's
lips are both on the ghetto's half of the line, so the domain is resident
whenever a car reaches either. Scenery follows the island you stand on
(`domain_scenery()`, every frame, a no-op unless the island changed).

Found on the way: the road painter's street mask and the ring road were
hard-coded to two islands, so the ghetto had markings on bare ground and
no tarmac. Both read the island table now.


| | city | coast | industrial | ghetto |
|---|---|---|---|---|
| trade | taxi, ambulance | boats | taxi | taxi |
| places | club, store, hospital | marinas, seafood restaurant | hangars, airport | club, store, airport |
| threat tops out at | helicopter + SWAT | marine unit | helicopter + SWAT | cop cars only |
| connected by | causeway | causeway | causeway | open water only |

**Seafood restaurant — v5.7, built.** `dom_diner.c`: on the coast, the gun
store's lot leads here instead - whitewash and blue, a cook behind the
counter, tables with diners, fish tanks where the cabinets were. A bill on
the counter is a meal, and a meal is twenty-five health, up to the top;
full up, the cook waves the money away. Trouble empties the place. 1,224
bytes in the PLACE arena: it cost the image nothing.

**Coast — v5.6, built.** `dom_coast.c`: pastel render with terracotta roofs,
villas one storey with a white plinth, a few hotel towers keeping their
height as the only tall things, and what the lot generator calls a parking
garage is a swimming pool here - a sunk blue rectangle in a low white wall
you can drive into. Same lot kinds, a different meaning per island: that
is the trick that makes an island's look cheap. 2,136 bytes, well under
the arena. Marinas and boats are still to come.

Villas - wide, low, pastel, flat-roofed; a `LOT_SOLID` with
different numbers. Swimming pools: a water-coloured ground quad inset in a
lot, which you can drive into and get stuck in. Bunkers reuse the sand cell.
A boardwalk from the existing jetty props. Hotel towers as the only tall
things, so the skyline differs from the city.

**Industrial.** The runway is a ROAD - the existing painter with a much
wider half-width, no pavement and different dash spacing. Hangars are the
parking garage with different proportions, so you can drive in. Shipping
containers: stacked boxes in rows, dense, destructible, good cover in a
chase. Fuel tanks are `gfx_taper` prisms. A control tower as the one tall
lot. Chain-link fences that channel a car chase.

## The airport — v5.9, built

`dom_airport.c`, one PLACE domain, four rooms replacing each other:
ticketing ($50 on the counter is a ticket; the back door needs one),
security (a bin for your guns; the guard at the gate refuses anyone still
armed), the terminal (seats, passengers, a 250-frame wait, NOW BOARDING),
the cabin (rows of seats, passengers, two pilots, the cockpit door). Sit
and the flight lands you at the other island's airport. Or beat the
cockpit door down, deal with the pilots, press the action at the controls
and the room drops away: you are flying the airliner. The airliner is a
helicopter entity with `sub == 1` - the air domain draws it (fuselage,
wings, red tail, engines) and flies it (cannot hover, climbs higher, holds
a cruising speed). The airport is what a garage lot IS on the islands that
have one - one in eight by a hash of its corner - on the ghetto and, for
now, the coast; the industrial estate when it exists. `airport_test.c`
walks the whole sequence, both endings.

Kernel side: the door, a `DOP_ACTION` hook so a room gets first refusal on
the action button, five captions, and one real bug: after `inside_update()`
the kernel clamped you back into the room, and the flight had just let you
out. Everything else is in the arenas.


Four small interiors in sequence, each mutually exclusive, each about a
kilobyte, all sharing the PLACE arena: ticketing (buy a ticket), security
(a bin to dump weapons in; carrying a gun stops you), the terminal (wait),
the cabin. Sit through the flight and it lands at the other airport. Or
punch through to the cockpit, press action, the interior drops and you are
flying the airliner over the world map - the same interior-to-world
transition the club door already does.

No special flight view and no level-of-detail system: the islands are far
enough apart that flying is just travelling. You cross open water and the
next island's scenery domain loads as you approach, exactly as it does when
you drive the causeway or take a boat. One rule instead of two systems.

**Ghetto island.** The second airport lives here, which is what makes a
flight worth taking. Its point of difference is that the violence is not
about you: armed civilians and police fight each other, and crime happens
whether you are there or not. That is a THREAT domain of its own and a small
one - it tops out at officers and patrol cars, no helicopter, no SWAT -
because an airport under a five-star air response would be unusable. Its
scenery: low blocks, boarded storefronts, chain-link, burnt-out shells of
the wreck model already in the game, fires in barrels reusing the fire
particles. A club and a store, but no hospital and no ambulance work.

The ambient fight needs one new behaviour and no new entity types: a
`criminal` role for a pedestrian that carries a weapon, shoots at police,
and is shot at by them. Police already chase and shoot a target; pointing
them at a criminal instead of the player is a target swap, not new AI. That
is the cheapest possible way to make a place feel alive, and it doubles as
cover: walk into a firefight and the police have something else to do.

**v5.5 — the militia.** Built as designed. A third of the ghetto's
pedestrians are born militia (`DOP_SPAWN`, in the ghetto's domain): 120 hp,
warpaint (dark brown across the skin - a mark a person chose), armed by
rarity - fists 15%, bat 20%, pistol 30%, shotgun 18%, machine gun 10%,
flamethrower 5%, rocket launcher 2%. Their behaviour is the domain's
(`DOP_PERSON`): the nearest officer within seven tiles is the enemy; melee
closes and swings, guns hold a distance, sidestep and fire, heavier
weapons slower. They ignore you until you hurt one of them
(`g_p.militia_hostile`), then you are a target too. They drop what they
carry. Police on the ghetto are always on the street - two officers'
worth, wanted or not - and an officer with a militia member in range
fights them, which is what he was doing before you turned up.

Kernel side, kept small: `ent_fire()` (one round from any weapon, for
anyone who is not the player), `ent_nearest_of()`, the faction bit on
bullets (militia fire hits police; police fire hits militia; both hit you
only when you have earned it), the two hooks. The ghetto's `draw_lot`
lost its window grids for a boarded band at street level, which is both
right for the place and what kept its blob under the arena.

## The secret island

No bridge; reachable by boat or plane, with a landing strip. No police,
which is what frees the THREAT arena for its residents.

A safe house with the protagonist's wife; the bedroom restores health (a
fade to black, not an animation). The population persists across deaths and
grows two ways: dancers rescued from the club - only from the club, after
the bouncers are down, carried through cars, boats and planes - and the
passengers of a hijacked airliner landed there, who arrive all at once. A
bonfire once there are enough of them.

The residents are people living at a resort: clothed, and doing resort
things. The mechanic is the rescue and the arrival, not a collection.

**v5.10 — the pause menu is a domain.** The world is not drawn while the
menu is up, so it takes the PLACE bench from whatever was resident (the
scenery, or the room you are in) and hands it back on close. Its drawing
and its rows moved out of the kernel; the SDK's drawing calls it needs go
through the context like everything else. About 700 bytes off the image,
and a proper black pause screen instead of a stale frame.

## The HANDS arena — v5.11, begun

A third bench: what your hands are doing. Tenants so far are the weapons -
melee (fists and the bat: the swing, fittings in reach, whoever is in
front), guns (pistol, shotgun, machine gun, with aim assist), the
flamethrower, the rocket launcher - loaded as you switch, a few
milliseconds under the pickup sound. Only the PLAYER'S trigger moved:
everyone else fires through the kernel's `ent_fire()`, and rounds in the
air are the kernel's too, so a militia rocket flies whatever you hold. The
kernel keeps the shared parts of the trigger (cooldown, the swing of the
arm, the noise it makes for the neighbours).

Honest accounting: the weapon tenants alone are a wash (about 440 out,
400 of arena in) - as predicted, the pay-off is the CAR tenant: boarding,
driving and getting out are about 2,100 bytes that would share this bench
and sit under a ~1,100 arena. That is next, and boats ride in behind it as
the tenant after.

Also v5.11: no arrests from the ground while you fly; interior walls hide
by where you FACE, not where the eye is - the eye lags and oscillates as
you turn, and deciding by it made walls flicker.

**v5.12, from the play report.** A fixed sun: the light was tied to the
camera, so every building changed colour as you turned. Wrecked patrol cars
no longer chase (a wreck's AI kept running). Junction paint needs a full
cross of road both near and far, or none - the ring road and the coast
were getting crosswalks on the beach. From the air, street furniture and
people beyond six tiles are not drawn: the pulled-back camera sees far more
city and the frame was over budget. And the trigger rule the owner
proposed: near a vehicle the press waits to see if it is a tap (attack)
or a hold (the door); anywhere else it fires at once and keeps firing on
a hold for the weapons that do. No rocket into the car you were boarding.

**v5.13, from the play report.** The crash: the club's bouncers called
`inside_exit()` from inside the club's own domain code, and `inside_exit`
loads the scenery into the same arena - overwriting the code still
running. The airport's hijack and landing had the same fault. Rule now: a
domain may never cause its own unloading. It ASKS (`inside_leave()` sets
`g_in.leave`) and the kernel leaves the room after the call has returned;
what a room wanted done on the way out (snap to a road; spawn the
airliner) is recorded in `g_in.after` and done by the kernel then. The
harness could not see this - the monolith never unloads - so it now has a
hook that aborts on any PLACE swap requested from inside a call, and a
test that runs the bouncer scene and both airport exits through it.

Also: flat room walls (one colour on every face, so nothing changes with
the angle; the store's lighter so the cabinets read); the hold works at a
door as well as the tap; a MAP in the pause menu - islands, bridges with
the gap, stores, clubs, airports, and you; one airport per island at a
fixed spot rather than by hash; terminals that look like terminals from
the street; and the venue walk widened to cover the ghetto.

## The foot/vehicle split — v5.14

The player's movement, boarding and driving are ONE source,
`src/domains/movement.inc`, compiled into different shapes: `DRIVING 0
NO_DOOR 1` into each weapon tenant (walking, swimming, steering on foot);
`DRIVING 1` into the car tenant (driving, the surfaces, crashes, the
collapsed bridge through the scenery's hook, getting out); `BOARD_ONLY 1`
into a boarding tenant that the kernel swaps in just for the hold that
gets you into a vehicle; `HELI 1` into the air domain for the aircraft's
steering. The compiler drops what cannot run in each shape, so no tenant
carries another's half, and the kernel carries none of it - it keeps the
dispatch and the helicopter's door.

The lesson, the third time: my first cut was a wash. Both halves carried
both doors (`in_car` is a runtime value), and the air domain took the
whole include for one function, so the bench grew by what the kernel
lost. Pruning to one door per shape and steering-only for the air brought
the bench to the car tenant's 1,700 and the kernel down 2,500. Measure
the blobs, not the plan.

Boats now slot in as a HANDS tenant under that bench for free.

**v5.15 — the arsenal.** The gun store sells everything, seven tiers by
cumulative credit: pistol 40, shotgun 100, vest 120, grenades 150, MG 180,
flamethrower 250, rocket launcher 400 - and everything paid for is on the
counter on every visit (`restock()` on entry as well as on each throw).
Grenades are back as a HANDS tenant: a lob on a 28-frame fuse that slows
as it lands, bounces off walls, and goes off wherever it is; six per
pickup. A gun that runs dry clears its stash and leaves your hands and the
cycle. Bullets are drawn ALONG their flight (they were at yaw zero - a dot
from most angles), rockets fat and grey with a flame behind, grenades a
ball on a shadow. Kernel cost: the grenade's fuse in `update_bullets()` and
two table entries, about 430 bytes.

**v5.16.** Real room walls: one colour on every face, a dark skirting, a
row of windows with a light strip each, distinct from the floor; a wall
between the camera and you is STIPPLED like the city's towers, not hidden
(hiding it let you see out of the room). The bridge to the ghetto now runs
from the city's ring road to the ghetto's, with the break at the ghetto
end - the streets lead onto it, and the gap is the last thing before the
far shore; jumpable from either side. The pause menu hushes every voice.
Touch is coalesced per the MirrorMov letter: the handler records the
newest position, the frame tick applies it once; touchdown and liftoff
pass straight through, with a flush before liftoff.

The ghetto: two new hooks let an island's scenery own its ground and
street furniture. `DOP_PROP` (what stands on this tile; kinds from
PROP_DOMAIN0 up are the domain's, solid and permanent) and `DOP_DRAW_PROP`;
`g_park_col` / `g_lot_col` set on the scenery's `DOP_ENTER`. The ghetto:
parks and block ground the colour of dirt, oil-drum fires with flickering
flames, rubbish piles, and - drawn with the buildings so they are as dense
as the buildings, which the 42-slot prop pool could never be - burnt-out
cars on blocks and drum fires at the kerb in front of buildings, and one
building in eight on fire with a smoke box drifting over it. Kept under
the city's blob size, so the PLACE bench did not grow.

Note: WR_GOD=True currently REMOVES about 200 bytes (the damage and
arrest paths compile out). Turning it off for release costs that back.

**v5.22 — mirror mode pass.** The radar dots (`hud_ring`) were measured
from the character's heading, but the camera is yawed off him by
CAM_YAW_OFF - the other way in mirror mode - so the dots sat 25 degrees
wrong and swung 50 when mirrored. Fixed on both platforms. The weapon box
was right on the watch in both modes (`mirror_test.c` proves it); on the
phone its taps were being routed to the turning pad when the box moved to
the right - the phone now sends the box's taps to the game on either side.
The phone's HUD draws in the watch's logical space, and framebuffer-space
values (the projection centre, the stick) are converted with L() on the way
in.

**v5.25 — the airport, properly.** The honest answer to "can the hardware
do it" was yes: the engine draws boxes and frustums at any size, and an
interior has the whole bench. Four rooms with real dressing, as tables
(13 bytes a part) drawn with the walls: a lobby (desk, departures board,
queue posts, a queue); security (walk-through arch with its green light,
x-ray belt and tunnel, agents, the bin for weapons, a line of people
between posts); a terminal (rows of joined seats, a window wall onto the
sky, the apron and the airliner at the gate, mullions, a gate agent); and
a FUSELAGE - 1.5 tiles wide by 6 long - with rows of seats either side of
the aisle, overhead bins, a line of windows onto sky, clouds drifting past
outside (the ground outside the walls becomes sky), and a cockpit behind
its door: a sloped console with instruments and two pilots' seats. Sit by
tapping anywhere along the rows.

Too big for one tenant (4,016), so it is two from one source
(`airport.inc`): landside (lobby, security) and airside (terminal, cabin).
The security gate hands over with `g_in.after == 3`, which the kernel
honours after the call returns - the same rule as leaving a room.

Rooms' windows everywhere: tall, framed, one every 300 units - two or
three to a wall, not a row of holes. No arrests at full health in a car.
The phone's venue marker drew in framebuffer space; L() fixes it.

**v5.26 — the rest of the list.** Knocked flying: a car at speed whose
box you are in throws you over the bonnet - its speed, up, and down again,
bouncing on the tarmac (each bounce a third of the last: a half hops
forever), blood at every landing, damage the speed deserves, nothing you
press matters until you have stopped; the police's own ramming code now
only shoves at a crawl and leaves speed to this. Cars are solid on foot at
their model's real size (a police car is bigger than a sedan; it used to
be a sedan's box for everything). The club's patrons throw their money
onto the stage, and cash going up while you are on or beside the stage is
the bouncers' cue. The arrest is the THREAT bench's third tenant, at one
to three stars: no one cuffs you at zero, and at four they have stopped
trying to take you alive. `knock_test.c` covers all of it.

Measured, again: the knockdown's physics in the kernel cost more than four
copies in the foot tenants save (the bench is set by the largest tenant,
and it went back under the car's size once the shared box test was
shared). The blobs decide, not the plan.

**v5.27.** Blowing up a car is two bodies' worth of heat (blame follows
the player's rounds and blasts to the write-off; explosions are always
noticed). Death halves what you had paid the gun stores, as it halves your
cash. No nightstick at zero stars. The bat kills in one and dents cars
(bodywork off, wreck it in six). The AMBULANCE is back - restored from
commit 9de498e - as `ambulance.inc` compiled into the taxi tenant (zero
stars) and the arrest tenant (one to three): it turns out for a settled
body, holds over it with the lights going, takes it away, and can be
stolen; from four stars there is no tenant for it and the kernel sends it
home. One patrol car in three rams; the rest pull up and put a shotgun on
the street. Two seconds' grace after a knockdown, so they cannot stun-lock
you. Kill four officers inside eight seconds and they pull back - CALLING
FOR REINFORCEMENTS, no dispatch for twelve seconds - and come back a star
harder. The helicopter's flight controls moved from the air domain to a
HANDS tenant (`dom_h_fly`), which shrank the THREAT bench by 880.

Not built: the tiers above air support - the national guard (armour, a
tank, a jet) and the supers. They are THREAT tenants to write; the bench
is the same one, and with the flight controls gone it has room.

**v5.28 — the paint scan.** Centre-line dashes were ending on top of
crosswalks: the rule that keeps a dash off a crossing road gave the
crossing road a 30-unit margin past its kerb, and its stop line and zebra
run to about 300. Worse, `road_here()` stops reporting a road 470 units
from its centreline - exactly the band the zebra is in - so the rule could
not see the crossing at all there. `road_paint_ok()` now measures both
diagonal families directly and keeps dashes 300 clear of a crossing's
kerb. `paint_test.c` scans every junction in the world (10,210 samples)
and asserts no dash position inside any crossing's box.

**v5.30 — the national guard, and the tier above.** At five stars with six
officers dead, air support stands down (`want_heli` returns 0 at tier 2)
and the guard comes: one tank (a police car with the CAR_TANK model, so
crashes know it - it crushes and does not feel it - and the car tenant can
drive it), jeeps, and soldiers in olive with MGs and 200 hp (PA_GUARD).
The guard is a THREAT tenant (`dom_t_guard`, 740 bytes): it turns the
dispatcher's units into its own on DOP_UNIT, runs the tank's frame (drives
at you, turret tracks you slowly, a shell every two seconds from eight
tiles - a rocket with the police bit), and draws the tank whole. Steal it:
military armour (ARMOR_MIL 250, a green bar), the aircraft's weapon cycle
with MG and SHELL (the car tenant's DOP_ATTACK), slow. A tank on the street
keeps the guard resident, so a stolen one draws and drives after the heat
is gone. The tier latches until the heat clears.

DOP_SPAWN on the THREAT bench already meant "spawn the hijacked airliner";
the dispatcher hook is DOP_UNIT. Caught by the air test.

Paying for it: the tank's guns and drawing live in tenants; the HUD's
tenant-specific marks (PAID, the club meter, the arrest bar, the helicopter
and fare markers) are drawn by the tenants that own them (DOP_DRAW_HUD, on
both benches); `road_paint_ok` is a fifth of its size because `road_here`
now reports a road out to its crosswalk's far edge (ROAD_HALF + 300); and
the death cinematic and respawn are a HANDS tenant (`dom_h_dead`) - your
hands do nothing while you are dead, and the first spawn is a respawn.
Moving the junction paint into the scenery tenants was tried and reverted:
it cost 476 in a tenant against 412 in the kernel, and the bench grows by
the tenant. The blobs decide.

The owner built it: 65,562, 27 over - the host-to-watch ratio is 1.0527,
not 1.0509. v5.31 pays: room windows are one part each (a pale pane with
dark edges) instead of frame plus pane, which took the club's blob from
3,480 to 3,386 and the PLACE bench with it; the club's meter is at a fixed
x; three captions are shorter; MAX_GIB 24 to 22. Host 62,128, which the
real ratio puts about 130 under. `tools/monocheck.sh` compiles every
source as one image with implicit declarations as errors, because the
Android build (clang) does and the host build did not.

**v5.32 — the guard, for real.** The owner killed a million cops and never
saw the military: the reinforcements rule reset the kill count the guard
was waiting for; the tank's own shells killing officers counted as the
owner's kills and started lulls; the turret's angle lived in `phase`,
which is a car's wheel spin; the dispatcher counted dead officers against
its quota (at five stars no ambulance clears them); and nothing converted
the units already on the street when the guard arrived. Now: the
reinforcements rule has its own window counter (kwin); police fire that
kills police counts for nothing (g_cop_fire); the turret is in `anim`;
only the living count; a THREAT tenant gets DOP_ENTER when it becomes
resident and the guard takes over the street on it; the military ignores
the police's lull; bodies are evictable at any distance. `guard_test.c`
kills officers one at a time with the helicopter overhead, through the
lulls, and gets the tank. The old test set the counter directly, which is
exactly the instrument trap in HANDOFF.md.

Paid for by moving the hijacked airliner - its model, its spawn, the
drawing of whatever aircraft you are in - from the air domain to the
flying HANDS tenant (`aircraft.inc` holds the two models, included by
both): air support 2,076 to 1,840. Host 62,027.

1. `WR_LTO` in the wscript (on): link-time optimisation, measured at 628
   bytes on a real link. It recovers most of what splitting the sources
   into domains cost in cross-file inlining.
2. Pools at their measured peaks (`MAX_ENT` 42, `MAX_PROP` 42, `MAX_GIB`
   24, draw list 126, `MAX_BLOOD` 10).
3. `-DWR_TAXI=0`, about 700 - the last resort, and it loses a feature.

The owner's SDK gcc emits about 6% more code than the toolchain the size
tool uses, and the ratio drifts upward as the code changes. Always leave
a few hundred bytes of estimated margin, not tens.

## Rules for anything built this way

- A domain may not name a host symbol. `tools/mkdomain.py` fails the build
  if one leaks in, rather than shipping something that faults on the watch.
  It caught three on the first attempt.
- Everything a domain creates lives in the entity and prop tables, which
  `ent_reset()` clears before `domain_drop()`. Nothing may hold a pointer
  into an arena that is about to be reused.
- Anything that outlives the room stays in the kernel: money, the wanted
  level, the island population. A domain cannot reach the wanted level, and
  should not.
- `-DWR_MONOLITH=1` compiles every domain in statically. The host harness
  uses it, so the tests never stop working, and it is the fallback if a
  build has to ship without resources.
