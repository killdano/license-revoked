# Handoff: License Revoked

A GTA 1-style open-world game for the Pebble Time 2 (emery), written in C
against the Pebble SDK. ~8,300 lines across `src/c/`. It runs, it is fun,
and as of September 2026 it has an economy: money off bodies, a strip club,
a gun store that sells and calls the police, and taxi fares.

Read the **September 2026** chapter at the top of `README.md` first. It
describes the game as it is; the rest of the README is the original
engineering log and still worth reading for the bugs it records.

---

## The size ceiling, still

**`text + data + bss` must be under 65,535 bytes.** The firmware stores it in
a 16-bit field; the 128 KB figure PebbleOS 4.33.2 quotes is the packed
image, not this. Going over fails at the last build step with
`struct.error: 'H' format requires 0 <= number <= 65535`.

```sh
find ~/.local/share/pebble-sdk -name arm-none-eabi-size    # not on PATH
<that> build/emery/pebble-app.elf
```

`update.sh` prints this and warns within 1 KB.

**Current state, measured with a modern GCC (the SDK's is roughly 9%
bigger): 60,090.** The as-received build measured 59,629 the same way with
the owner reporting 600–800 real bytes of margin, so the shipped
configuration should land a few hundred bytes under. **Build it and read
the number.** If it is over:

1. `-DWR_TAXI=0` in `wscript` — about 700 bytes.
2. `-flto` — saved 750 on a host link; untested against the SDK's linker.
3. Player knockdown (`knockdown_check()` and the arc in `game_step()`),
   about 500; grenades (`B_GRENADE`), about 300.

`-DWR_BOATS=1` brings the boats back for about 2.4 KB. It will not fit with
everything else; the owner chose the club, the store and the taxi over them.

### Where the bytes are

```
wr_render.o  ~14.2 KB   projection, rasteriser, roads, lots, HUD
wr_ent.o     ~11.3 KB   driver, police, helicopter, physics, particles
wr_game.o     ~7.9 KB   player, weapons, wanted level, bullets
wr_city.o     ~7.5 KB   roads, lots, props
wr_model.o    ~5.9 KB   part tables and the ped/car rigs
wr_inside.o   ~4.3 KB   money, club, store, taxi
wr_audio.o    ~2.3 KB   synth
```

bss is 7,124: `g_ent` (44×36), `g_prop`, `s_item`, `g_gib`, `g_lot`. All
pools are at their measured peaks. Police may evict a distant civilian or
corpse when the table is full (`free_slot_evict()`), so `MAX_ENT` is safe
at 44; the helicopter always gets the last slot.

### How to find more

Every `gfx_part()`/`gfx_taper()` call site costs 40–60 bytes of argument
setup. Anything static belongs in a `PartRec` table (`wr_gfx.h`) — that is
where the last 3 KB came from. The remaining hand-written calls are the
animated ones (limbs, doors, rotors) and the car body/cabin, which take
palette colours. Duplicated blocks are the other place: the crash physics
was two mirrored copies, the boat drivers were two functions, police
bullets were spawned in three places.

---

## Build and test

```sh
cd /mnt/c/dev/license-revoked && pebble build      # WSL2 Ubuntu
bash update.sh                                     # unzip+build+open folder
```

`pebble-tool` from PyPI via `uv`, pinned to Python 3.13.

**The host harness is the development loop.** `tools/hostcheck/` builds
the game against a stub `pebble.h` with host gcc.

```sh
cd tools/hostcheck
make check          # the gating suite (now includes air, garage, inside, taxi)
make <target>       # one harness
make bench          # render cost per frame (0.11 ms on a host)
make fuzz           # 150k frames of random input under ASan/UBSan
```

New tools, all in `tools/hostcheck/` and built by hand
(`gcc -std=c11 -O2 -I. -I../../src/c $(ls ../../src/c/*.c | grep -v main.c) stubs.c <tool>.c`):

- `snap.c` — real frames to PPM. You can *see* the game. Use it before
  touching anything visual.
- `worldmap.c` — the classifier top-down, junctions marked.
- `traffic_trace.c` — every vehicle's path for a minute. Spirals, beach
  excursions and lane errors are obvious in it.
- `sounddump.c` — every effect and generator to WAV.
- `-DWR_TRACE` with `wr_trace_ent = <index>` prints the driver's state per
  frame, including what capped its speed.

### The trap, still

**A change that produces byte-identical output is evidence about your
instrument, not your code.** This project's history is full of it, and
this round found three more: the `systems` traffic assertion counted parked
cars as stalled traffic; the `features` ambulance assertion checked for an
empty slot that a spawn refilled in the same step; and the SWAT failure
turned out to be the entity table being full of corpses. In every case the
game code was fine and hours could have gone into "fixing" it. Assert
properties, and when a number will not move, look at the test.

---

## Architecture

### Nothing about the world is stored

`cell_at_point()` is a pure function of position. Roads are described once
by `road_here()`; everything reads roads through it. Do not introduce a
second road test anywhere — that is exactly what produced kerbs across
carriageways and crosswalks on straight roads.

`road_point()` steps along a road, following the ring's wobbled arc.
Use it for any probe ahead of a vehicle; a straight probe leaves the ring
on a bend.

### The driver

`drive_along()` in `wr_ent.c` — pure pursuit with a lane offset and a
one-shot junction decision kept in `Ent.wep`/`vx`/`vy`/`vz` while it
executes. `Ent.spin` is the stall clock on a live vehicle (only a wreck
tumbles); `Ent.aux` is per-type scratch (fare flag, indoor role). Do not
use `anim` or `door` for anything: `ent_update()` counts them down every
frame — that is the bug that broke the old stall recovery and, briefly, the
ambulance.

### Interiors

`wr_inside.c`. One room builder from a `Fit` table per venue, a door prop
in the front wall, the outside world reset on the way in and out, and
per-frame house rules. `Inside g_in` holds everything: the meter, the
level, who is walking you out, the alarm, per-venue bans and stolen stock
(eight venues, hashed from the door position). Indoor people carry a role
in the low byte of `Ent.aux`; a dancer's clothing level is bits 8–9 and a
patron she is busy with is in bits 10–15. Money gibs carry `GIB_CASH`
(become pickups) or `GIB_BILL` (pay when they land).

### Light

`gfx_shade(normal, bright, dark)` — one light riding the camera. Buildings
and every box use it. If something looks flat, it is not going through it.

---

## Traps that have already cost time

- **`>>` on negatives rounds toward negative infinity.** `fmul()` for
  movement; `* 4` not `<< 2` on anything that can be negative (UBSan).
- **`road_here()` returns up to four roads.** Arrays are `Road r[4]`.
- **`PROP_HP`, `PROPD`, `PROPC`, `DEBRIS`, `PROP_OFF` all index by prop kind.**
  Adding a prop means adding to all of them; `PROP_OFF` needs its
  cumulative count.
- **Adding a weapon** means `WEP_RANGE`, `WEP_DMG`, `WEP_COOLDOWN`,
  `WEPC`, `WNAME`, `WEPDIM`, `WEPBODY`, and `Player.inv[]` grows with it.
- **The map is identical on every seed.** `hash2()` has no seed. Thirty
  tests hardcode seed 6.
- **The room is at (240, 240) tiles.** Anything thrown out of it lands in a
  world with no ground; `game_step()` puts you back.

---

## Owner's working style

He plays the build and reports in batches, often with screenshots. He
wants direct, honest reporting, including when something did not work or a
test measured the wrong thing. Ship a full zip after each round
(`update.sh` applies it and builds), and tell him what is verified versus
what is inspected but unproven. Nobody here can hear the audio: `sounds/`
in the zip is for him to judge.
