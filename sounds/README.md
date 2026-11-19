# What the watch will sound like

Rendered by `tools/hostcheck/sounddump.c` from the exact synth in `wr_audio.c`,
at the stream's own format (8 kHz, 8-bit). Play them on a PC before flashing:
nobody who worked on the synth could hear it, only look at it.

- `00`–`22`: every effect in `SFX_*` order (22 is being run over).
- `30_engine`: idle, accelerate to top speed, cruise, brake.
- `31_siren`: a patrol car approaching from ten tiles and leaving.
- `32_helicopter`: air support arriving overhead and leaving.
- `33_firefight`: pistol ×3, scream, shotgun, dying, crash, explosion.

Recipes are the rows of `SFX_TAB`; the engine, siren, rotor, flame, club
music and store alarm are the `gen_*` functions. Everything is a small
table or a dozen lines, so tuning by ear is cheap.
