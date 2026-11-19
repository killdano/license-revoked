#!/bin/bash
# The Android build compiles every source in one image with clang, which
# refuses implicit declarations. Do the same here so it is caught first.
cd "$(dirname "$0")/.."
ok=1
for f in src/c/wr_*.c src/domains/dom_*.c; do
  gcc -std=c11 -Wall -Werror=implicit-function-declaration -DWR_MONOLITH=1 -DWR_LEAN -DWR_DOMAINS=1 -DWR_TAXI=1 -DWR_GOD=0 \
      -Itools/hostcheck -Isrc/c -Isrc/domains -c "$f" -o /tmp/mono.o 2>/tmp/mono.err || { echo "monocheck: $f"; grep -m3 error /tmp/mono.err; ok=0; }
done
[ $ok = 1 ] && echo "monocheck: every source compiles as one image"
