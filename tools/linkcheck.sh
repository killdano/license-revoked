#!/usr/bin/env bash
# Link the kernel exactly as the overlay build does - WITHOUT the domain
# sources - against a stub of the SDK, so a kernel symbol that only a domain
# still defines shows up here instead of at the owner's SDK link.
cd "$(dirname "$0")/.."
# The SDK compiles with warnings as errors; so does this.
for f in src/c/*.c; do
  arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -Os -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -DWR_LEAN -DWR_DOMAINS=1 -Itools/hostcheck -Isrc/c -Isrc/domains -c "$f" -o /tmp/linkcheck.o \
    || { echo "linkcheck: $f does not compile with -Werror"; exit 1; }
done
FL="-mcpu=cortex-m4 -mthumb -Os -std=c11 -DWR_LEAN -DWR_DOMAINS=1 -ffunction-sections -fdata-sections -Itools/hostcheck -Isrc/c -Isrc/domains -w"
out=$(arm-none-eabi-gcc $FL -nostartfiles -Wl,--gc-sections -Wl,-e,main src/c/*.c -o /tmp/linkcheck.elf 2>&1 | grep "undefined reference" | grep -oE "\`[a-z_0-9]+'" | sort -u)
# SDK functions are expected to be missing here; anything else is a leak.
leak=$(echo "$out" | grep -vE "^\`(graphics_|window_|layer_|app_|persist_|speaker_|fonts_|resource_|light_|vibes_|touch_|gbitmap_|accel_|tick_|psleep|snprintf|memset|memcpy|strlen|abs|_gettimeofday|_exit|_sbrk|_write|_read|_close|_lseek|_fstat|_isatty|_kill|_getpid)" || true)
if [ -n "$leak" ]; then echo "linkcheck: kernel references symbols only a domain defines:"; echo "$leak"; exit 1; fi
echo "linkcheck: kernel links on its own"
