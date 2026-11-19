# License Revoked

A GTA 1-style open-world game for Pebble Time 2 (emery). Flat-shaded polygon
3D, over-the-shoulder camera, no missions — drive, fight, attract police, lose
them, and now: earn money, spend it in a gun store or a strip club, or drive a
cab for it.

Watch app only. No companion app, no AppMessage, no network.

> The sections below headed **September 2026** describe the game as it is.
> Everything after "Controls" is the original engineering log, kept because
> the bugs it records will bite again; where it contradicts the new sections,
> the new sections win. Retired features (the ambulance, cyclists, seafront
> parking bays, bodies dragged by cars, wall murals) are noted where they
> come up.

## September 2026: what changed, and why

The owner's list was: the graphics were flat and the player's arms ran
backwards; the roads formed nonsense; the audio was chirps; the helicopter
and SWAT were never seen; traffic was dumb and the police shot too hard;
then money, a strip club, a real gun store, and a taxi. Every item is in.
The cost was the 64 KB ceiling, paid for with a size pass and by retiring
the features nobody asked for. The numbers are at the end of this section.

### Tools that made it possible

- `tools/hostcheck/snap.c` renders real frames headless to PPM. Every
  graphics decision below was looked at, not guessed. `snap <out> street`,
  `cars`, `carslow`, `boats`, `helipark`, `heli`, `club`, `store`, `at X Y ANG ZOOM`.
- `tools/hostcheck/worldmap.c` draws the classifier top-down with real
  junctions in red and dead lattice crossings in magenta.
- `tools/hostcheck/traffic_trace.c` traces every vehicle's path over the
  map for a minute of play. Spirals and beach excursions are obvious in it.
- `tools/hostcheck/sounddump.c` renders every sound effect and each
  continuous generator to WAV (`sounds/` in the zip). Listen before
  flashing; the spectrograms were checked but nobody here has ears.
- `-DWR_TRACE` prints one vehicle's driver state per frame
  (`wr_trace_ent`).
- An ARM Thumb-2 toolchain measures real code size per function
  (`arm-none-eabi-size`, `nm --size-sort`). The SDK's older GCC produces
  about 9% more; treat host numbers as relative.

### Roads: one description, read by everyone

Three road kinds — the two diagonal street families, the coastal ring and
the causeway — used to be three unrelated tests in `cell_at_point()`. The
renderer, the junction markings and the traffic AI each re-derived roads
their own way, so they disagreed: the causeway's kerb was painted across
every street it crossed, street pavements were painted over the causeway's
tarmac, and `city_junctions()` marked every lattice crossing that was road
for *any* reason — including crossings buried under the ring or the
causeway — which is where the crosswalks on straight roads came from.

Now there is `road_here(x, y, Road *out)`, which returns every road under a
point (up to four: causeway, ring, one street of each family) with its
kind, canonical heading, signed lateral offset, half-width and pavement
reach. Classification is "any carriageway beats any pavement". Markings are
painted only where a point is on exactly one carriageway (`road_alone()`),
so lane dashes stop at every crossing of any kind. A junction is real only
if the crossing itself is diagonal road and at least three arms lead
somewhere (`city_junction_arms()`), and stop bars and crosswalks are drawn
only on arms that exist. Traffic signals therefore only stand at real
crossings.

Two bugs in the coast were fixed on the way. The wobble's pseudo-angle
covered half a turn and joined quadrants three and four the wrong way
round, which produced the spike at the top of each island and the "bite"
out of island B; and it rounded to whole tiles, which staircased the
shore. The coastline is smooth now and the classifier works in subunits.
The causeway runs from ring road to ring road (`BRIDGE_T0`/`T1` re-probed
against the new shore), so both ends are T-junctions instead of a highway
bulldozed into the grid.

`road_point()` steps along a road from a point: straight for streets and
the causeway, along the wobbled arc for the ring. The ring is not locally a
circle — the wobble can shift its radius two tiles over a few degrees — and
every straight-line probe along its "tangent" left the tarmac on a bend.

### Traffic: one driver

`drive_along()` in `wr_ent.c` is the only driver. Traffic, police cars and
anything else with wheels use it. It stores no route and knows no graph:
each frame it asks `road_at()` which road it is on (the best-aligned of the
roads under it, which is what keeps a car crossing the causeway on its own
street), holds the right-hand lane by aiming at a point a little way ahead
in that lane, and when a crossing comes up it decides once — straight,
left, right — aims at a point in the exit lane and lets the same
rate-limited steering carry it round. A road that ends is a crossing with
nowhere to go straight; no exits at all means a U-turn, which is a turn
toward the reversed heading at walking pace, so the arc fits in the
carriageway. Seekers (police) take the exit that ends nearest their
target. Cars queue behind slower cars in their lane, stop at red lights,
and a car stationary for more than a light cycle turns round; one still
stationary long after that goes back to the pool.

Bugs found by the tracer, all real: the lane sign was inverted (cars drove
on the left and met head on); two stopped oncoming cars treated each other
as obstacles forever; ring-road probes stepped in straight lines; spawns
on the ring snapped to a diagonal street's extension, onto the beach;
`road_here()` can return four roads and every array was sized for three;
the cruise-speed table had seven entries for nine car models (the bus and
the cybertruck read past the end); and the old stall counter lived in
`anim`, which `ent_update()` decrements every frame, so "wedged car"
recovery could never fire. It lives in `spin` now, which only a wreck uses.

The `systems` harness's "traffic keeps moving" assertion — the one that
never responded to any change — counted **parked cars in garage bays** as
stalled traffic. The measurement was the bug.

#### Where traffic is

Traffic that drives spreads itself over the whole despawn radius, so
thirteen cars looked like one. Cars now spawn three to eight tiles out in
the half of the world the camera looks at, are culled at half distance
once they are well behind you, and there are more of them (19 at no
stars) and fewer walkers (12). Traffic also brakes for the player on foot,
as it does for a car ahead — spawning on a road used to be a hit-and-run.

(v3.1 also fixed a build error introduced in v3: an `#endif` for the boat
option had landed after the car draw branch, so with boats off the cars
were driving around invisible. That is what "all the cars are gone" was.)

#### v3.2, from the first play report

- Running someone over has its own sound now: a wet crunch, a low thud, a
  snap and a cry cut short (`SFX_RUNOVER`). The screams and death cries
  fall in pitch instead of rising; the rising glide read as a slide
  whistle.
- Civilians never spawn inside the frame (`gfx_in_view()` against the last
  rendered frame); the cap is 15 cars, and they spawn 5–10 tiles ahead.
  The helicopter comes in from within 20 degrees of straight ahead and
  only lands once it has actually held station, so you see it hunt first.
- The ring road's dashes are snapped to the quad grid; walked from the
  player's own angle they slid round the ring with you.
- Sand tops a car out at 60% (grass 75%), and the throttle no longer
  slams you down to the limit: only rolling resistance does, over about a
  second, so speed carried onto the beach is spent, not chopped.
- Motorcycles: hit one and the rider comes off over your bonnet, the bike
  is flung along your heading and slides, and you lose an eighth of your
  speed instead of stopping. Hit the sliding bike again and it takes
  damage; the fourth knock sets it alight for the explosion.
- The helicopter could never spawn once a policeman took its reserved
  slot; nobody else can have that slot now. Cone and parking-meter props
  were still compiled in with nothing placing them; removed.

#### v3.3, from the second play report

- **Police escalate; they do not open with it.** Until you use a gun or
  kill an officer (`g_p.hostile`), nobody shoots and nobody rams: a patrol
  car pulls up two and a half tiles short, an officer gets out, and the
  arrest is made on foot - pulled out of the car and cuffed, as before.
  Air support needs four stars, guns out, two officers down and thirty
  seconds of it. SWAT fire slower (one round in 18 frames, damage 5) and
  the whole force fires one round every 12 frames.
- **Losing them is harder.** Ten seconds out of sight (was 1.6) before the
  heat starts to bleed, a star every twelve seconds (was six), and they
  keep eyes on you from seven tiles (was five). The garage swap is the
  fast way out again.
- **A car you get out of stays.** The driver's junction scratch was being
  read as the park anchor, so the car pinned itself to garbage
  coordinates - "it disappears". Patrol cars honour `ST_PARK` too; one
  you left used to resume the chase.
- The helicopter you steal is drawn as a helicopter (the player's-vehicle
  branch drew a car). The ring road's dash pattern is anchored to the
  world, not to your angle. Taxis have a roof light and are the only
  yellow car (the school bus is orange now, the jeep army green).
- Money is 18pt white on black. Entering the store puts the price list
  on screen: pistol $60, shotgun $150, flamethrower $350, rockets $600;
  ten dollars a throw, counted anywhere in the room; a cabinet opens when
  what you have thrown reaches its price.
- Clubs and stores anywhere on the map show as pointers (magenta and
  orange) on the ring around you; clubs have a magenta sign with a
  blinking neon band. There are four clubs and five stores.
- Cut for room: the death-screen stats line, the causeway barriers, the
  arrest tableau. (The Mirror option stays; the owner was clear.) The sine table is built
  in the heap at startup (Bhaskara's formula, within 2/1024), which takes
  512 bytes out of the image.

#### v3.4/v3.6: what the linker map actually said

The owner sent `pebble-app.map`. First reading: 10 KB of libc in the
image. Second, correct reading: those sections are in the map's
**discarded** list - gc-sections dropped them - and the image held only
~900 bytes of libgcc's 64-bit division plus a 600-byte SDK shim. So the
real saving was the division (every `int64 / int32` is now `wr_muldiv()`,
a 32-step shift-and-subtract), not the 6.5 KB claimed for a round, and
boats went back off after one over-the-ceiling build. **Count only the
kept sections.** Boats remain `-DWR_BOATS=1`, about 2.6 KB, and do not fit
alongside the club, the store and the taxi.

#### v3.5, from the third play report

- In a car, the car takes the damage: police rounds go into the bodywork,
  a blast hits the car, and you are hurt only when it goes up around you.
  Health is 150. Police aim has fifteen degrees of nerves in it, the door
  gunner more; patrol cars turn on a wide radius when they leave the lane
  to ram, and rest for a couple of seconds after each hit. Pinned in a car
  and barely moving, an officer in reach hauls you out and that is the
  arrest - no bar to fill on foot.
- Heat is only added if an officer within eight tiles saw it; otherwise
  one in three chance somebody phoned it in, for half. A kill is 60 heat
  (two unseen kills for a star). Killing an officer is always known.
- A police car comes with a pistol or a shotgun in the rack.
- You come back with half your money, on either island. The siren drops
  to half volume after ten seconds. The cash readout is plain white text.
- Getting into any taxi says so; fare and destination markers are bigger,
  outlined, and shown on the ring when off screen.
- Garages: 24% of lots (was 18%), each stocked with at least two cars,
  nearest garage first. Ten parked cars in the pool.
- The dancer gets on the pole: a tight fast spin, riding up and sliding
  down, half the time. Bought guns appear on the counter in front of the
  clerk with a caption saying so. Interiors: camera much closer. (v3.8:
  the front wall is full height again - it is simply not drawn while the
  camera is on its outer side, so it never blocks the door as you walk in
  and it hides the sea when you turn round.)
- The zoom slider runs past the old far stop (`ZOOM_MAX` 380: the camera
  keeps rising and backing off, the pitch stops steepening at 256). The
  menu lost the Resume row (BACK does it); UP is louder.
- Money gibs are never recycled out of a full particle pool: a bouncing
  ragdoll's blood used to overwrite bills before they landed.

#### v3.6

- The cash readout sat on top of the weapon name (both top-left); it is
  top centre now, with the store's PAID line under it. The weapon-name
  table had no entry for the money gun, so indoors it read whatever
  followed the table.
- Club music is an original slow piece in A minor at 70 bpm - a two-voice
  pad under a plucked arpeggio and a long falling line - in the register
  the owner asked for. (A specific published track cannot be reproduced.)

#### v3.9, from the fourth play report

- Parked cars are empty: the state was flipped to "driving" before the
  haul-out check, so getting back into your own car threw a driver out.
  Cars are solid on foot (a real footprint box). SELECT: a tap is the
  action, a hold of a third of a second is the car door - take or leave a
  vehicle - and a hold with nothing in reach is the trigger. So you can
  stand beside a car and hit it with a bat.
- Guns are not lying about any more: world pickups are bats and knives,
  sparse. Pistols and shotguns come off police, machine guns off SWAT,
  everything over the counter. Grenades went with that (unobtainable) and
  the player-knockdown ragdoll and lifetime stats went for room.
- Crashes: momentum goes mostly to the OTHER car, three times the spin,
  more debris, hit box 172. Your car takes a fifth of crash damage, half
  the ram damage, and street furniture only dents it. Explosions reach
  300, a car touching the one that went up goes with it (own fuse), a
  car-length out takes 90. A motorcycle is only knocked over by you or a
  patrol car - traffic was throwing riders across the city on its own.
- Police are noticed: half of unseen crimes are called in at full heat, a
  shot within earshot of an officer is 25, a car you blow up is 45 (yours,
  not the police wrecking each other - the cover test caught that).
- Taxi pays double, the passenger gets out at the kerb, fares get in from
  260 units at up to speed 5. Venue pointers show while driving.
- Guns 40/100/250/400. Club levels at 30/70/110/160; a lap dance heals to
  full and gives a full vest. On foot 12.
- HUD text is white with a black stroke everywhere; cash under the ammo
  pips. Garage bay markings are painted with the ground (as part of the
  lot they were drawn over parked cars). A pedestrian paints the far arm,
  torso, then the near arm, by depth either side of him. Indoors the world
  is floor in every direction and no sky is drawn. The cybertruck is
  brushed steel with a blue cast, not road grey.
- Club music is a pop track: 110 bpm, C-G-Am-F, kick every beat, snare on
  two and four, hats, a plucked bass, an arpeggio and a two-bar hook.
- v3.10: solid cars trapped you inside your own wreck - thrown clear "in
  place" meant standing inside the footprint with every step blocked.
  A car's box only blocks stepping INTO it, and a wreck throws you clear
  to the side.

#### v3.11–v3.13: the part sort, tried and reverted

v3.11 sorted every model's parts far-to-near before painting them, which
fixed a walker's far arm showing through his body and a car's far wheels
through its own. It cost 1.3 KB (a 12-part queue plus code) and about 40%
of render time, and the room came from the club's patron scenes, the
store's browsing customers, two sounds and the pools. The owner judged
it not worth what it cost, and v3.13 reverts it: the models paint in
listed order again (arms are still ordered by depth from v3.9), and the
patrons, customers, sounds and pool sizes are back. v3.12's cash piles
stay: a bill that lands when the ground pool is full joins the nearest
pile instead of vanishing.

What no version fixes: two OBJECTS close together are sorted by their
centres, so a man on the far side of a car can be painted over it. That
is the painter's algorithm; the alternative is a 45 KB z-buffer.

#### v3.14, from the fifth play report

- Helicopter controls redone: the weapon box cycles MG, ROCKET, LAND. A
  tap fires, a hold streams the chain gun, LAND brings it down wherever
  it is, and on the ground the hold gets you out. It never lands by
  itself. Taking it gives you a vest and one of the good guns.
- The pilot bug: the state was flipped to "driving" before the
  was-it-parked check, so every entry threw a pilot out. Out of a police
  vehicle now comes an officer (armed, into the chase), not a bystander.
- Taxi: a fare you run over stops being a fare (the yellow marker was
  following the body). Every ring dot has a black stroke.
- The store remembers what you have paid this life, per venue; anything
  paid for is on the counter every visit; only a cabinet you smashed is
  gone for good.
- Above hostile, a patrol car pulls up short and an officer gets out with
  a shotgun as well as ramming; below hostile it is the arrest as before.
  A car you wreck is a full star when noticed, and unseen crimes are
  called in two times in three (was one in two) - sporadic crimes were
  bleeding away between events.
- Street lights (70 hp) and lamp posts (50) can be knocked down; the
  shear-off physics was already there. Club patrons, store customers and
  the clerk run for the corner when hit. Parked cars in garages are cars
  (never a bike or a bus). Cash on the ground is a taller, blinking stack
  and takes a bat's slot before joining a distant pile.
- Cut for room, at the owner's suggestion: the knife (a bat does the
  job), the school bus's trim, four gib slots.
- Unresolved: a report of a garage car that would not drive, with no
  engine sound. Could not reproduce; a screenshot would help.

#### v4.0/v4.1: domains, and why they are switched off

The mechanism works and is proven on hardware. The accounting did not, yet.

A domain is a piece of the game compiled and linked on its own, stored as a
resource in flash, and copied into an ARENA - a reserved block of RAM - when
it is needed. The arena only has to be as big as the largest domain, not the
sum of them. `exec42/` and `ovl2/` proved on a real Time 2 that the watch
executes code from its own RAM, that such code can call back into the app,
and that a resource-borne blob relocates, runs, and can be swapped.

v4.0 made the club and the store domains, and the image got BIGGER:

    interiors removed from the image      -2,180
    arena (2,528) + loader                +3,217
                                          -------
                                          +1,037

The arena is sized by the LARGEST domain, so two small domains barely help;
worse, splitting one file into three cost about 600 bytes of cross-file
inlining that `-Os` had been doing. Break-even is one line - the code moved
out must exceed arena + loader - and it is not reached until boats and air
support are domains too, at which point the arena stops growing and every
domain after is nearly free.

So `WR_DOMAINS` is **off** in the wscript, the game builds and plays exactly
as 3.14 did, and the machinery is kept intact in `src/domains/`
(`wr_domain.c`, `wr_shim.h`, `dom_club.c`, `dom_store.c`,
`kernel_inside.c.next`) for the next phase. `tools/mkdomain.py` builds a
domain blob and, importantly, FAILS the build if a domain references a host
symbol rather than shipping something that would fault - it caught three on
the first attempt.

The blocker for the next phase is that the code is not in modules but in
three enormous functions - `render_frame` 8,644, `ent_update` 6,100,
`game_step` 3,364, a third of the image - because every helper is inlined
into them. `update_heli` is inside `ent_update`; `draw_lot` is inside
`render_frame`. They have to be decomposed before anything can be lifted
out. See DESIGN.md for the full plan, the three-arena design, and the
island layout it makes possible.

### Police
### Police

One police round every nine frames, whoever fires it, at most two shooters,
reduced damage (pistol 10, shotgun 14, MG 7), shooting from three stars.
`MAX_SHOOTERS` existed before but nothing checked it. Patrol cars follow
the roads toward where you were last seen and only leave the lane to ram
when they are within three tiles with a clear line.

### The helicopter

It holds station two tiles *ahead* of you at rooftop height (320), because
the camera looks over your shoulder and overhead is the one place you can
never see; the camera pulls out while it is up; it has a ground shadow and
a blinking marker, with a chevron at the edge of the frame when it is off
screen. Every few seconds, with no SWAT on the ground, it lands on the
nearest clear stretch in front of you and two SWAT step out of the doors;
it sits for about six seconds with the rotor turning, and in that window
you can take it. Flight: the stick is throttle, UP/DOWN turn, any input
climbs, two seconds hands-off settles it onto whatever is below. SELECT
fires — a chain gun on hold, six rockets one per press, swap by tapping the
weapon box — and SELECT on the ground, stopped, gets you out. Ground fire
cannot reach you aloft; a helicopter destroyed in flight kills you; patrol
cars cannot ram an aircraft (they did).

Bugs: the helicopter gave up before it arrived, because its "lost him"
timer started at spawn nine tiles out; at five stars the entity table was
over-committed (55 wanted, 48 slots), so nothing could respawn — civilians
now thin out as the heat rises and police may evict the most distant
walker or corpse when they need a slot; explosions killed *any* non-car
entity in range, including a helicopter or a boat.

### Graphics

Every side face used to get its colour from its **index** — front and left
bright, back and right dark — regardless of where it pointed, which is why
a box lit from any angle read as two flat planes and a cap. `gfx_shade()`
picks the colour from the angle between the face's normal and one light
that rides the camera (from behind the eye, off to the left), so the two
faces you can see are always shaded differently. Four steps: the two given
colours, one lifted a notch (the dominant channel, so navy gets bluer, not
greyer) and one dropped. Buildings use it too.

The player had four arms. `player_detail()` drew a second pair at ±19 with
the swing sign flipped relative to the base rig's pair at ±21, so one pair
always ran backwards, plus a second torso over the first. It is gone; one
rig serves everyone, with tapered legs and arms, shoes and hands. Cars have
wheels as tall as they are round with the disc on the visible flank, a
glass cabin on every side, and no "sill sticks". The police helicopter is
white with a blue band and a glass nose.

`gfx_part()` takes sixteen arguments and every call site cost 40–60 bytes.
Static parts now live in `PartRec` tables (13 bytes each, `PART()`/`BOX()`
macros take full-unit numbers and halve them at compile time, a colour
below 0x10 indexes a palette passed at draw time) drawn by `gfx_parts()`.
Props, the helicopter, the boats, the motorbike, car trim, the arrested and
dead poses, the store sign and the wreck's fire are all tables.

### Audio, from scratch

`wr_audio.c` was rewritten. Two ideas: continuous things are continuous —
the engine, the siren, the rotor, the flamethrower and the club music are
stateful generators that run every sample for as long as their source
exists, nothing is retriggered — and every one-shot goes through a low-pass
whose cutoff decays with the amplitude, so a bang is bright for a few
milliseconds and dull for the rest of its life. Amplitude and cutoff decay
every eight samples with 12-bit amplitude; an 8-bit per-sample multiplier
cannot express anything slower than a 30 ms tail, which is exactly what the
first attempt sounded like. The engine is a four-stroke pulse train
low-passed into a putter at idle and a buzz at speed; the siren a triangle
wailing 650–1300 Hz over 1.6 s; the rotor noise chopped at the blade-pass
rate with a turbine whine; the music four-on-the-floor at 128 bpm with a
pitch-dropping kick, a hat, an eight-note bass loop and a stab every other
bar. Recipes are eight-byte rows in `SFX_TAB`. `sounds/` has all of it as
WAV.

### Money, and where to spend it

Cash comes off bodies the way blood does: a burst of bills that arc, bounce,
skid and come to rest as pickups around the corpse (gibs tagged `GIB_CASH`
become `PICKUP_CASH` when they stop). Half of civilians carry nothing, a
third a little, a few a lot; police always carry some. Walking over a bill
adds to the `$` on the HUD. Indoors the money gun (`W_CASH`) comes out as
you walk through the door, throws $10 a press or a stream on hold, cycles
like any weapon, and does not exist outside.

**The strip club** (`LOT_CLUB`, about 5% of lots, `wr_inside.c`): a 6×5
tile room with a pink stage, pole, spinning mirror ball, three
colour-chasing pools of light on the floor, a bar, five chairs with
patrons, a dancer, and two black-suited bouncers by the door. Money thrown
within two and a half tiles of the stage feeds a meter (0–400) that runs
down by itself. Five levels: the dance; top off, clothes flying off as
debris; everything off; she comes over and you are put in the chair with
the stick doing nothing; and closer. It lasts as long as the meter does —
more money, longer. More money also brings more dancers out (up to three).
Patrons walk up and throw bills of their own, which can only ever take the
show to level one and never tops up yours, and now and then a dancer gives
one of them a bottoms-on lap dance. Fire a gun, swing anything, kill
anyone or step on the stage: the dancers hide in the corner, the bouncers
walk you to the door one shove at a time, and you are barred from that
venue for the rest of the run (they remember at the door). Hit a bouncer
and the two of them fight you instead. Two minutes without paying and
there is a small chance each second of being walked out. A body on the
floor means three stars waiting outside.

**The gun store** was rebuilt: 4×3 tiles, a counter with a shotgun-armed
clerk, four cabinets down the sides (pistol $60, shotgun $150, flamethrower
$350, rockets $600), racked long guns on the wall, two browsing customers
with pistols. Thrown money is credit; when the credit reaches a price that
cabinet's glass slides back and the gun appears on the floor beside it.
Break a cabinet instead and an alarm sounds, the clerk and customers become
gunmen with the police AI, three stars are called on the spot and again at
the door, and that store's stock is gone for the rest of the run (eight
venues are remembered, by door position). Both interiors share one room
builder, one door, one set of per-frame rules (`inside_person()`,
`inside_update()`).

**The taxi**: take a cab (`CAR_TAXI`) and a pedestrian three to nine tiles
away waves; a yellow marker shows where; stop beside them and they get in;
the marker moves to a road point eight to sixteen tiles away; stop there
and they pay $15 plus one per half tile. Nobody hails a cab with stars on
it. `-DWR_TAXI=0` removes it.

### The garage trick

Under a parking deck with heat on, a caption says what the deck is for;
take a different car inside it and the wanted level goes to zero. The
theft's own heat is added first so it cannot come straight back.

### Retired

To pay for the above: the ambulance, cyclists, seafront parking bays and
meters, bodies dragged by cars, and the wall murals (which the owner
released). Boats are behind `-DWR_BOATS` (default off): four hulls, the
marinas and the marine unit cost about 2.4 KB with everything they pull in,
and they were the feature least missed.

### Size

Measured with a modern Thumb-2 GCC; the SDK's produces roughly 9% more, so
these are relative. The as-received build was 59,629 on this scale, with
the owner reporting 600–800 bytes of real margin.

| configuration | text+data+bss |
|---|---|
| as received | 59,629 |
| all of the above, boats off, taxi on | 60,090 |
| with `-DWR_TAXI=0` | ≈ 59,400 |
| with `-DWR_BOATS=1` | ≈ 62,500 — does not fit |

The pools were trimmed to their measured peaks: `MAX_GIB` 40, `MAX_ITEM`
144, `MAX_ENT` 44 (peak 39; police evict when full), `MAX_LOT` 32 (peak 30).
The pass that made room: part tables, one crash-physics helper set instead
of two mirrored copies, one boat driver, one `cop_shoot()`, `props_refresh()`
rewritten around `prop_put()` (which also fixed a bug: the first knocked-over
prop blocked every placement after it, so the streets emptied of furniture
once you hit a lamp post), and the terrain loop no longer paints ~350
lot-grey quads over a lot-grey background every frame (render cost on the
host fell from 0.16 to 0.11 ms/frame).

### Harness

`make check` runs `air`, `garage`, `inside`, `taxi` and the old suite.
`features` had an ambulance assertion that could never pass: the body *was*
collected, but its slot was recycled by a spawn in the same step, so a test
checking for an empty slot between steps never saw it. The old gun-shop
names (`shop_enter`, `ROOM_W`) are macros for the harnesses.

## Build

```sh
cd wrist-revoked
pebble build
pebble install --emulator emery      # or --cloudpebble for a real watch
```

Targets `emery` exclusively. The touchscreen, the speaker and the 128 KB app
budget are all Time 2-specific, and the game assumes a 200×228 framebuffer
throughout.

## Controls

| Input | Action |
|---|---|
| **Touch** anywhere | virtual analogue stick — forward, reverse, strafe |
| **Touch** right edge | zoom slider — drag **up to come in close**, down to pull out. The knob travels with your thumb, so the top of the strip is the near end. |
| **BACK** | pause menu — **Mirror** (left-shoulder mode), volume, sound |
| **UP** (hold) | turn left |
| **DOWN** (hold) | turn right |
| **SELECT** (click) | attack, or steal / exit the car you're standing at |
| **BACK** | quit |

The stick is **fixed** in the bottom-left corner at (38, 190), tucked as far
into the corner as it goes with its full throw still on screen — muscle memory beats hunting for a floating pad. Dead
zone 5 px, full deflection at 30 px, so the thumb barely travels. It is inset
far enough from the corner that a wide thumb has room on every side, and a
touch within 58 px of centre grabs it (`STICK_CX` / `STICK_CY` /
`STICK_DEAD` / `STICK_RANGE` / `STICK_GRAB` in `wr.h`). Throttle is analogue —
half a push is half speed.

It is drawn at all times, dim when idle, so it needs no tutorial. On foot it
is a ring. In a car it collapses to a **vertical slot**, because there is no
steering on the pad while driving — that lives entirely on the buttons.

BACK is deliberately left unbound. A raw click handler on BACK does not
reliably stop the window stack popping the app, so it does its normal job.

Because facing and movement are now separate inputs, on foot you get proper
twin-stick movement: back away from an officer while still facing him, or
circle a target with the strafe axis while your aim stays put.

Car yaw follows **signed** velocity, so reversing swings the nose the opposite
way to the same input going forward, exactly like backing a real car out of a
space. The rate is normalised against reverse top speed rather than forward,
so full reverse steers as hard as full forward instead of going vague just
because reverse gear is slower.

Turning is rate-based, not position-based: holding accelerates your angular
velocity up to a cap, releasing lets friction bleed it off. Cars only turn in
proportion to how fast they're rolling, so you can't pivot a stationary car.

## How it plays

- **Flora and beach** — trees in the parks and along the streets, palms,
  boulders and parasols on the sand. Trees, lamp posts, post boxes and boulders
  are *solid* — you cannot drive through them until you have snapped them off.
  Tables and chairs you can just plough through.
- **Off-road** — a car simply cannot reach its road top speed off the tarmac:
  68% on grass and pavement, 45% on sand. Capping the *target* rather than
  subtracting from the current speed matters, because a subtraction just loses
  to the throttle — the first attempt at this made sand frictionless. You coast
  down to the lower limit over about a second. Palms and boulders are no longer
  solid obstacles either; a scattering of immovable things on open sand was
  most of what made the beach feel like a wall.
- **Legacy note** — leaving the tarmac used to slam your speed to a hard cap the
  instant a wheel left the road, which felt like hitting a wall. It is rolling
  resistance now, so you wash off speed over about a second and can carry
  momentum over a kerb or along a beach. Sand drags twice as hard as grass.
- **Movement** — walking tops out at 9, backing up at 6, sidestep at 7. Cars
  do 30 forward and 13 in reverse, halved off the tarmac. Cars only turn while
  rolling, so you can't pivot a parked one.
- **Destruction** — cars take bodywork damage from gunfire and explode after
  about five pistol rounds, killing anyone nearby and hurting you if you are
  inside the blast. Wrecks stay put, blackened and smoking, and can't be
  driven.
- **Weapons** — fist, bat, knife, pistol, shotgun. Pickups are scattered on
  sidewalks and in parks; walking over one swaps your weapon and gives ammo
  (pistol 24, shotgun 12). Guns fall back to fists when empty. Shotgun fires
  three pellets in a spread.
- **Cars** — seven models with different sizes, speeds and handling: sedan,
  taxi, van, sports, truck, jeep and motorcycle. AI traffic keeps to the road. You don't have to —
  mounting the kerb works, you just lose half your top speed off the tarmac.
- **Crime and heat** — one body is roughly one star. Heat rises on kills
  (85), car theft (45) and killing police (170); a star is 80 heat, capped at
  five.
- **Police** — foot officers in blue uniform from one star, patrol cars from
  two, scaling with the level. Foot cops close in with nightsticks and start
  shooting at three stars. Cars ram you.
- **Escape** — heat only bleeds off once no police have been within five tiles
  for about 1.6 seconds. Stay clear and dispatch stops sending units; the ones
  already chasing fall behind and despawn at fifteen tiles. From five stars a
  clean run takes about thirty seconds.
- **Death** — the screen holds on a red vignette; tap DOWN to respawn
  somewhere else in the city with a clean sheet.

Pedestrians are men and women. GTA never put children on its streets and this
doesn't either — that was a deliberate line in the source material and it's a
sensible one to keep.

## Sound

`wr_audio.c` is a small software synthesiser, not a beeper. The obvious way to
use the Speaker API is `speaker_play_tone()`, and that is exactly why the first
version sounded like a doorbell: **a gunshot is not a pitch**, it is a noise
burst with a very fast decay. You cannot get there with an oscillator.

So the game opens one continuous PCM stream at 8 kHz, 8-bit signed, and mixes
into it every frame. 8000 / 25 = 320 samples per frame — a 320 byte buffer and
a few thousand integer ops, which is nothing on a 240 MHz core.

Six voices, each with a waveform, a pitch glide, a vibrato depth and a
per-sample exponential decay:

| wave | used for |
|---|---|
| `NOISE` | white — cracks, impacts, breath |
| `RUMBLE` | one-pole smoothed noise, gated by the oscillator — blasts, engines |
| `SQUARE` / `SAW` | sirens, voices, metal |
| `SINE` | pickups, atmosphere |

Everything is layered, which is what makes sounds read as objects rather than
tones. A pistol is a noise crack over a 150 Hz body with a downward glide. A
shotgun is the same idea, longer, with two low layers under it. An explosion is
620 ms of low rumble plus a noise sheet plus a sub layer. A scream is a
sawtooth with vibrato that glides *up* in panic and *down* while dying. The
engine is a rumble retriggered every four frames, pitched by road speed.

The synth has its own PRNG, deliberately separate from the gameplay one, so
audio can never perturb the simulation — otherwise a run would stop being
reproducible between plays. `speaker_is_muted()` is checked at init and the
whole synth is bypassed in Quiet Time.

## How the rendering works

Fixed-point throughout — no floats, no `snprintf`, no float formatting linked
in. Positions are int32 subunits (256 = one tile), angles are uint16 so they
wrap for free, trig is a 256-entry Q10 sine table.

The camera pitch is a compile-time constant (46° below horizontal), so its sine
and cosine are `#define`s and the projection is two shifts and two divides per
vertex. Buildings are extruded at draw time from a 2-byte-per-tile grid —
there is no stored geometry anywhere.

Framing is over-the-shoulder in the Resident Evil 4 sense, biased for a left
thumb. Two things happen together: the projection centre sits down and right
(`PROJ_CX` 146, `PROJ_CY` 130) so the character is well clear of the pad, and
the whole camera yaws about 12.6° (`CAM_YAW_OFF`) so he is angled across the
frame rather than staring straight up it. The yaw is what stops the offset
reading as a mis-aimed camera. Pushing the character down the frame also buys
lookahead: the horizon falls with him, so you see further up the road.

Camera distance and height are *not* fixed, and neither is pitch. You dial zoom
in on the right-edge slider, and **pitch rides the same slider**: fully in it
is a GTA 3 chase camera — 14°, down at street level, close enough behind him to
count the polygons — and fully out it tips toward straight overhead at 66°.
Where he sits vertically slides too, dropping down the frame when the camera is
low so there is sky above him.

Zoom is a *setting*, not an input state — letting go of the slider leaves the
camera exactly where you put it. The easing is symmetric and snaps the last
couple of units, because an arithmetic shift on a negative number rounds toward
negative infinity: a naive one-liner crept downward forever but stalled seven
short of the target whenever it was climbing.

Below about 20° the horizon lands on screen, so the renderer draws sky above it
and a haze band below before any geometry goes down. Locking the two together is what makes both ends useful — a close
top-down view shows you nothing, and a distant low view is all rooftops.

Speed adds to zoom but never subtracts, so flooring it always pulls the camera
out and can never fight a deliberate setting.

**The character does not move when you zoom.** `PROJ_CY` is solved every frame
from the current camera parameters so he lands on the same pixel regardless of
pitch — without that, sliding pitch from 34° to 66° drags him from below the
bottom edge of the screen to near the top. He stays put at (154, 148) and the
world tilts around him.

There is no pinch gesture, and this is a hardware limit rather than a choice:
PebbleOS reports touch as a single `TouchEvent` with a type and one x/y pair
and **no finger identifier**, so two-finger input cannot be distinguished. The
right-edge slider is the single-touch equivalent, placed where the stick isn't.

Three passes per frame:

1. Clear to asphalt, then draw flat ground quads. Ground is coplanar so it
   never occludes itself and needs no sorting.
2. Blood decals, also flat, straight after.
3. Collect everything with height — buildings, cars, people, pickups,
   explosions — into a draw list keyed by depth, insertion-sort far to near,
   and paint. The list is nearly sorted frame to frame so the sort is close to
   linear.

No z-buffer: at 8bpp that would be 45.6 KB, a third of the entire app budget.
Painter's algorithm plus per-face backface culling by screen-space signed area
is correct here because everything is a convex box.

Shading is a lookup, never a calculation. Every material has a hand-picked
three-step ramp baked as `GColor8` bytes, and the rasterizer takes a colour
byte rather than a normal.

### Models

`wr_model.c` builds every character and vehicle out of one primitive,
`gfx_part()` — a box placed in an entity's local frame with its own extra yaw.
Limbs, wheels, doors, gun barrels and car bodies are all the same call with
different numbers. Nothing is stored as geometry; the rigs are assembled from
constants each frame, which costs a handful of shifts.

A pedestrian at full detail is seven boxes: two legs, torso, head, two arms and
whatever is in the right hand. Legs swing fore and aft off the same sine table
the renderer uses, arms counter-swing, and the body lifts slightly at
mid-stride. An attack drives the right arm — and the weapon with it — forward
for eight frames. Weapons are visibly different shapes: a stubby pistol, a long
shotgun, a bat, a short bright knife.

Cars at full detail are body, cabin, four wheels, windscreen, headlights and an
animated driver door, plus a flashing bar for police. The door swings about its
front edge and is drawn either before or after the body depending on which
flank faces the camera — worked out by comparing `gfx_depth_at()` either side
of the car, so it needs no extra camera state.

Detail is spent where you are looking. Beyond `LOD_FULL` a character drops to
three boxes, beyond `LOD_MID` to one; the player is always full detail. Without
this, a crowded street would cost more than the whole rest of the frame.

## Physics

Nothing here is a rigid-body engine — it is a handful of integrators that each
buy one specific behaviour.

- **Ragdolls.** Run someone down and the body inherits your heading and speed,
  gets lift and a tumble rate, then integrates under gravity. It bounces off
  the tarmac (shedding energy and throwing more blood on each impact), skids
  under friction, and rebounds off walls. Only a real downward impact bounces —
  the threshold is deliberate, since an `abs(vz)` test leaves bodies twitching
  on the spot forever instead of settling.
- **Debris.** Street furniture and shop glass throw particles with the same
  arc, bounce and skid as blood, but coloured by whatever broke — splinters off
  a table, glass off a cabinet, painted metal off a hydrant, leaf off a tree —
  and they leave no pool behind. One particle system, two materials.
- **Blood.** Droplets, not squares. A hit sprays particles that inherit the
  impact direction, arc, bounce, skid and stop against walls — and each one
  stamps a decal where it comes to rest. The pool that remains is the shape the
  spray actually made.
- **Car crashes.** A pairwise sweep over live vehicles (never more than about
  ten, so a few dozen checks). Closing speed sets the damage, both cars take
  it, they are shoved apart along the line between them, lose most of their
  speed and get knocked off-heading. Enough damage and one of them goes up.
- **Police contact.** A patrol car ramming you damages the *bodywork*, not you
  directly — and when the car dies it dumps you onto the street where they can
  reach you.
- **Arrest.** An officer on foot who gets within reach while you are slow
  starts building a grip, shown as a bar at the bottom of the screen. He hauls
  you out of a vehicle first, then cuffs you: BUSTED, which reads as a blue
  vignette rather than the red one you get for dying. Keep moving and he never
  gets hold of you.

## The city

Nothing about the city is stored. `cell_at()` is a pure function of the tile
coordinate — roads, sidewalks, parks, beach and sea all fall out of two modulos
and a hash. That deleted the 4.6 KB grid outright, and it means the island can
be any size for free.

It **is** an island: a wobbled circle, a ring road hugging the shoreline, then
beach, then water. The sea stops everything, so there are city limits without
an invisible wall. The coast is tested in *squared* distance against a true
circle — the first version compared `wr_dist()`, an octagonal metric, against
an integer radius, which is exactly why the shoreline came out in blocky steps
and the ring road broke into slabs.

**Every street runs on the diagonal.** Two families of parallel avenues at
right angles to each other, so blocks are diamonds and almost every junction is
angled rather than square.

Buildings are no longer derived from the street grid at all. Candidate
footprints sit on a fixed lattice and a lot is only built where every sample
inside it is still buildable ground. That decouples the architecture from the
road layout completely — the avenues can run at any angle and the buildings
just fill whatever is left over.

Buildings are generated as **lots**, not tiles. Each block is split once — into
one big building, two, or four around a light well — and each lot is drawn as a
single box spanning several tiles. Lots come in three kinds:

- **Solid**, one to five storeys.
- **Parking garage**, low with an open ground level you can drive straight
  into. Breaking line of sight inside one is the cheapest way to shake a tail.
- **Storefront**, glass frontage at street level. Hit it above 12 and the
  window goes, showering glass, and the lot stops being solid — you can drive
  right through the shop.

Lots for the 5×5 blocks around you are cached and only rebuilt when you cross
into a new block, so drawing, collision and prop placement all read one small
array.

### Losing a helicopter

Air support pins the evasion timer whenever you are not under cover, and the
helicopter holds station overhead and never falls behind - so from four
stars, ducking under a parking garage deck was the *only* escape that
existed. A straight high-speed run could not work however long you kept it
up, which is not obvious from inside the game and reads as the wanted level
being broken.

Two changes. The helicopter is slower than a car flat out, deliberately, so
a fast car can open a gap on it. And it only counts as eyes-on within four
tiles rather than the full ground sight range: an aircraft trailing half the
island behind you is not keeping you in view. If it spends long enough with
you well ahead of it, it loses you and turns for home.

### What the chase harness can and cannot show

Worth being straight about, because the test names imply more than they
prove. `cover_test.c` asserts the escape end to end: five stars, park under
a garage deck, clean in 33.5 seconds. That is the real assertion.

`chase_test.c` asserts the *crime* half - that bodies raise the heat and
that dispatch puts the right units on you, five officers and four patrol
cars at five stars. It no longer asserts that its driver completes a
getaway. That driver is a scoring function, not a player: in a city this
dense it dodges traffic, corners badly, and at five stars gets boxed in and
rammed off the road. Across runs its best contact break varies from almost
nothing to a third of the heat gone, so a threshold there is a coin flip
rather than a regression test. The numbers are printed on every run so a
real regression is still visible in the log.

### The ring road is a curve now

The coastal ring was painted tile by tile, because its classification is
tile-quantised and per-tile is therefore its exact shape. Exact, and ugly: a
bend rendered as a staircase of squares, and no centre line on it at all -
on the one road in the city where a centre line has something interesting to
do.

It is walked as an arc now. A ring of short quads follows the true circle,
each seam shared with its neighbour so the bend has no gaps, and each
segment masked against the classifier on all four corners so paint cannot
land where the road is not. Only the arc in front of you is emitted - a full
ring is two hundred quads and most of it is behind the camera.

The dashes are the point of it. They step by **arc length** rather than by x
or y, so they come out evenly spaced around the bend instead of bunching up
on the diagonals, and each one is laid along the local tangent so it stays
square to the road as the road turns.

**The 328-pixel hunt, which is worth writing down.** A 480,000-pixel
consistency scan reported 328 pixels of road colour over open sea. I assumed
the new curve was overhanging the waterline and narrowed the band three
times - 290, 262, 226 units. The count came back *identical* every time.
Then I masked the ring against its own classifier instead of the general
road test: identical. Then I masked the full extent of every centre-line
dash rather than its midpoint: identical.

A number that will not move under four unrelated changes is not measuring
what you think it is. Dumping the offenders' coordinates showed all of them
clustered at 679-735 units from the bridge centreline - just past the
scan's own exclusion of `BRIDGE_KERB + TILE`, which is 676. They were the
highway's shoulder, painted exactly as intended, on a road the test had
stopped excluding two hundred units too early. Widening the exclusion to
the actual painted corridor: **328 to 11**.

The renderer was right the whole time and the test was wrong. Both of the
masking changes made along the way are kept anyway - a ring segment should
be validated by the ring classifier rather than by whatever road happens to
be nearby, and a dash 256 units long should be checked along its length
rather than at its midpoint - but neither of them was the bug, and it is
worth being clear about that.

### Finding the gun shop

Raising gun shops from 6% of lots to 14% put one within a couple of blocks
of anywhere - measured, 40 of 40 sampled street positions have one in the
loaded cache, the furthest 27 tiles off - and yet they were still easy to
walk straight past, because from street level a shopfront looks like every
other shopfront until you are on top of it.

So there is a **pointer** now: a small orange marker that orbits the
character showing the bearing to the nearest one. It only appears on foot,
only when a shop is actually in the loaded block cache - so it promises
nothing the world has not already built - and it disappears once you are at
the door. The bearing is relative to the camera rather than to north, so the
marker sits where the shop is on screen rather than where it is on a
compass, which is the difference between a hint and a puzzle.

### The engine keeps running

The old note about a "monophonic audio gate" was stale - the synth has mixed
six voices for a long time. The real complaint underneath it was true
though: a shotgun is three voices and an explosion another three, so a burst
of fire would evict the engine drone through the quietest-slot allocator and
the car would fall silent underneath you mid-chase.

Slot 0 belongs to the engine now and nothing else may take it; everything
else claims the quietest of the remaining five. That costs a sixth of the
polyphony and fixes it outright.

**Caveat:** `engine_test.c` is a diagnostic rather than a gate. The host
stub never opens an audio stream, so no voice is ever marked active and the
probe cannot see the engine from here - the allocation policy is small and
inspectable, but like the stipple it wants an ear on real hardware.

### Occluders are stippled, not deleted

A building that gets between you and the camera used to be skipped outright,
so it blinked out of existence the moment it crossed in front of you and
blinked back when it cleared - far more distracting than whatever it was
hiding. It is drawn as a **checkerboard** now: every other pixel, on a
pattern keyed to screen position so it stays put as the world moves under
it. The building stays present, you can see your character straight through
it, and the transition is a change of texture rather than an object
vanishing.

That is one flag on the rasteriser rather than a second fill routine, and it
is cleared on entry to every lot as well as on exit - `draw_lot()` has ten
return paths, and a stipple flag that leaked past any of them would
checkerboard the entire city.

**Caveat, honestly:** I could not get a headless scenario to reliably drive
the occlusion branch, so `occ_test.c` is in the tree as a non-gating
diagnostic rather than a passing test. The rasteriser change is small and
inspectable and nothing else regressed, but the end-to-end effect is
unverified - if buildings look wrong when this finally runs on a watch, this
is the first thing to suspect.

### Bodies get dragged

A ragdoll that had come to rest used to be inert scenery you could drive
straight through - the clearest possible tell that the physics stops caring
once something is dead. A wheel over a body now kicks it along the car's
heading with a bit of lift, so it tumbles down the street ahead of you and
leaves a fresh smear where it lands.

Two conditions keep it from being silly: only a vehicle actually moving does
it, and only to a body already settled on the ground. A fresh kill has its
own arc and does not want interfering with halfway through it.

Measured in `drag_test.c`: a car at speed drags a body 226 units down the
street, and a car parked beside one moves it not at all.

### Piers

Two planks set at an angle was not a marina, it was a hint of one. Each
mooring now grows a **pier**: sections laid end to end running out from the
shore into open water, with pilings down both flanks of the last one so the
far end reads as standing in the water rather than floating over it.

It finds its own direction rather than being told. The generator samples the
compass from the mooring, walks each bearing until it runs out of water, and
builds along whichever had the longest clear run - so the pier points out to
sea whichever shore of whichever island it happens to be on, and stops when
the water does. Nothing about it is hand-placed per marina.

Measured in `pier_test.c`: all four moorings across both islands grow a pier
of at least three sections, the longest runs to ten, and no section is ever
built on dry land.

### Persistence

Three integers survive closing the app: lifetime kills, lifetime thefts, and
your best escape - the frame count from the moment you become wanted to the
moment the last star goes. They are shown on the death and arrest screen,
which is the only place they appear and also the only moment they are final.

Two details worth keeping. The tally is banked when you die or are taken,
not continuously, so a run in progress cannot inflate the record. And a
slower getaway never overwrites a faster one, which is the whole point of a
best time.

The HUD builds that line **by hand**, with a bounded integer-to-decimal
helper rather than `snprintf`. This game deliberately links none of the C
formatting machinery, and one careless `%d` drags the whole of it into the
binary - the first version of this feature did exactly that, and the
compiler warning was the only thing that caught it.

Verified in `stats_test.c`: a death banks the tally, the tally survives a
restart, a clean getaway records a time, and a slower one does not replace
it.

### Skylines

A tower that goes straight up and stops dead is the most obviously generated
thing in a city. Tall lots now step inward as they rise: one crown above
five storeys, two above seven, each inset from the last and slightly
tapered, with a mast and a blinking obstruction beacon on anything over
eight. It is purely additive - the walls below are untouched - so it costs a
few parts on the handful of buildings big enough to warrant them, and the
downtown core reads as a skyline rather than a bar chart.

### Less blocky, part two

The frustum primitive earned its keep, so it now does the boats and the
helicopter as well:

- **Hulls** flare from the keel to the deck, which is most of the difference
  between a boat and a floating crate, and the forward section narrows to a
  prow at the waterline while flaring at the gunwale - that is what gives a
  boat its sheer.
- **The helicopter** fuselage narrows toward the top and shifts back so it
  reads as a body with a spine rather than a shoebox, the canopy noses down
  and in, and the tail boom thins along its run.

Render cost is 0.36 ms/frame on the host, up from 0.19 before any of the
taper work. That is the price of the whole de-blocking pass and it is the
number to watch first on real hardware - roughly 15 ms of a 40 ms frame by
my scaling, which should be comfortable but has never been measured on a
watch.

### Foot pursuit around buildings

Foot police used to aim straight at you and, on a blocked step, swing 45
degrees and try again next frame - which walks a man into a wall, slides him
along it, and traps him outright in a dead end. They now probe outward from
the direct bearing in widening pairs and take the first heading with clear
ground a stride ahead, so they peel around a corner roughly the way a person
does. The chosen detour is held for a couple of dozen frames rather than
re-decided every frame, because re-deciding at a corner leaves a man
jittering on the spot between two equally blocked options.

On top of the probe there is now a **route**. It needs no navigation graph
and stores nothing, because the street layout is not arbitrary - it is two
families of parallel lines at right angles. Work in band coordinates
(u = x+y, v = x-y) and the problem collapses: every street is a line of
constant u or constant v, and moving along a u-street changes only your v.
So the route from anywhere to anywhere is the Manhattan one - get onto the
nearest centreline of the family that closes the bigger gap, follow it until
that coordinate matches the target, then switch families. Two legs, a dozen
lines of arithmetic, no search and no memory. When the direct line to you is
blocked, that is what a foot officer steers along; the blind probe still
runs on top of it, handling the doorway and the parked car the route cannot
know about.

This is **better, not solved**, and the numbers say so: in `path_test.c`,
which starts a cop directly opposite the player across a building, 2 of 4
reach him, up from 1 of 3 with the probe alone. That test is deliberately kept out of `make check` - it is a
diagnostic with a recorded baseline rather than a gate, because a suite that
is permanently red teaches you to ignore it.

Worth recording what did *not* work: making the computed route override the
probe entirely - clearing the detour hysteresis and refusing to turn out
when boxed in - took the score *down* to 1 of 4. The blind groping is not
noise to be tidied away; it is what gets a man out of the corner the route
walked him into.

### Swimming

Walk into the sea and you swim. The only collision change is one line - open
water stops being a wall for a player on foot - and everything else about
collision is untouched, so the shoreline, the buildings and the props all
still stop you exactly as they did.

The trade is deliberate. You move at 5 against a walk of 9, and **you cannot
fight while you are swimming**: both hands are busy staying afloat. So the
water is a way out of a chase and a way onto a moored boat, not a way to win
a gunfight from somewhere nobody can reach you. The swimmer is drawn sunk to
the chest and leaves a small wake.

Boarding reach goes from 190 units to 300 while you are in the water. Lining
yourself up to within 190 of a hull while your own strokes push you about is
a miserable way to steal a boat, and treading water beside one you can
plainly reach further than you can from a jetty.

Verified in `swim_test.c`: walk off the sand and you are swimming two tiles
out, at speed 5, unable to fire - and you can haul yourself aboard a moored
boat from the water.

### Losing them

The wanted system had no way down at four stars and up, and the reason was
structural rather than a matter of tuning. Evasion measured one thing - the
distance to the nearest officer - and `ent_any_cop_near()` counted the
**helicopter**, which holds station directly overhead at essentially zero
horizontal distance. So from the moment air support arrived the evasion
timer could never start, and the heat had nowhere to go. This file has
claimed since the garages were built that ducking into one is the cheapest
way to shake a tail. That claim was simply false.

It is true now, and it took three things:

- **Cover shrinks their sight.** Under a garage deck the police have to be
  almost on top of you to keep eyes on, instead of seeing you across five
  tiles of open street.
- **A roof blinds the helicopter.** It cannot see through concrete, so it no
  longer pins the timer while you are underneath one.
- **The police work from a last-seen position.** In the open that is simply
  where you are; the moment you are under cover it stops updating, so they
  converge on the spot they last had eyes on and mill about there rather
  than walking straight to a man they cannot possibly see. That last part is
  what turns a garage from decoration into a tactic.

Measured end to end in `cover_test.c`: five stars to clean in **33.5
seconds** under a deck.

One honest limit, and it is correct behaviour rather than a bug: dive into a
garage with a patrol car on your bumper and they follow you in, they can
still see you, and nothing decays. Break contact first, then disappear.

That is also why `chase_test.c` no longer asserts an escape. It drives a
scoring function, not a player - it dodges traffic, corners badly and never
opens the gap a getaway needs - so it asserts the pursuit half of the loop
(crime raises the heat, dispatch sends the right units) and leaves the
escape half to the cover test. One test per property, each demonstrating
something it can actually demonstrate.

### Less blocky

Everything in the game is a box, because `gfx_part()` was the only
primitive. `gfx_taper()` is the same thing with a top face that can be a
different size and offset from the bottom - a frustum. It costs exactly what
a box costs, four sides and a cap, and it is the cheapest available cure for
how angular everything looked:

- **Cars** get tumblehome. Real bodywork narrows toward the waistline and the
  roof is shorter than the floor it sits on; two frustums instead of two
  boxes is the difference between a car and a brick. The cabin also slopes
  back, so there is a windscreen rake.
- **People** get shoulders wider than their waist and heads that narrow at
  the crown.
- **Trees** flare at the base of each tier and close above, so a canopy
  reads as foliage instead of three crates on a pole. **Palms** get fronds
  that taper to a point and droop outward from a trunk that thins as it
  rises.
- **Traffic cones** are conical.

### Wall murals

Some buildings carry a mural for one of the Mirror apps - MirrorMap,
MirrorMsg or MirrorMov - sprayed on the side in that app's own colours: a
panel, a top band, a watch or a speech bubble, and a three-letter wordmark.

The wordmark is three letters on purpose. A painted wall is maybe sixty
pixels across at this range, so "MIRRORMAP" would be five pixels a letter
and unreadable; "MAP" under the app's colours is legible from across the
street. The glyphs are a 3x5 stroke font drawn as one quad per run of set
pixels in a row, which is about seven quads a letter.

Three things about it were wrong before they were right, and all three are
the same lesson - lattice-aligned coordinates make terrible hash inputs:

- **Keying the mural to a hashed face** put most of them on walls the
  camera never sees, because a backfacing wall is skipped before that code
  runs at all. It goes on the first face actually being drawn.
- **Keying the brand to the lot corner** gave two thirds of the city to one
  app, and no amount of avalanching the combined value fixed it, because
  lot corners are lattice multiples and stay correlated. The brand comes
  off the *wall's* midpoint now: different for every painted surface,
  stable frame to frame, so a wall always advertises the same app and its
  neighbours differ.
- **Gun shops became common enough to actually find** (6% of lots to 14%),
  which quietly thinned the downtown skyline, because shops were exempt
  from the tower conversion. They are barred from the core instead.

Measured over sixty random street views: eleven show a mural, split four
MirrorMap, three MirrorMsg, four MirrorMov.

### Casting off

A moored boat points at the jetty it was tied to, and land stops a hull
dead - so pushing the stick forward did nothing and the boat felt broken.
Boarding one now sweeps the compass and casts off facing open water.

### Where the map ends

The sea has always stopped a car dead, and with nothing on the ground to say
so it read as an invisible wall rather than the edge of the world. There is a
works cordon there now: a striped barricade with a flashing amber lamp and a
cone either side, wherever the drivable surface meets open water.

Two details make it work. They are **solid**, so you hit the barrier rather
than the sea — but they shear off their base like any other post, so you can
smash through and let the water do the real stopping, which is the honest
order of events. And they are **spaced by a hash rather than continuous**, so
the line has gaps in it: it reads as a works cordon instead of a fence, and
you can still get down to the water to reach a boat.

The rule is "any land tile with open water beside it", which is deliberately
not "any road tile" - roads never touch the sea, there is always beach
between, so the obvious test finds nothing at all. As a free consequence the
same rule guards the **bridge**, whose deck kerb genuinely does have open
water beside it: barriers march down both edges as a crash rail without a
line of code that knows what a bridge is. Measured: 38 of 43 shoreline
approaches carry a barrier (the rest are the deliberate gaps), 3 of 3
over-water bridge sections have a rail, and no barrier stands anywhere that
isn't beside water.

### Junctions

Every crossing now carries **mast-arm signals**: a twenty-foot pole set back
on the pavement corner, a horizontal arm reaching out over the carriageway,
a signal head hanging off the end of it and a luminaire beside it. They are
placed unconditionally, before any other street furniture gets a slot -
previously they were hashed in with the benches and post boxes, so most
crossings had none at all - and the hashed pass now keeps clear of junction
corners so a mast is never buried in café chairs.

Worth recording, because it is the kind of thing that looks right in the
maths and wrong on screen: both street families run at 45 degrees, so they
cross at a right angle and **the corners of the junction box lie on the
world axes, not on the diagonals**. Offsetting the masts diagonally, which
is the obvious thing to write, plants every one of them in the middle of
the crossing carriageway.

The road markings make it read as a junction rather than two roads that
happen to overlap: a **stop bar** across the near lane on each of the four
arms, a **crosswalk ladder** just outside it on the near ones, and centre
lines that **stop at the box**. A lane divider painted straight through the
middle of a crossing is the single clearest tell that a road layout was
generated rather than drawn.

### Parks are blocks, not tiles

The grey squares dropped into the grass were a rendering artefact, not bad
classification - a scan of 3,264 land tiles finds no isolated road, no road
ringed by park and no pavement that leads nowhere. Parks were painted tile
by tile, and a tile grid cannot follow a 45-degree kerb, so every park edge
came out as a staircase of green boxes sitting in the pavement. A park is
now drawn as its **block's own diamond**, inset from the centrelines by the
pavement width - the same shape the buildings are fitted into - so it lines
up with the kerbs exactly.

### Cars that hold together at ground level

The game is framed from above, but you can pull the camera down to street
level, and from there the cars came apart: a body slab hovering eight units
off the ground over four separate cylinders parked outboard of it. Three
changes fix it. Ride height drops to three units, so there is no bright
strip of ground under the car. The wheels tuck *inside* the body line with
their radius derived from the body height, so their tops overlap the body
rather than floating beside it. And each flank gets a **sill** - the rocker
panel between front and rear wheel - which closes the gap that made the
wheels look detached.

### Making the city make sense

A pass over the generator looking for things that should never exist, with a
scanner that samples every lot in the cache over several world seeds and
counts nonsense. Two real faults turned up, and the scan is now the bar:
**4,614 lots, zero overlapping a road, zero on the ring road, zero on the
bridge deck, zero on the beach, zero in the sea; 3,618 props, zero standing
inside a building.**

- **Lots straddled roads near the coast.** A block was accepted on its
  centre tile alone, but the square inscribed in it can still cross the
  ring-road annulus or the highway corridor — so whole buildings sat across
  the carriageway. Every sublot is now sampled on a 3×3 grid and dropped
  unless all nine points are buildable. A rejected lot leaves a courtyard,
  which reads as deliberate.
- **The gaps between sublots were 28–36 units.** Invisible at any distance,
  so a split block read as one building with a phantom seam a car could
  slip through — the drive-through buildings. They are **88-unit alleys**
  now: wide enough to see, wide enough to walk down, and cutting through one
  reads as ducking down an alley rather than clipping through a wall.

### Parking garages

A garage was a box with an open ground floor, which read as a building you
could inexplicably drive through — it wasn't obvious it was a garage at all.
Four things fix that: the deck slab you can see under, **columns** holding it
up, **painted bays** on the floor, and **cars parked in them**.

The parked cars are real obstacles. `city_garage_slots()` derives the bays
from the lot itself — two rows with an aisle down the middle of the longer
axis, some bays deliberately empty — and both the renderer and the collision
test call it, so nothing is stored and they cannot disagree. Driving through
a garage now means threading the aisle, which is a far better version of the
same shortcut: it still breaks line of sight, it just asks something of you.

### Street lights

The old lamp post was a small box on a pole, which reads as a bollard. A
street light is now a mast with an **arm reaching out over the carriageway**
and a lamp head hanging off the end, lit underside and all. Each one is
given a facing when it is placed — the nearer of the two street families,
found from the sign of the cross-coordinate offset — so the arms lean over
the road from both kerbs the way real ones do.

### Why the old buildings flickered

Worth recording, because two earlier attempts to fix it missed the cause. Each
tile was drawn as its own box. Adjacent tiles of a block have *almost
identical* depths, so the painter sort swapped their order every frame — that
was the flicker. And every party wall buried inside the block was still being
rasterised, which is what made blocks look like they were missing walls. One
box per lot fixes both at once: one depth, no seams, nothing to fight over.

## Street furniture

Lamp posts, stop signs, post boxes, hydrants, and café tables and chairs
clustered on the pavement. All placed deterministically per tile, so they don't
wander when the cache rebuilds, and all destructible.

A glancing knock rocks them and they spring back upright. Hit one hard enough,
or often enough, and it **shears off its base** — at which point it becomes
loose debris with its own physics: it flies, tumbles, bounces, skids and comes
to rest wherever it lands, and stays there. Everything sprays particles on
impact, and everything you hit costs you bodywork. Driving through a café
terrace at speed does exactly what you'd hope.

## The gun shop

Some retail lots are gun shops, signed and with a door on the front. Walk up on
foot and press SELECT and the scene changes: you are inside.

There is no second engine for this. Entering swaps the lot cache for a handful
of wall lots and the props for a counter and three display cabinets, so
drawing, collision, ragdolls, particles and audio all carry over untouched.
The camera pulls right in, because indoors there is nothing to look ahead at.

The man behind the counter is a pedestrian like any other. Smash a cabinet —
with a bat, a bullet, anything — and it spills its stock on the floor. Stock is
keyed to the cabinet slot rather than hashed off position, so every shop
reliably holds one **flamethrower** and one **rocket launcher** rather than
three of the same thing.

- **Flamethrower** — three tongues per pull, hosed side to side with random
  spread. Short range, very fast cycle, and the flame keeps burning *through*
  whoever it touches rather than stopping at the first body.
- **Rocket launcher** — five rounds, flat and fast, detonates on any impact
  including a wall. One hit wrecks a car outright.

## Two islands, and the bridge

The world is now two islands: the main city, and a smaller one across the
water. `cell_at_point()` takes whichever island centre is nearer and tests
against that one's shore radii, so both get a coastline, a beach, a ring road
and a full diagonal street grid for free.

They are joined by a **highway**. Its centreline runs between the two island
centres, and the road test wins over everything else — so the same few lines of
code give you a main road cutting across the first island, a bridge over the
open water, and a highway continuing into the second. There is no separate
"bridge" system to keep in sync with the terrain.

The deck sits at sea level as a causeway rather than a raised span. That was a
deliberate call: a raised deck would mean plumbing a world-space Z through
collision, entity movement and the painter sort, for a view angle where you
would barely see the difference. Pylons march down both sides where the deck
crosses open water, which is what actually reads as "bridge".

Measured end to end: driving from one island centre to the other is about 1,100
frames, on road 100% of the time, with 280 of those frames over open water.

## Boats

Four hulls, in `BOATC` / `BOATD`:

| | character |
|---|---|
| **jet ski** | quickest and twitchiest; will outrun the marine unit |
| **motorboat** | the middle option, outboard and windscreen |
| **yacht** | long, slow, with a cabin and a mast |
| **police boat** | motorboat hull, blue livery, flashing bar |

Boats are the exact inverse of every other vehicle: `water_blocked()` says open
water is passable and land is not, and `move_player()` picks the collision
function from what you are sitting in. A hull answers the helm at any speed
rather than needing to be rolling like a car, and reversing still swings the
bow the other way.

Each boat trails a **wake** — three pale wedges astern that widen with speed.
It is three quads and it is the single cheapest thing that makes a boat look
like it is moving *through* water rather than sliding over it.

**Marinas** are two moorings per island, on opposite shores, and civilian craft
only spawn there. That gives the water somewhere worth going rather than
scattering unattended boats across the bay. Each has a planked jetty with
mooring posts.

You board from up to 190 units away — further than a car — because otherwise
you would have to be standing on the water first. Stepping off searches
outward for dry land, and **if there is none within reach you simply stay
aboard**: putting the player down on open water would leave him standing on the
sea, which nothing else in the game has a concept of.

The **marine unit** turns out at two stars, but only while you are actually
afloat — it is no use tied up at a jetty on the far side of the island while
you are driving through town. It comes to you across open water rather than
launching from a marina, rams, and opens fire from three stars. A jet ski can
shake it; a yacht cannot.

## Air support

At four stars a **helicopter** turns up. It has no road to follow, so it just
closes on you and holds station overhead — rotor and tail rotor both turning.
Once it's over you it fast-ropes **SWAT** down, two at a time: black armour,
130 HP, and machine guns that fire roughly four times faster than a service
pistol. While a man is on the rope he can't fight.

Kill one and he drops the machine gun. That's the only way to get one.

The helicopter is armed itself. From `HELI_WANTED` it runs a **door gunner**:
rounds fired from overhead with a deliberate spread, so it walks fire across
the ground around you rather than sniping you dead from a position you cannot
reach. Measured 47 rounds and 20 hits over 400 frames of holding station.

One bug worth recording from building it: `ent_any_cop_near()` only counted
police on foot and in cars. That made the marine unit self-destructing — it
would be dispatched, nothing reset the evasion timer because no *land* unit was
nearby, and a second later dispatch decided you had escaped and deleted the
boat that was busy ramming you. Boats and the helicopter count as pursuit now.

Buildings **cull themselves when they get between the camera and you** — but
only when they genuinely are. The first version took the nearest of a lot's
four *top* corners as its depth, which is wrong in a way that gets worse the
taller the building: depth falls as height rises, so a tower standing well in
*front* of the character reported a tiny depth and got hidden. Occlusion depth
now comes from the corners on the ground, the building has to be a good tile
and a half nearer than the player, and the screen test is inset rather than
padded so it has to sit squarely over him.

 The
player's screen position and depth are computed before anything is drawn; any
lot nearer than that whose projected bounding box covers him is skipped for the
frame. Without it, driving a street with towers either side means losing sight
of your own character every few seconds.

## Stability pass

A round of deliberate bug hunting, mostly on the street rendering and traffic.
Recording the causes because several were subtle and none were where the
symptom pointed.

- **Pavement painted over the road at every crossing.** The two diagonal
  families were drawn one after the other, so the second family's kerb went
  down on top of the first family's tarmac. Every intersection had a slab
  through it that shifted as you moved. All the pavement now goes down first,
  then all the tarmac.
- **Streets appeared to slide with the camera.** The bands were only fourteen
  tiles long, so at a low camera pitch you could see the end of one, and it
  tracked you. They now run far enough to reach the haze at the horizon.
- **Sub-pixel centre lines flickered.** A continuous hairline is thinner than
  a pixel at any distance and winks in and out as the scan converter rounds
  it. Markings are dashes over a short range now, on the two streets you are
  actually near.
- **The near clip punched a hole in the road under your feet.** Clipping used
  a worst-case margin of `half_wid + 200`; at street level the eye is only
  ~300 units behind you, so a 250-wide road was clipped further away than the
  player. Superseded: bands now keep a coarse margin of geometry behind the
  eye and the exact cut happens per corner in the camera-space polygon
  clipper, so there is no margin to get wrong in either direction.
- **The clip arithmetic overflowed.** A street is tens of thousands of units
  long and a point behind the eye has a large negative depth; the naive
  product blew past int32 and clipped to somewhere arbitrary. 64-bit
  intermediates.
- **Cars veered off the centreline.** `>> 10` is an arithmetic shift and
  rounds toward negative infinity, so on a 45-degree heading the negative
  component lost a unit every frame and the positive one did not. Anything
  travelling diagonally walked sideways off its lane. Added `fmul()`, which
  rounds symmetrically, and routed all movement through it. Measured drift on
  all four street headings is now exactly zero.
- **One bump killed a car forever.** `car_collisions` scrubbed speed on
  impact and nothing ever restored it, so traffic accumulated as permanently
  parked vehicles. Cars now accelerate back to a per-model cruising speed, and
  a car that gets wedged is put back on the nearest centreline.
- **Traffic drove across the roads.** Vehicles spawned on axis-aligned
  headings while every street runs diagonally, so they spent their lives
  turning away from kerbs. They now spawn snapped to a centreline pointing
  down it. On-road time went from intermittent to 100%.
- **Street furniture stood in the carriageway.** Tiles are classified at their
  centres, so a pavement tile straddling the kerb could host a lamp post that
  poked into the road and stopped cars dead. Props now keep clear of the
  tarmac by their own radius.
- **The draw list silently overflowed.** It capped at 110 items against a
  worst case well over that, so on a busy street whatever got pushed last
  simply was not drawn.
- **Two undefined shifts.** `tx << TILE_SH` on a negative tile coordinate,
  which happens off the island's origin side. Replaced with multiplication.
- **The helicopter never left.** It follows you rather than falling behind, so
  the distance despawn never fired and it stayed overhead at zero stars.
- **You could drive a wreck.** Several places destroyed a vehicle without
  checking whether the player was inside it. All of them go through
  `vehicle_destroy()` now, which ejects you.
- **Dying indoors stranded you.** `respawn()` searched for a road while the
  gun shop was still loaded, found nothing, and left you standing in the sea.
  It drops the interior first.

- **`shop_exit()` with no shop open** teleported you to the saved door
  position, which is (0, 0) until a shop has been entered — i.e. out to sea
  with no city loaded. Guarded.

Verified with 150,000 frames of randomised input under ASan and UBSan —
driving, shooting, shop interiors, menu, explosions, and teleports into the
sea — with no diagnostics.

### Lane discipline

Every street was two-way but had a single shared centreline, so traffic in
both directions drove down the exact middle and met nose to nose. Each car
now holds a lane 92 units to *its own* right of the centreline — which for
opposite headings is opposite sides of the road, so the two streams pass
each other properly. It is a steering correction rather than a teleport, so
a car joining a road eases into its lane over about a second.

The nudge only applies on the diagonal grid. The first version applied it
everywhere, including the highway between the islands, where the x+y / x−y
cross coordinate means nothing — that steered every car off the bridge and
took the island-to-island drive from 100% on-road to 24%. A car is only
"on the grid" if it is actually pointing down one of the four family
headings.

Car-following is direction-aware to match: only same-direction traffic
counts as a leader, so oncoming cars just pass. A stopped car or a wreck in
the lane is an obstacle whichever way it faces.

### Fire

A car that gets written off usually **catches fire** rather than exploding on
the spot: flames off the bonnet, a few seconds of burning, then it goes up.
The delay is randomised so it never feels scripted, and a minority still
detonate outright. Anything that touches a burning car — a shunt, a bullet —
sets it off immediately, which makes a burning wreck a weapon rather than
scenery. Wrecks smoke while they burn and traffic queues behind them.

### What the crashes feel like

Restitution for the struck car is deliberately over unity: a crash should
*throw* the other car, not nudge it. Impacts shed three separate debris
bursts, damage scales harder with closing speed, and **whatever you are
driving takes a third of the damage the traffic does** — so you can write off
two or three cars before yours goes, which makes ramming a tactic rather than
a trade.

The watch buzzes now, through one `wr_vibe()` gate so it can be silenced in
one place: a tap when a punch or a swing lands, a longer buzz for gunfire and
for taking a hit, and a three-part thump for a crash or an explosion nearby.

### Getting run over

A car that hits you on foot no longer simply kills you. It takes your legs
out: 8–26 damage by speed, a real ragdoll arc inheriting the car's heading,
bounces, and then you pick yourself up. There is a grace window after you
stand — without it a busy street knocks you straight back down as you rise,
which is not a hazard, it is a pit — and a car has to actually be coming at
you, so one pulling away beside you is harmless.

### Carjacking

Stealing an occupied car now hauls the driver out: he is thrown clear
tumbling, lands beside his own car, and runs. The player's arm swings with
it. Shooting at a car makes whoever is behind the wheel **floor it** — a
shot at a car should start a chase, not leave it trundling along at the
speed limit.

### The rolling stock

- **The motorcycle is a motorcycle.** It was a small car, which is exactly
  why it read as a toy: same body-and-cabin rig, just scaled down. It gets
  its own — two big wheels in line, tank, seat, forks, bars, headlight, and a
  rider sitting astride it in a helmet.
- **School bus**: long, tall, black flank stripe, window band and a stop arm.
- **Cybertruck**: the wedge, a long angular slab over the body and a light
  bar across the nose. At this scale the silhouette is the whole joke. Both
  are in the ordinary traffic rotation, so both are drivable.

### The player, and the streets

The player is drawn with detail nobody else gets — jacket panels, collar,
belt, hair, shoes — because he is on screen every frame and he is the thing
you are actually looking at. Eleven extra boxes for exactly one character.

The streets carry roughly double what they did: eighteen pedestrians,
thirteen cars, and **cyclists**, who ride faster than a walker, keep to the
road, and come off the bike hard when you clip them. Street furniture is
twice as dense, palms are half again as tall, and **pigeons** pick about the
pavement, the parks and the sand.

The shoreline has **surf**: a foam band on every sand tile that touches
water, breathing in and out on a slow cycle. One quad per shore tile, and
only shore tiles have one.

### Making dense traffic work

Doubling the traffic broke it, and the fixes are all rules real traffic has:

- **Never block the box.** A car that stops *inside* a junction sits through
  the other family's green and gridlocks the crossing. Once you are in the
  box you clear it, whatever the queue ahead says.
- **Nothing may sit still longer than a full light cycle.** A red lasts at
  most `LIGHT_CYCLE` frames, so anything stationary past that is deadlocked
  rather than obeying a signal, and gets a forced crawl to break the chain.
- **Progress, not motion, is the test for a wedged car.** Position is
  sampled every 256 frames — comfortably longer than a cycle — and a car
  that got nowhere is put back on the nearest centreline, or despawned if
  it has wandered somewhere there is no road to return to.
- **The lane offset had a sign bug** for a subset of the four headings, so
  those cars steered permanently away from their own lane and drove in slow
  circles, stalling the streets behind them. It is derived honestly now:
  the offset along the family's unit normal, projected onto the car's own
  left normal. Opposing traffic passes at 712 units of separation, up from
  266, with no contact.
- **The ambulance span on the spot.** When its path to a body was blocked it
  added 6000 to its heading *every frame* — a third of a revolution per
  frame. It turns a bounded amount now, only every few frames, and gives up
  on a body it cannot reach.

### Crashes

Crashes are physics now, not dice. Momentum is exchanged along the line
between the two cars, so a head-on stops both, a glancing blow deflects, and
a rear-ender shunts the car in front forward — all from one impulse rather
than three scripted cases. The tangential remainder knocks the nose round.

Damage comes from the *closing* speed along that line, so two cars
travelling together touching bumpers hurts nobody, and it fires **once per
contact** with a short cooldown. That last part is what was writing cars off
after a single prang: the old code applied damage every frame the two hulls
overlapped, so one crash was fifteen damage events. A full-speed ram now
takes about 30 of 100 bodywork, and it takes **four** to destroy a car.
Every real impact throws painted metal and glass off the point of impact.

Two things learned building it, both worth keeping:

- **The player's heading knock has to be clamped far tighter than an AI
  car's.** At AI deflection a single hit spun the player most of a right
  angle, which on the bridge put him over the kerb and wedged him off the
  deck. You should feel a crash, not lose the car to it.
- **A separation push must be refused if it would put a car somewhere it
  could not have driven.** Unchecked, a shunt posts cars through walls, and
  on the bridge it walked them straight off the deck.

**Wrecks stop driving.** A destroyed car used to carry right on — blackened,
smoking, and doing laps — because nothing checked. It is scenery now: it
never moves, it never steers, and traffic queues behind it. It stays in the
collision sweep with effectively infinite mass, so you can pile into a
wreck and it neither moves nor cares.

### Auto-aim

Guns snap to whichever person or vehicle sits nearest the middle of your
aim. *Smallest angular offset wins, not smallest distance* — so a crowd
doesn't steal your shot at the man you are actually pointing at. The cone is
about 28° each side, it ignores anything closer than a quarter tile or
further than seven, and it never fires backwards. Shotgun spread and
flamethrower hosing centre on the assisted bearing rather than the raw one.

### Traffic control

Junctions have traffic lights now: a pole at the pavement corner with two
lamps, the top one answering the x+y streets and the bottom the x−y streets.
One global cycle (`LIGHT_CYCLE`) drives both the lamps and the cars, so the
light a prop shows and the queue at it can never disagree. On a red, traffic
brakes to a stop line just short of the junction box — a car already in the
box clears it, same as the real rule — and a car-following pass caps every
car's speed against the vehicle ahead in its lane, which is what makes a
queue form behind the first car instead of a shunt. The player's car is in
that sweep too: traffic pulls up behind you at the lights. Crash damage got
a threshold in the same pass, because queueing traffic nudging bumpers was
writing off both cars; below walking pace nobody takes bodywork damage, and
above it the damage scales with closing speed, so deliberate ramming still
wrecks and light contact never does.

The ambulance ignores the lights — it is allowed to. When a body has settled
somewhere and no ambulance is already out, one is dispatched: a white van
with a red stripe, roof cross and strobing light that drives to the nearest
corpse, sits over it for a few seconds with the lights going, and the body
is gone when it pulls away. The blood pool stays.

### Life revoked, freedom revoked

Death and arrest were easy to miss — a vignette and a frozen frame. Both now
play a cinematic: the camera drops to a close-up on the body, holds, then
pulls up and away in a slow orbit while the caption sits mid-screen — LIFE
REVOKED in red, or FREEDOM REVOKED in blue when you are taken instead. The
world freezes underneath it, which turns the scene into a tableau: for an
arrest the nearest officer steps in right behind you, arms mid-reach, and
you are drawn kneeling with your hands cuffed behind your back rather than
as a corpse. Respawning takes a deliberate SELECT once the shot has played
out, so a mashed attack button cannot skip the moment.

### The graphics pass

- **Wheels are round.** They were thin boxes, which from the flank is exactly
  the two silly triangles they looked like. Each wheel is now a tread box for
  the silhouette plus `gfx_wheel_disc()`: an octagonal tyre with a lighter
  hub, standing in the vertical plane along the car's axis. Only the flank
  facing the camera gets discs — the far side is hidden by the body anyway —
  so it costs two polygons per visible wheel, full-detail cars only.
- **Buildings have windows.** Vertical glass mullions run down every exposed
  face; crossed with the storey bands that were already there, they read as a
  window grid for a handful of tall quads per face instead of a quad per
  window. Big roofs get a stair head and a hashed unit so towers stop
  reading as extruded rectangles from above.
- **Downtown.** The outskirts keep the low Miami sprawl, but the middle of
  the main island now climbs into a high-rise core — lot heights scale with
  how deep into the core the lot sits, topping out at nine storeys, so the
  skyline rises smoothly rather than a wall of towers starting at a radius.
- **Police light bars strobe properly**: two lobes flashing against each
  other, red one side and blue the other, swapping — not one box blinking.
- **Ragdolls fly.** A run-down now inherits half again your speed, launches
  higher and tumbles harder, which buys extra bounces on the way down.

### The gun shop door

Lots sit on a lattice, and the door was hardcoded to the +y face — so a shop
could spawn with its entrance pointing straight at the building next door,
sign and all, and no way in. `city_shop_door()` now scores all four faces by
how far a person can actually stand in front of them (probing outward for
lot collisions — cell class is the wrong question, since the inside of a
block is all CELL_BLD yet the courtyard gaps in it are walkable) and puts
the door on the clearest face. The renderer's sign, the door slab and the
entry test all call the same function, so they cannot drift apart. Measured
across six world seeds: 451 shops, zero with a blocked door.

### Mirror mode

The framing has always been biased for a left thumb: the character sits right
of centre, the camera yaws him across the frame, the stick rests bottom-left
and the zoom slider lives on the right edge. **Mirror**, in the pause menu,
flips all four at once — character left of centre, yaw negated, stick resting
bottom-right, slider on the left edge.

The point is one-handed play. With the watch on the left wrist and the right
hand doing everything, the thumb falls on the three buttons (steer and act)
and the index finger curls onto the near edge of the glass — which is the
left edge, exactly where the slider now is, with the whole road visible past
your finger because the character keeps clear of it on the other side.

The projection centre became a runtime variable for this, solved the same way
`PROJ_CY` already was, so the character lands on his pixel in either mode and
the world simply reflects around him.

The pause menu got a matching cleanup: pill switches (Mirror, Sound) toggle
directly on SELECT — a switch that needed an "editing" mode first was two
taps for a one-bit decision — and only Volume, an actual slider, keeps
edit-then-adjust. The Camera row is gone entirely, because the zoom slider
already lives on the game screen's edge.

### Odds and ends from the same pass

- **The centre-line dashes walked with you.** A band's anchor point is the
  player's perpendicular foot on the centreline — which slides along the
  street as you do. The dash grid was measured from that anchor, so the
  dashes were pinned to the player, not the road. They are snapped to
  absolute multiples of `DASH_STEP` along the street now (floored, so the
  grid doesn't jump crossing zero), and each dash checks the terrain under
  it — the street ends at the coast, and a centre line marching on across
  the beach rather gave the game away.
- **And some streets had no lines at all.** The dashes were drawn on the
  single nearest band per family — so only two streets in the whole city
  ever had markings, and crossing the middle of a block handed them to a
  different street, which is what looked like lines vanishing while you
  stood next to them. Every band in view gets its dashes now, skipped only
  past the perpendicular distance where a dash is sub-pixel anyway.
- **Then the dashes flickered in place.** Not z-fighting — the paint order
  of coplanar ground is fixed — but scan conversion: a dash is 36 world
  units wide, under a pixel a street away, so the rasteriser rounded it in
  and out of existence as the camera moved. Below a couple of pixels a dash
  now snaps to a minimum 2×2 block instead of vanishing.
- **The coastal ring road is finally visible.** `cell_at()` has classified it
  from the start — a two-tile annulus inside the shoreline, and traffic
  drives round it — but the renderer never painted it, because it is the one
  road that is not a straight band. Cars looked like they were doing laps of
  the beach on dirt. It is painted per tile, using the classifier's own
  radial test (`city_ring_road()`), which is exact rather than an
  approximation: unlike the diagonal streets, its classification really is
  tile-quantised. A first attempt drew it as tangent chords of the wobbled
  circle, which taught two lessons — the coast wobble's internal angle scale
  is half of `fsin()`'s, and no smooth curve can match a tile-quantised
  annulus anyway.

### The wedge flicker, and where it actually came from

The worst of the "polygons flickering and disappearing" had one cause with
three faces, and none of them were the painter sort.

- **Screen-crossing wedges that swung with the camera.** `gfx_project()`
  clamps screen coordinates to ±2000 because `Proj` stores them in an int16.
  That is harmless for a forearm; it is catastrophic for a street. A band
  passing *beside* the camera has corners whose true projection is x = ±10,000
  or more, and clamping a vertex bends both edges that meet at it — so the
  quad's fill swept across the middle of the screen as a giant wedge, and
  re-bent into a new shape every time the yaw changed. Instrumenting the span
  filler showed quads with corners like (−2000, 725) at depth 103 joined to
  (641, 11) at depth 11,940. Measured over a slow full rotation, the worst
  single-frame change was 29,412 of 45,600 pixels — two thirds of the screen
  snapping between frames.
- **Buildings vanishing as you drove past.** `draw_lot()` refused to draw if
  *any* of its eight corners was behind the near plane, so the whole building
  popped out the moment you got close, and popped back a frame later. Ground
  tiles under the camera did the same.
- **Roads on the open sea.** The two diagonal band families were drawn at full
  96-tile reach no matter what was underneath, which painted tarmac and kerb
  straight out over the water at every coast — the "water under the roads".
  The terrain classifier stops streets at the wobbled shoreline; the renderer
  didn't.

The fix is one mechanism, not three patches: `gfx_world_poly()` takes a
polygon in world space, clips it against the near plane in *camera* space
(where the interpolation is exact), projects without clamping — once nothing
is behind the eye the projection maps straight edges to straight edges, so
coordinates are large but honest — and then clips the projected polygon to a
guard box just past the screen, so the rasterizer only ever sees small
numbers. Ground tiles, blood quads, street bands and every building face go
through it. Lot walls stopped needing projected corners for backface culling
at all: they are axis-aligned planes, so a wall faces the camera exactly when
the eye is on its outer side, which is one compare against the eye's ground
position.

The band families are additionally clipped to their chord through each
island's circle, with the coastal wobble re-evaluated at each end of the
chord, so streets now end where `cell_at()` says they end. The highway is
exempt — it is the bridge.

Measured after: worst single-frame churn over the same rotation fell from
29,412 to 5,186 pixels (ordinary parallax); inverse-projecting every
road-coloured pixel back to the ground plane finds **zero** painted over open
sea (previously 3,144 across 16 headings at the coast); and street overdraw
at mid zoom dropped from 359k to 107k pixels per frame, because the wedges
had been repainting the screen several times over. The clamp in
`gfx_project()` stays, but nothing screen-sized goes through it any more.

### Rasteriser

`gfx_fill_poly()` used to divide once per edge per scanline to find the span
ends. A street band running to the horizon spans most of the screen, so that
was hundreds of divides per polygon and comfortably the largest single cost in
the frame. Edges are now set up once with a 16.16 slope, and walking down a
scanline is a multiply and a shift. Measured 18% faster on x86 — and x86 has a
fast divider, so the saving on a Cortex-M33, where `SDIV` costs far more
relative to a multiply, should be considerably larger.

Worth knowing if you tune the street rendering: the scanline walk dominates,
not the fill. Drawing pavement as two narrow kerb strips instead of one wide
band under the road overdraws about 15% fewer pixels but adds 14 tall polygons
per frame, and comes out slower overall. Fewer, larger polygons win here.

## Icons

The launcher icon and a packed 48x48 live in `resources/images/`, declared in
`package.json`. The 25x25 carries `"menuIcon": true`, which is what tells
PebbleOS to use it in the app list; the 48x48 is packed so it ships inside
the PBW rather than living outside the build, and is not drawn anywhere yet.

Resources come out of the 256 KB resource budget, not the 64 KB RAM image,
so they cost nothing against the ceiling described below.

## Open issues (read this first)

**`features` fails on the ambulance assertion (seeds 6, 11, 19) - and the
test, not the ambulance, is the prime suspect.**

Four separate changes were made to how the ambulance behaves: widening the
on-scene radius to 620, collecting on foot when stuck within three tiles,
making `ST_FLEE` non-terminal so it re-takes a call, and exempting an
ambulance on a call from the distance cull. Every one of them produced
**byte-identical** results on all three seeds: closest approach 4358, 1235,
1178, unchanged to the digit.

Four changes that must alter behaviour altering nothing means the measurement
is decoupled from what it claims to measure. The likely mechanism: the test
watches one entity slot and calls the body collected when that slot reads
`ENT_NONE` - but `parked_refresh()` now claims the first free slot every 32
frames, so the instant the body IS collected a parked car fills its slot and
the test never sees it empty. `closest` would be similarly measured against a
stale position.

**Next step: fix the test first.** Tag the body by identity rather than by
slot - record its coordinates and check whether any dead pedestrian remains
near them - then re-measure. Only if it still fails is there an ambulance bug
to find. All four behavioural changes above are kept; each is defensible on
its own merits, and none of them is known to be wrong.

The wider lesson, three times over in one session: **a change that moves
nothing is evidence about your instrument, not about your code.** It came up
with lane keeping (dead code behind an always-false gate), the flamethrower
(burning unreachable behind lethal impact damage), and here.

**`marine` fails on seed 6 only, and is NOT a regression.** Across five seeds
the current tree wins one, ties three and loses two, one of those at 91%.
The metric - percentage of frames on tarmac - is deterministic but chaotic:
the scripted driver swerves around whatever traffic is in front of it, and
each swerve changes its entire route, so the number moves thirty points on
changes that are neither better nor worse. It should assert *reaches the far
island having crossed open water*, which is what its name claims. Left as-is
so the next session can see the evidence.

The lesson from both, and from `edge.c` asserting exactly three cabinets:
**assert properties, not incidental measurements.** A test that encodes a
number breaks when the number moves and teaches you to edit the test.

## Seeing through walls, properly

The occlusion system - draw a building as a checkerboard when it stands
between the camera and the character - had been in for a while and did not
work indoors at all. Instrumenting the test as a funnel found it in one run:

```
lots drawn 216 | passed depth 42 | inside bbox 0 | stippled 0
```

Forty-two walls were correctly identified as being in front of the player,
and every one failed the bounding-box test. The box's top edge came only from
the roof polygon, and `by0` started at a sentinel of 30000 - so whenever the
roof was clipped away entirely, no pixel on screen could be inside the box.
That is exactly what happens when you stand close to a wall whose top is
above the view, which is the one situation where a wall certainly blocks the
camera. A discarded roof now means the box is open upward.

```
lots drawn 216 | passed depth 42 | inside bbox 42 | stippled 42
```

The depth margin came down from 340 to 120 units in the same pass - 340 was
tuned for a tower across the street and silently excluded anything close.

This is systemic, not an interior fix: any building you stand near has its
roof off screen. Outdoors it stays selective - 1.2% of drawn lots across 120
street views - because the real guard is the silhouette test, not the margin.

## The weapon plate: icons tried, words kept

Silhouettes were tried and reverted. At twenty pixels a pistol and a machine
gun are the same handful of dark rectangles, and guessing which is which is
worse than reading a short word. `draw_weapon_icon()` is left in the file,
unused, because the idea is sound at a larger size and the shapes are worth
keeping if the plate ever grows.

**The plate is gone entirely.** Solid, dithered at a half, dithered at three
quarters - every version was a box of furniture sitting on top of the game.
The name is drawn in bold black instead, five times over, a pixel out in each
direction. There is no stroke or outline in this graphics API, so weight has
to be built by hand, and a single-weight glyph vanishes the moment it crosses
a dark roof or a shadow.

A note from the round the icons were in: the box shrank to fit them and the
**tap area did not**, so a tap 26 pixels outside the visible plate silently
changed weapon. Hit areas and drawn areas are the same numbers now, and moved
back together when the box widened again.

## Cars stay where you leave them

Stepping out of a car handed it straight back to `update_car()`, which found
an empty vehicle sitting in a lane and drove it away - so the car you parked
at the kerb was gone when you turned round.

It goes into `ST_PARK` now: the same state the garage and seafront bays use.
A real vehicle, collidable, stealable, on fire if you left it burning, with
nobody driving it. Getting back in returns it to `ST_DRIVE`. The same applies
when the police haul you out of one.

## Rocket hits, and wrecks that fly

A rocket into a car is two events now, not one. The rocket **detonates where
it struck** - its own blast, damaging whatever is around it - and the car is
thrown separately: up, out, tumbling, alight and on a **short fuse** of
40-85 frames rather than the usual 70-220. It bounces off the road, shedding
speed, spin and a shower of parts on each touchdown, and goes up wherever it
comes to rest.

Measured: 155 units of height, fourteen touchdowns, 1,400 units downrange,
and 48 fresh pieces of wreckage on the frame it finally went.

Two things made this cheap. Cars already carry the `z`, `vx`, `vy` and `spin`
fields ragdolls use - nothing had ever put a car in the air before, so they
sat unused. And every vertical offset in `model_car()` is expressed relative
to one `RIDE` constant, so adding the throw height to that single term lifts
body, cabin, wheels and lights together.

Exploding cars now shed panels and glass in five directions rather than
flashing and sitting there.

One thing the test caught that the game gets right: the debris pool
saturates. A burning car emits embers every four frames, so by the time it
explodes all 48 slots are full and the count cannot rise. `gib_spawn()`
steals the shortest-lived slot when full, so the wreckage still appears - it
replaces embers rather than being dropped. The test counts *fresh* pieces
rather than total, which is what a shower actually looks like.

## Fire does not bleed

A man who is alight throws off embers, not blood - the wound is cauterised as
it is made. Burning victims spray sparks rather than blood on each hit, leave
ash rather than a pool when they die, and the ambulance that collects a
charred body leaves ash where it lay. Shooting someone still bleeds exactly
as before, which `nobleed_test.c` checks in the same run: twelve pools from a
shooting, zero from a burning.

## Walls going transparent in front of you

The occlusion test judged a building by the depth of its NEAREST ground
corner, which asks "does any part of this lot lie between camera and
character". That is true of any long wall running alongside you - it begins
behind you and ends in front - so walls you were looking straight at went
see-through. Using the farthest corner is the opposite mistake: a building
you stand beside always has one corner past you, and nothing was hidden at
all (measured: 0% of lots outdoors, and the indoor test failed outright).

It uses the **centre** of the footprint now. A wall squarely behind you
qualifies, one you are level with does not. Outdoors that is 0.4% of drawn
lots across 120 street views, and the indoor case still passes.

## Signals that vanished and never came back

Making gunfire and explosions break props had a consequence I did not think
through: a destroyed prop is deliberately never rebuilt - `props_refresh()`
keeps anything knocked loose where the player left it - and traffic signals
had 40 hit points. A single stray round took one down permanently, so
junctions quietly lost their signals over a session.

Anything that should still be there tomorrow has to be indestructible today.
Signal masts join bridge pylons, jetties and shop doors at 255.

## Fire, and two rules that fought each other

Setting people alight looked like nothing was happening, and the cause was a
flag doing two jobs. `PedDraw.burnt` was set from `fire != 0`, so a man was
painted charcoal the instant he caught - being set on fire looked like being
shot with a dark bullet. Charred is now `fire && dead`, alight is `fire &&
alive`, and a living victim is drawn standing inside a body of flame with a
lick above his head.

Fire spreads to cars now: a tongue of flame lights one exactly as a shot
does, and **any further hit on a burning car brings the explosion forward**
about half a second, so shooting a burning wreck times the blast instead of
wasting rounds. A collision with a burning car still sets it off on the spot.

Getting there meant untangling two rules I had written weeks apart that
directly contradicted each other:

- One said *any bullet hitting a burning car detonates it immediately*.
- The other said *a hit shortens the fuse by twelve frames*.

With both present the flamethrower blew up what it had just lit - the second
tongue detonated the fire the first tongue started - and no amount of staring
at the ignition code explained it, because the ignition code was correct. The
instant-detonation rule is gone; hastening is the rule for gunfire, and
immediate detonation belongs only to collisions.

A related guard: `vehicle_destroy()` now refuses to run on a car that is
already alight, whatever its state. A burning car may still be driving,
because its panicking driver is fleeing with it, so keying that guard on
`ST_DEAD` let the next hit fall straight through to the detonation.

**Explosions break scenery.** A grenade in a gun shop that leaves the glass
cabinets standing is not an explosion, it is a noise. Blasts now damage props
within 260 units, hardest at the centre.

**A contextual press no longer fires the gun.** Opening a door, taking a car
or leaving the shop clears the held-fire flag, so the press that let you out
of the gun shop no longer empties a magazine into the street as you go.

**The top bar is gone.** Dithering it still cluttered the readouts sitting on
it; the readouts are legible on their own. The weapon box lost a third of its
height, which was empty padding under the word.

## Automatic fire

SELECT was a single-click subscription, which only fires on release and has
no notion of holding - so a flamethrower could only be tapped. It is a raw
press/release subscription now, like the turn buttons already were, and the
**flamethrower and machine gun keep firing while the button is down**.
Everything else stays one press, one shot: a pistol that empties itself while
your thumb rests on the button is not a feature.

Measured: flame alight on 60 of 60 held frames with up to 15 tongues in the
air, out within seven frames of release, and a held pistol fires nothing.

## Carrying an arsenal

You keep everything you pick up. `Player.inv[]` holds one entry per weapon -
rounds for a gun, 255 for a melee weapon you simply own - and `g_p.ammo`
mirrors the one in your hands, written back when you switch. Tapping the
weapon box cycles to the next thing you are carrying; fists are always in the
cycle, so it can never strand you holding nothing. Picking up a second
shotgun adds its rounds to the first rather than replacing it. Dying loses
the lot.

Measured: three weapons collected and all three carried, the cycle visits
every one plus fists, and a shotgun's 24 rounds survive switching away and
back.

## Arrests need you hurt first

An officer in reach used to start cuffing you at full health, which is why
arrests felt arbitrary - a cop brushed past and the meter started filling.
The meter only builds below 45 health now. They have to beat you down first,
which is what the nightstick is for, and it makes the fight legible: while
you are healthy a cop is a threat to your health, not a threat to your
freedom.

Measured: arrest meter stays at 0 through 90 frames of contact at full
health, and builds to 45 in 120 frames once hurt.

## Doors work the same in both directions

You press to get in, so you press to get out. Walking through the wall used
to dump you on the street, which meant entering and leaving worked by
different rules - and that rescue only existed because the doorway had a hole
in it, which it no longer does. If something throws you clear of the room now
you are put back inside rather than silently ending the visit, because
outside the room there is no ground to stand on.

## Bullets you can see

A three-unit dot travelling 55 units a frame is invisible in flight and gives
no clue where it landed. Rounds are larger and brighter, and carry a dimmer
tail one frame behind, so a shot reads as a streak toward a point rather than
a flicker.

## Audio: gain staging and retrigger limits

Two problems, one measurable and one not.

**The mixer had no gain staging.** `osc()` returns about +/-127 and voice
amplitude goes to 255, so a single voice at full amplitude already sat at the
output ceiling - and a pistol is two voices, a shotgun three, an explosion
three. Everything is scaled to 45% before summing now, and what still exceeds
a knee at 96 is squashed rather than sliced: a hard clamp turns a loud note
into a buzz, while bending the top of the waveform just sounds loud.

Measured, firing shotgun, pistol, explosion and impacts together over 120
frames: **44 samples at the rails out of 38,400, down to 0**, and peak level
127 (railed) down to 108. Worth being precise about that, because "every
gunshot was clipping" was my first description of it and the measurement says
otherwise - it was 0.1% of samples, not all of them. The harshness was real
but smaller than claimed, and the test now guards it.

**The chirping was retriggering.** Nothing stopped the same sound starting
again the next frame, so a burst of fire, a shower of glass or a car
grinding along a wall fired dozens of copies a second, each beginning at full
amplitude. Every sound now has a minimum gap before it can play again - five
frames for impacts, twenty for explosions and the bust sting. That is most of
what made it sound busy, and it also keeps several copies of one sound from
stacking up in the mixer.

## Burning

Hit someone with the flamethrower and they catch. They run - badly, veering,
not toward anything - trailing flame, and the burn takes them down over
several seconds. What is left stays **charred**: a flat charcoal palette over
the whole rig instead of the usual corpse.

The tuning matters more than the code here. Flame impact damage is **6, not
30**. At 30 a point-blank pull killed instantly and the burning never played
at all - the fire I had written was unreachable behind a weapon that had
already won. The burn does the killing now, which is the entire point of
setting someone alight rather than shooting them.

The charred state costs no extra field: `fire` is held at 1 after death as
the marker, since a corpse has no fuse left to count down.

## The UI, and a second pair of legs

- **The HUD bars are dithered, not filled.** PebbleOS has no alpha channel,
  so "semi-transparent" means drawing every other scanline: the world shows
  through and the readouts still have something to sit on.
- **Tapping the weapon box** puts the gun away and brings your fists up;
  tapping again takes it back out with the same ammunition. Full weapon
  cycling needs a real inventory - per-weapon ammunition rather than one
  slot - which is a bigger change than a tap handler.
- **The round count is gone from the box.** The bar along the top already
  carries it, and two readouts of the same number is one too many.
- **The gun shop's front wall is solid.** It was split around a doorway gap,
  and through that gap you could see out of the interior to open water -
  because outside the room there is no world at all, only the shop's own
  scenery. The door and the mat in front of it are the way out; the wall has
  no hole in it.
- **The player had two pairs of legs.** `model_ped()` draws a full rig, and
  the detail pass drew thigh, shin and shoe again on top in a lighter
  palette. That is what the pale limbs were, and any phase difference between
  the two pairs reads as one set walking backwards. The detail pass keeps the
  shoes and drops the duplicate legs. A detail pass must ADD to the base rig,
  never restate it.

## Pool sizes and the 27 bytes

The prop-dimension fix (`int8_t` to `int16_t`) and the bullet-versus-prop
sweep pushed the image to 65,562 bytes against a hard ceiling of 65,535 - a
failed build by **27 bytes**. Worth remembering how little headroom this
platform gives: a table type change and one loop cost more than the margin.

Trimmed from measured peaks twice since. Current sizes, with the peak each
was measured at in brackets: lots 48 (30), entities 52 (39), props 48 (40),
draw list 168 (computed worst case 148), debris 48 (45), blood 22. Static RAM
is 8,446 bytes.

`update.sh` prints the footprint every build and warns within 1 KB of the
ceiling. Expect to do this again: text grows with every feature, and the
ceiling does not move. When the pools have no slack left, the next lever is
code size - the murals, the setback crowns and the pier generator are all
self-contained enough to compile out behind a flag if it comes to that.

## Fixed this round

- **Traffic signals were drawn below ground, and palms were stumps.** `PROPD`
  is `int8_t`, so a 250-unit signal mast wrapped to -6 and a 196-unit palm to
  -60. Nothing warns you: the values are in range for the declared type, they
  are just not the values you wrote. The table is `int16_t` now.
- **Gunfire could not break anything.** Melee smashed fittings and cars
  flattened them, but bullets passed straight through - so the glass cabinets
  in the gun shop, the one place built to be shot up, could only be opened
  with a bat. Slugs now break what they hit and stop; rockets and fire carry
  on through.
- **Seafront parking was out on the sand.** Cars on loose dune looked
  abandoned rather than parked. The bays are inland of the carriageway now,
  against the kerb they serve.

## Open regression: traffic (read this first)

Tonight's session left a regression I did not finish isolating, and the
honest state is worth more than a tidy summary.

`make marine` **fails**: the island-to-island drive is at 53% on-road, having
been 100% earlier in the session. It still completes; it just spends much of
the trip off the tarmac. Everything else in the suite passes.

Two causes found so far, one fixed and one not:

**Lane keeping was dead code, and enabling it broke the streets.** The gate on
that block is `!on_bridge(...)`, and `on_bridge()` used to return true for
every point in the world - so the correction never ran once, and every
measurement that appeared to validate it was measuring nothing at all.
Bounding the causeway to its real segment made `on_bridge()` honest, which
switched lane keeping on for the first time, with the sign bug already
documented in its own comment: `leftlat` has the wrong sign for at least some
of the four street headings, so those cars steer away from their lane forever
and drive in slow circles. It is disabled again, with the calculation left in
place and a note that it must be re-enabled only after a test asserts the
sign on all four headings.

That took the drive from 33% back to 53%, so something else remains.

**A failed attempt at smooth car turning, reverted but not cleanly.** Steering
toward a *relative* target every frame is a spin, not a turn - the target
moves with the heading - which is the same bug fixed in the ambulance long
ago. Reverting it did not restore the numbers, so either the revert is not
byte-identical or it interacts with something else changed the same evening.

**Where to start next time:** diff `update_car()` against the previous zip and
revert hunk by hunk, re-measuring `marine` and `band` after each. Note that
the `band` diagnostic printing "0.8 tiles in 300 frames" is probably an
artefact of that harness not advancing the light clock, so a car sits at a
red for the whole window - confirm that before chasing it as a bug.

**Smooth turning, done properly**, needs a car to choose a target heading
once, store it in a field on `Ent`, and steer toward that fixed value until
it arrives. Not a per-frame relative nudge.

## The gun shop, tightened up

Seven tiles by five with three cabinets in it was a warehouse. It is five by
four now, with **five cabinets**, **four guns racked on each side wall** at
chest height - knock one off and it drops a working weapon, so the walls are
stock rather than scenery - and **two men behind the counter**, one at each
end, facing the door so they are looking at you as you walk in. They are
pedestrians like any other, which means they can be robbed, shot and mourned,
and it means the shop has witnesses.

The layout is asserted rather than eyeballed: `shoplayout_test.c` walks in
and counts two clerks both inside the room, five cabinets, eight racked guns
and exactly one door.

And a correction to the previous round, which is the more useful part of this
entry. I "fixed" the exit mat by moving it from the front wall to the back,
on the assumption that the original was wrong. It was not: the gap in the
wall really is in the **front** wall, at `ROOM_OY + ROOM_H`, which is plain
from `city_set_interior()` where the two front wall lots are the pair split
around the doorway. So my fix moved the only exit to a stretch of solid
brick.

The bug I was actually chasing was never the mat. It was that *walking*
through the gap put you outside the room, where the interior has no ground -
and that is handled by the recovery in `game_step()`, which returns you to
the street however you leave. Both are now correct, and the unit test stands
in the real doorway rather than the imaginary one.

Two tests had to stop hardcoding numbers they should have been deriving:
`edge.c` asserted exactly three cabinets, and the armour test stood at the
wrong wall. A test that encodes a constant instead of a property breaks every
time the constant moves, and teaches you to edit the test rather than read
it.

## Grenades

A grenade is a slug with a fuse. It is lobbed slowly, **bounces off walls
rather than detonating on them**, and goes off when the fuse runs down
wherever it happens to be lying - about a second and a half, which is long
enough to get clear of your own throw or to regret it. That bounce is the
whole appeal: it puts an explosion somewhere you cannot shoot.

It is aimed on the **raw** heading, deliberately. Auto-aim is for pointing a
barrel; snapping a thrown grenade onto whoever happens to be nearest would
take the throw away from you.

Eight to a pickup, stocked in the gun shop cabinets alongside the vests, and
drawn as a dark tumbling body with a fuse spark that blinks faster as the
fuse burns down - the tell that it is about to go.

Two mistakes worth recording, both caught by the test rather than by reading
the code:

- The grenade branch went **inside** the pistol/shotgun/MG branch, which a
  grenade never enters. It was unreachable, and had it been reached it would
  have taken two rounds off the count for one throw. Weapons that behave
  differently need their own top-level branch, not a nested special case.
- At 80 damage it left a pedestrian standing on 20 health. A grenade landing
  beside someone should kill them; it is 115 now.

## Armour is a second life bar

A vest is spent before any health is, so it belongs **on** the health bar,
not beside it: blue laid over green on the same track, draining away to
reveal the green underneath. It used to be a separate strip under the weapon
name, which meant reading two gauges to answer one question - how much can I
take before I go down.

A dark seam marks the edge of the vest, so full armour over full health still
reads as two things rather than one blue bar.

## Body armour

A vest is a layer that takes damage before you do. Pick one up and the first
hundred points of whatever is coming hits the armour instead of your health;
it does not make you invulnerable, it buys you a fight. Two ways to get one:
smash the cabinet in a gun shop that holds the rocket launcher - the armour
is behind the same glass as the best gun - or **jack a patrol car**, which is
the only source that does not need a shop and makes taking a cop car worth
the extra heat.

Every point of damage the player takes now goes through one function,
`player_absorb()`, because there are eight damage sites across two files and
armour that is honoured at six of them is worse than no armour at all.

Which produced the best bug of the session. I routed those eight sites
through the new function with a regular expression - and the expression
matched the line *inside* `player_absorb()` itself, the one place that
actually touches health. The function called itself, forever. It hung on the
second unit test, before any output had been flushed, so it looked like a
hang with no cause. There is now a comment on that line explaining why it
must never be rewritten.

## The gun shop had no way out

Walking out of the shop put you in the ocean. The reason turned out to be a
disagreement about which wall the door is in: the exit mat tested the **back**
wall of the room, while the gap in the wall - the only actual way out - is in
the **front** one. So the mat sat at the far end of the room against solid
brick, pressing SELECT in the doorway did nothing, and walking through the
gap left you outside the interior, where there is no ground at all.

The mat is in the doorway now, there is a **door** to walk up to and press,
and crossing the room's walls by any means - shoved by a ragdoll, thrown by
an explosion - returns you to the street rather than the sea.

## What the 128 KB actually covers

I got this wrong in both directions, so here is the rule, from Pebble
directly:

- **Packed image: 128 KB** on emery and gabbro since PebbleOS 4.33.2. This is
  the total on-flash bundle, and it was raised so that a large relocation
  table - big arrays of global pointers - can use the space above 64 KB.
- **Virtual size: still 65,535 bytes.** Code plus data plus bss, as loaded
  into RAM. The firmware stores this in 16-bit fields, so it has not moved
  and is not going to.

Two different numbers. Only one of them changed, and it is not the one that
governs whether this game builds.

The changelog line - *"emery/gabbro apps can now use the full 128 KB binary
limit, was wrongly capped at 64 KB"* - reads like the ceiling is gone. It
isn't. Notably, the clearer error message about `virtual_size` being a
`uint16_t` was added by that same change.

What I got wrong, twice: first I treated 64 KB as a property of the hardware,
when the SDK's own report printing `N / 128.0KB` alongside a hard failure at
65,535 should have prompted a question. Then I read the changelog as lifting
it and restored every pool, which would have overshot by several kilobytes.
**When a report and a limit disagree, find out which one is authoritative
before building on either.**

## Two duplicated primitives, nearly a kilobyte

Hunting for the ~100 bytes needed to fit found something better.
`gfx_part()` - a box - and `gfx_taper()` - a frustum - were near-identical
copies: same projection, same four side quads, same cap, differing only in
whether the top corners were scaled. A box is a frustum with no taper, so
`gfx_part()` is now a one-line call into `gfx_taper()`.

That is **695 bytes of text**, on a routine called 134 times across the
models, with no visible change at all. Another 144 bytes came from trimming
the lot and blood pools, both still well above their measured peaks.

Worth remembering when the ceiling next bites: look for duplicated code
before cutting features. The murals were removed to save 2.8 KB, and a
sixth of that was sitting in two copies of one function.

## Open: `systems` traffic and SWAT (read before touching either)

`make systems` fails two assertions - "traffic keeps moving" (8 of 19 cars
made progress) and "air support deploys" (no SWAT). **Four unrelated changes
moved neither number by a single digit**: capping parked cars at 6, reserving
eight entity slots for dispatch, dropping the garage rate from 26% to 18%,
and shortening the bridge from T1=735 to 700.

That is the same signature as the ambulance: a measurement that does not
respond to the thing it claims to measure. `systems.c` reseeds and rebuilds
the world for each section, so whatever it is observing is probably decided
before any of these take effect. **Diagnose the harness before changing more
game code** - the four changes above are each defensible on their own, and
none of them is known to have caused this.

## The bridge that ended in the sea

The deck stopped over open water on the far side - a road that simply ends,
which is what the screenshot showed. The cause was not the bridge: **island B
has a bite taken out of it on that side.** The coast wobble puts real water
inside its nominal sand radius, so a span sized by radius arithmetic lands in
the sea.

The fix was to stop calculating and probe: walk the line, sample the terrain
400 units to the SIDE so the deck's own tiles cannot answer, and find where
solid ground actually resumes. That is 705, against a nominal 645. `T1` is
735 now and the deck reaches ground.

Worth keeping as a habit: **for anything the coast wobble touches, measure the
terrain rather than deriving it from the radius constants.**

## More garages

Two per map, which is what the rendered map showed, is not enough for a
system the whole wanted mechanic is built around. Garages were 12% of lots
but only blocks above a certain size can hold one. They are 18% now, which
measures out at about five per map. 26% was tried and backed off: a garage is
a solid block traffic has to route around.

## The bridge only spans the water now

The island centres are 69 tiles apart and the water between them is **ten**.
The causeway ran the whole 69 - so **85% of the deck was built across dry
city**, overriding every street it crossed. That is the real answer to why
the highway never joined the road network: it was not passing those streets,
it was replacing them.

It now runs from `BRIDGE_T0` to `BRIDGE_T1` - measured positions a little
inside each shoreline, giving about three tiles of ramp at each end - and
outside that span the ordinary streets are simply the streets. `on_bridge()`
answers false there, so nothing overrides anything.

**And it arches.** `bridge_height()` is a parabola: zero at both shores,
150 units at mid-span, computed from position rather than stored. Beside the
deck, where the arch is high enough, the surface stays water - so a boat
sails past the piers and under the span.

The first attempt returned water for *any* raised point, which punched a hole
straight through the middle of the carriageway. The deck is the one place a
car must always be able to drive; the water belongs beside it, not under it.

Two test lessons from this one:

- `arch_test.c` walked the span using the same 0-1024 parameter the
  constants use, and compared it against the `along` value `on_bridge()`
  reports - **which is on a different scale**. It read a continuous deck as
  having a gap in it. Walking by real distance and using the reported value
  fixed the test, not the game.
- `marine.c` asserted a percentage of frames on tarmac. With the bridge
  shortened, most of that drive is ordinary streets, where a blind scripted
  driver clips kerbs - so the number measures the harness's driving, not the
  road. It now asserts what it is named for: you reach the far island, and
  part of the trip is over open water.

## The causeway was an infinite road

The screenshot that started this: a grey deck with a solid yellow line
running dead straight to the horizon, open water either side, and a boat
sitting on the tarmac. One cause, several symptoms.

**`on_bridge()` returned true for every point in the world.** It clamped the
projection to the ends of the island-to-island segment and returned true
regardless, so it was really "distance to the causeway axis" with every
caller expected to check that distance itself. Past either island the nearest
point on the segment became that island's *centre*, so a circular blob of
highway appeared around it.

**And the renderer painted the deck as an infinite line.** The band was
centred on the player's own projection onto the axis and drawn 96 tiles in
both directions, bounded by nothing. Past the islands that paint lay over
water that the classifier still called water - which is why boats appeared
to drive on the road, and why a car ran out of driveable deck a long way
before it ran out of visible deck.

Both are bounded now. `on_bridge()` answers false outside the segment, and
the deck is painted as one band from island centre to island centre. Painted
and driveable are the same thing everywhere along it.

Measured in `deck_test.c`: zero highway claims past the far island, 35 deck
points sampled and **none passable to a boat**, and across 2,278 points over
the whole span, **nothing painted as deck that cannot be driven**.

Still outstanding on the highway, and the reason this is not finished: it
still *overrides* the street grid rather than joining it. Crossings have no
junction geometry, no stop lines and no give-way, because the road test wins
over everything else in `cell_at_point()`. That is the next piece.

## Parked cars are real cars

The garages were drawn with decorative car-shaped boxes. That is a poor
trade: they look like cars, so of course you try to steal one. They are
ordinary vehicles now, carrying a new `ST_PARK` state - same models, same
bodywork, same collisions, same fires and explosions - with no driver and no
intention of going anywhere. You can steal one, ram one, shoot one, set one
alight.

Three details that make it work:

- **The bays stopped blocking.** They used to be solid, to give the
  decorative cars presence. With real vehicles the cars block by being
  vehicles, and a bay you have just driven out of is exactly as passable as
  it looks - an emptied bay that still blocked would be an invisible wall in
  the one place you had just proved you could drive.
- **Parked cars are not traffic.** They are excluded from the headcount
  dispatch uses, so filling a car park never starves the streets of moving
  cars. Measured: 4 parked and 13 moving at once inside a garage.
- **A refilled bay stays empty if you emptied it.** The pass only fills bays
  with nothing already in them, so the car park does not quietly restock
  behind you.

**The seafront has parking too**: bays on the sand side of the coastal ring
road, cars nosed in toward the sea, and a **parking meter** on the kerb side
of each. They are derived from the ring road's own geometry - the same
arc-walking the road markings use - so they sit square to the kerb wherever
the coastline wobbles, rather than being placed by hand per island.

Measured across ten seafront positions: 20 bays, all of them on sand, all
with a car and a meter.

## This round of play notes

Fixed from playtesting:

- **Cops were a firing squad.** Five units converging and all firing at once
  is not difficulty. At most **two officers shoot in any frame** now, foot
  cops fire on a 52-frame cadence rather than 30, and SWAT machine guns
  dropped from 9 frames between rounds to 18 with damage cut from 26 to 18.
  You still get pinned down; you no longer get deleted five seconds after
  the helicopter arrives.
- **Cars detonated with no warning.** `vehicle_destroy()` blew them up in
  the same frame the bodywork ran out, which made the fire system I built
  almost unreachable. A written-off car now always **catches fire first** and
  burns down to the explosion, and if it was yours you are thrown clear on
  foot while it burns. Anything touching a burning car still goes at once.
- **You could die of a fender-bender.** While you are in a car, crash damage
  goes to the bodywork and not to you: the only thing that can kill you in a
  vehicle is the vehicle going up.
- **Ammunition doubled** across every weapon, and shop stock with it.
- **A weapon readout** sits in the corner opposite the zoom slider, with the
  round count going red at four or fewer. It mirrors with the rest of the
  layout, so it is never under the thumb working the camera.
- **Crosswalk stripes** were 22 units wide and shouted louder than the road;
  they are 12 now. They were also culled at five tiles while every other
  marking ran to nine - an inconsistency, not a distance worth having.
- **Barricades left the beach.** The rule was "land with water beside it",
  which is the definition of a shoreline. A beach is obviously the edge of
  the land and needs no fencing. They now appear only where **tarmac** stops
  over water, which is the causeway kerb.

## Running out of image, and what gets cut

Four ceiling failures in five rounds. The pools are now about 25% above their
measured peaks - lots 40 against a peak of 30, entities 48 against 39, props
44 against 40, draw list 156 against a computed worst case of 140 - and there
is very little left to take from data without risking an overflow in play.

So the next lever is code, and it is now a switch. `WR_LEAN` drops the three
largest blocks of purely decorative rendering:

| switch | what goes |
|---|---|
| `DECOR_MURALS` | the Mirror app wall ads |
| `DECOR_SETBACKS` | tower crowns and beacon masts |
| `DECOR_ROOFTOPS` | stair heads and units on flat roofs |

Together they are worth about 2.8 KB of host text. They are compile-time
constants rather than `#ifdef` blocks, so the code still has to compile and
stay correct - the optimiser removes the bodies. **`WR_LEAN` is on by default
in the wscript**; take it out when there is room again.

`update.sh` now reports the actual image size on a failed build - text, data,
bss and how many bytes over 65,535 - by finding `arm-none-eabi-size` inside
the SDK rather than expecting it on PATH. The previous three trims were done
without ever seeing that number.

## The 64 KB wall

The first real SDK build compiled and linked every file without a single
error - and then died on the last step, injecting metadata:

```
Total footprint in RAM:  67365 bytes / 128.0KB
struct.error: 'H' format requires 0 <= number <= 65535
```

A Pebble app header stores the loaded image size in a **sixteen-bit** field.
So the real ceiling is not the 128 KB heap this file has been quoting all
along - it is 65,535 bytes of text + data + bss, and the build was 1,830
over it.

The fix was to size the pools from measurement rather than optimism. Peaks
over 9,000 frames of five-star play, and over the whole map across six world
seeds:

| pool | was | peak | now |
|---|---|---|---|
| lot cache | 150 | 30 | 64 |
| entities | 80 | 39 | 56 |
| props | 84 | 40 | 60 |
| draw list | 300 | 58 | 192 |
| debris | 52 | 45 | 48 |

That returns about 4.6 KB of bss and lands the image near 62.5 KB. Nothing
about how the game plays changes: every pool still sits comfortably above
its measured peak.

One thing worth recording. The draw list is the exception to "size it from
the peak" - its observed peak is 58, but `systems.c` computes a worst case
of 164 from the pool sizes, and the first attempt set the cap to 160. The
test caught the four-item overflow instantly. An overflowing draw list does
not crash; it silently stops drawing things. The test now checks
`wr_draw_capacity()` rather than a number copied into the test, so the two
cannot drift apart again.

## Measured budgets

Compiled `-Os` for host x86 as a proxy (ARM Thumb-2 lands similar or smaller):

```
text  16.7 KB     bss  6.9 KB
```

Game state in detail:

```
city grid       4,608 B     (48x48 tiles, 2 bytes each)
entities          600 B     (30 slots)
blood/bullets/pickups 640 B
player             40 B
draw list         880 B
                -------
                ~6.8 KB
```

So roughly 25 KB of the 128 KB emery budget, leaving a lot of room. Watch the
`Free RAM` line in `pebble build` output — that's the number that kills
projects like this.

Render cost, profiled over 1,500 frames of a five-star chase
(`-DWR_PROFILE` exposes `wr_prof_polys` / `wr_prof_pixels` / `wr_prof_items`):

```
avg    215 polygons     98,397 px    62.8 sorted items
worst  471 polygons    150,529 px    (screen is 45,600 px)
overdraw 2.16x
```

That is roughly triple the geometry of the version before this pass — window
grids, roof furniture, downtown towers, garage interiors with parked cars,
overhead street lights, denser traffic and denser street furniture — for
about 1.6x the host render time, because the added polygons are small and
the cost was always the long street bands. Around 8 ms of a 40 ms frame at
240 MHz.

Lower again — the default zoom now starts mid-slider at a shallower pitch, so
less of the city is on screen at once. Pull the slider out and it climbs back
toward the previous figures.

Up again with street furniture and lot variety in the mix, but overdraw is
*down* — merged lots waste far fewer pixels than per-tile boxes did. Worst case
is around 5 ms of a 40 ms frame at 240 MHz.

Static RAM went **down** despite everything added, because deleting the stored
city grid freed more than the lots, props and particles cost:

```
text  38.5 KB      bss  7.3 KB      (of a 128 KB budget)
```

```
```

Against a 45,600-pixel screen and a 40 ms frame budget at 240 MHz, that is
comfortable. The dominant cost is the per-scanline divide in `fill_poly`; if
you need headroom later, hoist an x-slope per edge and step it instead of
dividing per row. That single change should roughly halve rasterizer time.

## Files

```
src/c/wr.h          types, tuning constants, fixed-point helpers
src/c/wr_math.c     Q10 sine table, xorshift PRNG
src/c/wr_city.c     procedural city grid, collision
src/c/wr_render.c   projection, polygon fill, painter sort, palette, HUD
src/c/wr_ent.c      spawning, pedestrian / traffic / police AI
src/c/wr_game.c     player movement, weapons, wanted level, escape
src/c/wr_audio.c    priority gate over the Speaker API
src/c/main.c        window, frame timer, buttons, touch
```

## Verified

Compiles clean under `-Wall -Wextra`. Run headless against a stub SDK under
ASan and UBSan for tens of thousands of frames with no diagnostics. The crime →
police → escape loop was measured end to end: nine vehicular kills produced
five stars and nine dispatched units, and a clean run brought it back to zero
in 61 seconds.

## Not done yet

- Foot cops don't path around buildings; they steer straight at you and slide
  along walls. Fine at these distances, visibly dumb in a dead end.
