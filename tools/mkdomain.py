#!/usr/bin/env python3
"""Build one domain source into a loadable blob.

    [ 4-byte header ][ code + rodata ][ relocation offsets, 2 bytes each ]

Header: u16 code_len, u16 nreloc. Each relocation is a byte offset into the
code at which a 32-bit absolute address sits; the loader adds the arena base
to that word. Linking at base 0 with --emit-relocs is what makes this
possible: every address in the blob is already an offset.
"""
import subprocess, sys, struct, os, re

src, out, cc = sys.argv[1], sys.argv[2], sys.argv[3]
tmp = out + '.tmp'
inc = os.path.dirname(src)

subprocess.run([cc, '-mcpu=cortex-m4', '-mthumb', '-Os', '-std=c11',
                '-ffunction-sections', '-fdata-sections', '-fno-common', '-DWR_DOMAINS=1', '-DWR_MONOLITH=0',
                '-I', inc, '-I', 'src/c', '-I', 'tools/hostcheck', '-c', src, '-o', tmp + '.o'], check=True)

# .text.entry first, so the entry point is at offset 0.
# Code and constants first, then the domain's own variables. A domain may
# keep state in statics; those live in the arena after the code and are
# carried in the blob as zeros (bss) or initial values (data), so the arena
# holds everything the domain touches and nothing is written past it.
with open(tmp + '.ld', 'w') as f:
    f.write("""SECTIONS {
  . = 0;
  .text : { *(.text.entry) *(.text*) *(.rodata*) }
  .data : { *(.data*) }
  .bss  : { *(.bss*) *(COMMON) }
  /DISCARD/ : { *(.ARM.attributes) *(.comment) *(.debug*) }
}
""")
def tool(name):
    # arm-none-eabi-gcc -> arm-none-eabi-<name>, keeping any directory part.
    import os
    d, b = os.path.split(cc)
    return os.path.join(d, b.replace('gcc', name)) if d else b.replace('gcc', name)

ld = tool('ld')
subprocess.run([ld, '-T', tmp + '.ld', '--emit-relocs', '-o', tmp + '.elf', tmp + '.o'], check=True)

objcopy = tool('objcopy')
# bss is NOBITS; make it a real, zero-filled section so the binary covers it.
subprocess.run([objcopy, '--set-section-flags', '.bss=alloc,load,contents',
                '-O', 'binary', tmp + '.elf', tmp + '.bin'], check=True)
code = open(tmp + '.bin', 'rb').read()

objdump = tool('objdump')
rel = subprocess.run([objdump, '-r', tmp + '.elf'], capture_output=True, text=True).stdout
offs = []
for line in rel.splitlines():
    m = re.match(r'^([0-9a-f]{8})\s+(R_ARM_\w+)\s', line)
    if not m:
        continue
    off, kind = int(m.group(1), 16), m.group(2)
    if kind == 'R_ARM_ABS32':
        offs.append(off)
    elif kind in ('R_ARM_THM_CALL', 'R_ARM_THM_JUMP24', 'R_ARM_PREL31', 'R_ARM_REL32'):
        pass                      # PC-relative: correct wherever the blob lands
    else:
        raise SystemExit('mkdomain: unhandled relocation %s at %#x - the domain '
                         'is referencing something outside itself' % (kind, off))

if len(code) % 4:
    code += b'\0' * (4 - len(code) % 4)
blob = struct.pack('<HH', len(code), len(offs)) + code + b''.join(struct.pack('<H', o) for o in offs)
open(out, 'wb').write(blob)
for e in ('.o', '.ld', '.elf', '.bin'):
    os.remove(tmp + e)
print('%s: %d bytes of code, %d relocations, %d total'
      % (os.path.basename(out), len(code), len(offs), len(blob)))
