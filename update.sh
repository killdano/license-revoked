#!/usr/bin/env bash
# Apply a new License Revoked zip, build it, and open the folder with the PBW.
#
# Usage, from WSL:
#   bash /mnt/c/dev/license-revoked/update.sh                 # default zip path
#   bash update.sh ~/Downloads/license-revoked-full.zip       # or name one
#
# Safe to re-run. It backs the project up before overwriting anything.
set -euo pipefail

ZIP="${1:-/mnt/c/Users/killd_v6dmj/Downloads/license-revoked-full.zip}"
DEV=/mnt/c/dev
PROJ="$DEV/license-revoked"
STAMP=$(date +%Y%m%d-%H%M%S)

say() { printf '\n\033[1m== %s\033[0m\n' "$*"; }

[ -f "$ZIP" ] || { echo "No zip at: $ZIP"; echo "Pass the path as an argument."; exit 1; }

say "Backing up the current project"
if [ -d "$PROJ" ]; then
  cp -r "$PROJ" "$DEV/license-revoked-backup-$STAMP"
  echo "  -> $DEV/license-revoked-backup-$STAMP"
else
  echo "  (no existing project - fresh install)"
fi

say "Unpacking $(basename "$ZIP")"
mkdir -p "$DEV"
# The zip contains a top-level license-revoked/ directory, so this lands in
# exactly the right place and overwrites the old sources.
unzip -oq "$ZIP" -d "$DEV"
echo "  -> $PROJ"

say "Building"
# uv installs pebble-tool here; a fresh shell will not have it on PATH.
[ -f "$HOME/.local/bin/env" ] && source "$HOME/.local/bin/env"
command -v pebble >/dev/null || { echo "pebble not found on PATH - see BUILD.md"; exit 1; }

cd "$PROJ"
if pebble build; then
  say "Build succeeded"
  # The two numbers worth reading, pulled back out of the build output.
  FOOT=$(pebble build 2>&1 | grep -oE "Total footprint in RAM: *[0-9]+" | grep -oE "[0-9]+" || echo 0)
  echo "  Virtual size (code+data+bss): $FOOT / 65535 - this is the real limit"
  if [ "$FOOT" -gt 64500 ] 2>/dev/null; then
    echo "  WARNING: within 1 KB of the 65535 virtual-size ceiling. The next"
    echo "  addition will fail the build. See 'The 64 KB wall' in README.md."
  fi
  PBW=$(ls -1 build/*.pbw 2>/dev/null | head -1 || true)
  if [ -n "$PBW" ]; then
    echo "  PBW: $(wslpath -w "$PROJ/$PBW")"
    say "Opening the folder"
    explorer.exe "$(wslpath -w "$PROJ/build")" || true
  else
    echo "  Built, but no .pbw found in build/ - check the output above."
  fi
else
  say "Build FAILED"
  # If it died on the image ceiling, say by how much. Trimming blind is how
  # this has been done three times running.
  ELF=$(ls -1 build/*/pebble-app.elf 2>/dev/null | head -1 || true)
  # The size tool lives inside the SDK, not on PATH.
  SIZE=$(command -v arm-none-eabi-size || \
         find "$HOME/.local/share/pebble-sdk" -name 'arm-none-eabi-size' -type f 2>/dev/null | head -1)
  if [ -n "$ELF" ] && [ -n "$SIZE" ]; then
    read -r T D B _ <<EOF2
$("$SIZE" "$ELF" | tail -1)
EOF2
    TOTAL=$((T + D + B))
    echo "  image: text $T + data $D + bss $B = $TOTAL"
    echo "  virtual-size limit 65535 (16-bit firmware field), over by $((TOTAL - 65535))"
  fi
  echo "If it stopped at inject-metadata with"
  echo "  struct.error: 'H' format requires 0 <= number <= 65535"
  echo "you are over the 64 KB image ceiling; see 'The 64 KB wall' in README.md."
  echo "Your previous copy is at $DEV/license-revoked-backup-$STAMP"
  exit 1
fi
