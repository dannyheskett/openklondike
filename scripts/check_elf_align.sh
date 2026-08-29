#!/usr/bin/env bash
#
# Verify every PT_LOAD segment in an ELF shared object is aligned to at least
# 16 KB. Google Play requires 16 KB memory-page support for native apps
# targeting Android 15+ (some devices use 16 KB pages instead of 4 KB); a .so
# whose LOAD segments are only 4 KB-aligned won't load there and Play rejects
# the bundle. NDK r26's lld still defaults to 4 KB, so the game is linked with
# -Wl,-z,max-page-size=16384 and this script asserts it took effect.
#
# Usage: check_elf_align.sh <path-to-llvm-readelf> <path-to-.so>
set -euo pipefail

readelf="$1"
so="$2"

bad=""
seen=0
# The Align column is the last field of each program-header LOAD line; readelf
# prints it in hex (e.g. 0x4000). POSIX arithmetic parses the 0x prefix.
for a in $("$readelf" -lW "$so" | awk '$1 == "LOAD" { print $NF }'); do
    seen=$((seen + 1))
    if [ "$((a))" -lt 16384 ]; then
        bad="$bad $a"
    fi
done

if [ "$seen" -eq 0 ]; then
    echo "ERROR: no LOAD segments found in $so (is '$readelf' a working readelf?)" >&2
    exit 1
fi

if [ -n "$bad" ]; then
    echo "ERROR: $so has LOAD segment(s) aligned to <16 KB:$bad" >&2
    echo "       (expected >= 0x4000; is -Wl,-z,max-page-size=16384 set?)" >&2
    exit 1
fi

echo "[android] $so: all LOAD segments are 16 KB-aligned"
