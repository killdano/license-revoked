# Building License Revoked

Nothing in this tree has ever been compiled by the real Pebble SDK. Every
check described in README.md was run against a stub `pebble.h` with host
x86 gcc, which catches logic and arithmetic but cannot catch a missing SDK
header, a changed API signature, or a RAM overflow. Treat the first real
build as the first real test.

## The app

```sh
cd wrist-revoked
pebble build
pebble install --emulator emery
```

Targets `emery` (Pebble Time 2) exclusively.

### If the toolchain will not run

`Could not determine the compiler version` during configure means waf found
`arm-none-eabi-gcc` but could not execute it. The SDK ships a 32-bit
toolchain and modern 64-bit Linux has no i386 runtime by default:

```sh
sudo dpkg --add-architecture i386 && sudo apt-get update
sudo apt-get install -y libc6:i386 libstdc++6:i386 zlib1g:i386
arm-none-eabi-gcc --version        # should now print
```

Docker sidesteps the whole problem:

```sh
docker run --rm -v "$PWD":/pebble -w /pebble rebble/pebble-sdk pebble build
```

### The binary size limit

**The limit that matters is 65,535 bytes of virtual size** - code + data +
bss, as loaded into RAM, stored in 16-bit firmware fields. PebbleOS 4.33.2
raised the *packed image* to 128 KB on emery and gabbro, which is a different
number: it is the on-flash bundle, raised so a large relocation table can use
the space above 64 KB. Going over the virtual-size limit fails at the last
build step with:

```
struct.error: 'H' format requires 0 <= number <= 65535
```

Updating the SDK will not fix this - the newer SDK simply reports it more
clearly. You have to make the app smaller. The pool sizes in `src/c/wr.h` and `MAX_ITEM` in `wr_render.c` are
the levers if you ever genuinely need them, and `tools/hostcheck` has
harnesses that report each pool's true peak.

### What to watch on the first successful build

The `Free RAM` line. Entity, prop and particle budgets roughly doubled over
recent work and the tapered geometry added parts on top. If it will not fit,
`MAX_ENT`, `MAX_PROP` and `MAX_ITEM` in `src/c/wr.h` are the cheapest things
to walk back without changing how the game plays.

## The host test suite

Runs anywhere gcc runs; no SDK, no watch.

```sh
cd tools/hostcheck
make check      # the gating suite
make fuzz       # 150k frames under ASan/UBSan
make bench      # render cost per frame
```

Individual targets are listed at the top of the Makefile. Three are
deliberately **diagnostic rather than gating**, because the harness cannot
observe what they test:

| target | why it is not gating |
|---|---|
| `path` | foot pursuit around buildings: improved, not solved (2 of 4) |
| `occ`  | the stipple path: could not reliably trigger the occlusion branch |
| `engine` | the stub never opens an audio stream, so no voice reads as active |

**Known state at handover:** a full `make check` was still running when work
stopped, with six targets reporting and zero failures. It is unconfirmed
end to end. If it hangs, the newest things in the run are the likeliest
suspects - `consistency_test.c`, or the long `chase` target which simulates
900 seconds of gameplay.
