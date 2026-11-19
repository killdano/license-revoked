# Build and run Wrist Revoked on a desktop against a stub Pebble SDK.
# Everything here runs the real simulation headless - no watch required.
#
#   make check     the whole suite (this is the one to run)
#   make fuzz      150k frames of random input under ASan + UBSan
#   make systems   every major feature, pass/fail
#   make logic     state-machine edge cases (wrecks, interiors, air support)
#   make edge      interaction edges: shops, death screen, dangling refs
#   make audio     synth output is in range, decays, and mutes
#   make drive     player and AI traffic following the diagonal streets
#   make drift     a car holds its lane on all four street headings
#   make band      street bands survive turning; no holes under the player
#   make marine    boats, marinas, the marine unit, driving the bridge
#   make gunner    helicopter door gunner
#   make world     ascii map of both islands and the bridge
#   make chase     five stars down to clean
#   make physics   ragdolls, crashes, arrest, walking backwards
#   make island    terrain mix, lot variety, props, zoom slider
#   make heli      air support, SWAT rappel, weapon drops
#   make shop      pause menu, gun shop interior, heavy weapons
#   make reverse   reverse steering mirrors forward
#   make camera    where the character sits across the zoom range
#   make pixels    where the fill budget actually goes
#   make bench     host render time, for comparing rasteriser changes
#   make lod       polygon budget, driving vs on foot
#   make lint      warnings-only pass over the game sources

GAME := $(filter-out ../../src/c/main.c,$(wildcard ../../src/c/*.c)) $(wildcard ../../src/domains/dom_*.c)
SRC  := $(GAME) stubs.c
INC  := -I. -I../../src/c -I../../src/domains -DWR_MONOLITH=1 -DWR_SWAP_HOOK
SAN  := -fsanitize=address,undefined
WARN := -Wall -Wextra -Wshadow -Wstrict-prototypes

define RUN
	@gcc -std=c11 -O2 -g -DWR_PROFILE $(SAN) $(INC) $(SRC) $(1) -o /tmp/wr_t
	@/tmp/wr_t
endef
# The dash trace: the painter reports every dash it emits.
define RUN_TRACE
	@gcc -std=c11 -O2 -g -DWR_PROFILE -DWR_DASH_TRACE $(SAN) $(INC) $(SRC) $(1) -o /tmp/wr_t
	@/tmp/wr_t
endef
# Boats are a compile-time option (off in the shipped build); their harnesses build with them on.
define RUN_BOATS
	@gcc -std=c11 -O2 -g -DWR_PROFILE -DWR_BOATS=1 $(SAN) $(INC) $(SRC) $(1) -o /tmp/wr_t
	@/tmp/wr_t
endef

check: lint air garage inside taxi systems logic edge audio drive drift band marine gunner physics island heli shop chase features cover swim stats pier parked deck armor grenade shoplayout burn clip inv hold wallsee nobleed rocket arch shopfind
	@echo
	@echo "all checks complete"

fuzz:
	@gcc -std=c11 -O2 -g $(SAN) $(INC) $(SRC) fuzz.c -o /tmp/wr_fuzz
	@/tmp/wr_fuzz 150000

systems:  ; $(call RUN,systems.c)
logic:    ; $(call RUN,logic.c)
edge:     ; $(call RUN,edge.c)
audio:    ; $(call RUN,aud.c)
drive:    ; $(call RUN,drive.c)
drift:    ; $(call RUN,drift.c)
band:     ; $(call RUN,band.c)
marine:   ; $(call RUN_BOATS,marine.c)
gunner:   ; $(call RUN,gunner.c)
world:    ; $(call RUN,world.c)
hole:     ; $(call RUN,hole.c)
chase:    ; $(call RUN,chase_test.c)
features: ; $(call RUN,features.c)
cover:   ; $(call RUN,cover_test.c)
swim:    ; $(call RUN,swim_test.c)
stats:   ; @echo "  (retired: lifetime stats cut with the death-screen line that showed them)"
pier:    ; $(call RUN_BOATS,pier_test.c)
drag:    ; @echo "  (retired: bodies are no longer dragged by cars - corpse_shove was cut for size)"
parked:  ; $(call RUN,parked_test.c)
deck:    ; $(call RUN,deck_test.c)
armor:   ; $(call RUN,armor_test.c)
grenade: ; @echo "  (retired: grenades cut - unobtainable once guns stopped lying about)"
shoplayout: ; $(call RUN,shoplayout_test.c)
burn:    ; $(call RUN,burn_test.c)
clip:    ; $(call RUN,clip_test.c)
inv:     ; $(call RUN,inv_test.c)
hold:    ; $(call RUN,hold_test.c)
wallsee: ; $(call RUN,wallsee_test.c)
nobleed: ; $(call RUN,nobleed_test.c)
rocket:  ; $(call RUN,rocket_test.c)
arch:    ; $(call RUN,arch_test.c)
shopfind:; $(call RUN,shop_find_test.c)
# Diagnostic, not gating: the reserved-slot policy is inspectable, but the
# host stub's audio stream never opens, so no voice is ever marked active
# and the probe cannot see the engine from here.
engine:  ; $(call RUN,engine_test.c)
# Diagnostic, not gating: the stipple path is inspectable but I could not
# get a headless scenario to reliably trigger the occlusion branch.
occ:     ; $(call RUN,occ_test.c)
# Diagnostic, not gating: foot pursuit around buildings is improved but not
# solved. Baseline at time of writing: 1 of 3 staged cops reaches the player.
path:    ; $(call RUN,path_test.c)
physics:  ; $(call RUN,physics_test.c)
island:   ; $(call RUN,island_test.c)
heli:     ; $(call RUN,heli_test.c)
air:      ; $(call RUN,airsupport.c)
garage:   ; $(call RUN,garage.c)
inside:   ; $(call RUN,inside.c)
taxi:     ; $(call RUN,taxi.c)
shop:     ; $(call RUN,shop_test.c)
reverse:  ; $(call RUN,reverse_test.c)
camera:   ; $(call RUN,camera_test.c)
stall:    ; $(call RUN,stall.c)
stability:; $(call RUN,stab.c)

secret:  ; $(call RUN,secret_test.c)
guard:   ; $(call RUN,guard_test.c)
dash:    ; $(call RUN_TRACE,dash_test.c)
paint:   ; $(call RUN,paint_test.c)
crime:   ; $(call RUN,crime_test.c)
knock:   ; $(call RUN,knock_test.c)
mirror:  ; $(call RUN,mirror_test.c)
arsenal: ; $(call RUN,arsenal_test.c)
swap:    ; $(call RUN,swap_test.c)
airport: ; $(call RUN,airport_test.c)
militia: ; $(call RUN,militia_test.c)
jump:    ; $(call RUN,jump_test.c)
bench:
	@gcc -std=c11 -O2 $(INC) $(SRC) bench.c -o /tmp/wr_bench
	@/tmp/wr_bench

pixels:
	@gcc -std=c11 -O2 -DWR_PROFILE $(INC) $(SRC) px.c -o /tmp/wr_px
	@/tmp/wr_px

lod:
	@gcc -std=c11 -O2 -DWR_PROFILE $(INC) $(SRC) lod_test.c -o /tmp/wr_lod
	@/tmp/wr_lod

lint:
	@for f in $(wildcard ../../src/c/*.c); do \
	  gcc -std=c11 -Os $(WARN) $(INC) -fsyntax-only $$f || exit 1; \
	done
	@echo "lint clean"

.PHONY: check fuzz systems logic edge audio drive drift band marine gunner world hole chase physics \
        island heli shop reverse camera stall stability pixels bench lod lint
