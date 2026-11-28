import os.path

# Overlays off: the club and the store are compiled into the image, which is
# how the game shipped up to 3.14. Turn this on (and it flips -DWR_DOMAINS=1
# below) once enough domains exist to pay for the arena - see DESIGN.md.
WR_DOMAINS = True

# Link-time optimisation. Saves 628 bytes on a plain gcc link, but the SDK's
# link step made the image 257 bytes BIGGER with it on - the merged code is
# not being optimised there. Off until that is understood.
WR_LTO = False

# Testing: immortal and unarrestable. OFF before anything is published.
WR_GOD = True

top = '.'
out = 'build'

def options(ctx):
    ctx.load('pebble_sdk')

def configure(ctx):
    ctx.load('pebble_sdk')
    # Optimise for size, and let the linker drop anything unreachable.
    #
    # The app image has a hard 64 KB ceiling - text + data + bss must fit in a
    # 16-bit field - and this project has hit it three times. These flags cost
    # nothing in behaviour: -Os trades a little speed for size on a renderer
    # that is already inside its frame budget by a wide margin, and
    # gc-sections removes functions and data that nothing references rather
    # than shipping them.
    for env in ctx.all_envs.values():
        # WR_LEAN is ON, and has to be. PebbleOS 4.33.2 raised the PACKED
        # IMAGE to 128 KB - the space a large relocation table can occupy -
        # but the app's VIRTUAL SIZE, meaning code + data + bss as loaded
        # into RAM, is still capped at 65535 because the firmware stores it
        # in 16-bit fields. Two different numbers, and only one of them
        # moved. See 'What the 128 KB actually covers' in README.md.
        #
        # Feature switches, in the order to flip them if the build lands
        # over the ceiling (sizes measured with a modern GCC; the SDK's is
        # roughly 9% bigger):
        #   -DWR_BOATS=0   drops the boats and marinas     (about 2.5 KB)
        #   -DWR_TAXI=0    drops the taxi fares            (about 700 bytes)
        #   -DWR_KEYS      emulator build: hold SELECT to drive, tap for the action
        #                  (the emulator has no touchscreen; arrow keys are the buttons)
        for flag in ['-Os', '-DWR_LEAN', '-DWR_BOATS=0', '-DWR_TAXI=1',
                     '-DWR_DOMAINS=1' if WR_DOMAINS else '-DWR_DOMAINS=0',
                     '-DWR_GOD=1' if WR_GOD else '-DWR_GOD=0',
                     '-ffunction-sections', '-fdata-sections'] + (['-flto'] if WR_LTO else []):
            if flag not in env.CFLAGS:
                env.append_value('CFLAGS', flag)
        for flag in ['-Wl,--gc-sections'] + (['-flto', '-Os'] if WR_LTO else []):
            if flag not in env.LINKFLAGS:
                env.append_value('LINKFLAGS', flag)

def _find_cc(ctx):
    """Locate arm-none-eabi-gcc: it is set per platform, not in the top-level
    env, and the SDK toolchain is usually not on PATH."""
    import os, shutil
    for env in list(ctx.all_envs.values()) + [ctx.env]:
        cc = getattr(env, 'CC', None)
        if isinstance(cc, list):
            cc = cc[0] if cc else None
        if cc and os.path.basename(str(cc)).endswith('gcc'):
            return str(cc)
    found = shutil.which('arm-none-eabi-gcc')
    if found:
        return found
    for root in (os.path.expanduser('~/.local/share/pebble-sdk'),
                 os.path.expanduser('~/pebble-dev')):
        for dirpath, _d, files in os.walk(root):
            if 'arm-none-eabi-gcc' in files:
                return os.path.join(dirpath, 'arm-none-eabi-gcc')
    ctx.fatal('cannot find arm-none-eabi-gcc to build the domains')

def build(ctx):
    # Domains are compiled and linked on their own, and land in resources/ as
    # raw blobs. They are NOT part of the app image - that is the whole point.
    import subprocess, glob, sys, os.path as _p
    cc = _find_cc(ctx)
    # Only when overlays are switched on: otherwise the domains are compiled
    # into the image like any other source and the blobs would be dead weight
    # in the bundle.
    if '-DWR_DOMAINS=1' in ctx.env.CFLAGS or WR_DOMAINS:
        # Two arenas: PLACE (dom_*.c) and THREAT (dom_t_*.c). Each is sized
        # to the largest blob in its group, from the blobs just built - never
        # hand-written: a compiler that emits 28 more bytes than the one you
        # sized against makes the loader refuse the blob, silently.
        # Built once per watch: the Round 2's blobs see its 260x260 circle
        # (WR_ROUND), and go in as dom_*~gabbro.bin, which the SDK picks for
        # that platform over the plain file. The Time 2 keeps the plain files,
        # built exactly as before.
        ROUND = {'gabbro': ['-DWR_ROUND=1', '-DSCR_W=260', '-DSCR_H=260']}
        arenas = {}
        for plat in ctx.env.TARGET_PLATFORMS:
            biggest = {'place': 0, 'threat': 0, 'hands': 0}
            tag = '~' + plat if plat in ROUND else ''
            for src in sorted(glob.glob('src/domains/dom_*.c')):
                out_bin = 'resources/' + _p.basename(src)[:-2] + tag + '.bin'
                subprocess.check_call([sys.executable, 'tools/mkdomain.py', src, out_bin, cc]
                                      + ROUND.get(plat, []))
                with open(out_bin, 'rb') as f:
                    head = f.read(4)
                b = _p.basename(src)
                grp = 'threat' if b.startswith('dom_t_') else 'hands' if b.startswith('dom_h_') else 'place'
                biggest[grp] = max(biggest[grp], head[0] | (head[1] << 8))
            arenas[plat] = tuple(((biggest[g] + 16) + 7) & ~7 for g in ('place', 'threat', 'hands'))
            print('arenas %s: place %d (largest %d), threat %d (largest %d), hands %d (largest %d)'
                  % (plat, arenas[plat][0], biggest['place'], arenas[plat][1], biggest['threat'],
                     arenas[plat][2], biggest['hands']))
        body = '#define ARENA_BYTES %d\n#define ARENA2_BYTES %d\n#define ARENA3_BYTES %d\n'
        plain = [p for p in sorted(arenas) if p not in ROUND]
        rounds = [p for p in sorted(arenas) if p in ROUND]
        with open('src/c/wr_arena.h', 'w') as f:
            f.write('// Generated by wscript from the domain blobs. Do not edit.\n#pragma once\n')
            if not rounds:
                f.write(body % arenas[plain[0]])
            else:
                for i, p in enumerate(rounds):
                    f.write('#%s defined(PBL_PLATFORM_%s)\n' % ('if' if i == 0 else 'elif', p.upper()))
                    f.write(body % arenas[p])
                f.write('#else\n' + body % arenas[plain[0]] + '#endif\n')

    ctx.load('pebble_sdk')
    binaries = []
    for p in ctx.env.TARGET_PLATFORMS:
        ctx.set_env(ctx.all_envs[p])
        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        # With overlays off the game is exactly what shipped in 3.14:
        # src/c/ only. The domain sources and the kernel-side wr_inside.c
        # that goes with them are kept in src/domains/ for the next phase -
        # see DESIGN.md - but are not built, because the split costs about
        # 600 bytes of lost cross-file inlining and does not pay for itself
        # until boats and air support are domains too.
        # With overlays ON the domain sources are NOT part of the image -
        # they were compiled into blobs above and are loaded from resources.
        # With overlays OFF they are compiled in like any other source.
        srcs = ctx.path.ant_glob('src/c/**/*.c')
        if not WR_DOMAINS:
            srcs += ctx.path.ant_glob('src/domains/dom_*.c')
        ctx.pbl_program(source=srcs, target=app_elf,
                        includes=['src/c', 'src/domains'])
        binaries.append({'platform': p, 'app_elf': app_elf})
    ctx.set_group('bundle')
    ctx.pbl_bundle(binaries=binaries, js=[])
