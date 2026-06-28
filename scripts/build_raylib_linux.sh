#!/usr/bin/env bash
# Build raylib from source on Linux and install its headers + static archive
# into third_party/raylib-install/{include,lib} (the paths the Makefile
# expects). The install dir is gitignored, so run this once on a fresh clone
# before `make`, and CI runs it before every Linux build.
#
# Building raylib inside the runner/clone guarantees the archive links against
# the local libc (a prebuilt .a from a newer glibc fails on older runners with
# undefined __isoc23_* symbols). Pinned to raylib 6.0; bump RAYLIB_TAG to move.

set -euo pipefail

RAYLIB_TAG="${RAYLIB_TAG:-6.0}"
RAYLIB_SRC_DIR="${RAYLIB_SRC_DIR:-third_party/raylib}"
INSTALL_DIR="${RAYLIB_INSTALL_DIR:-third_party/raylib-install}"

if [ ! -d "$RAYLIB_SRC_DIR" ]; then
    git clone --depth 1 --branch "$RAYLIB_TAG" \
        https://github.com/raysan5/raylib "$RAYLIB_SRC_DIR"
fi

# Static archive only; PLATFORM_DESKTOP_GLFW is the standard Linux desktop
# target. No shared lib, examples, or extras — the game links libraylib.a.
make -C "$RAYLIB_SRC_DIR/src" clean
make -C "$RAYLIB_SRC_DIR/src" \
    PLATFORM=PLATFORM_DESKTOP_GLFW \
    RAYLIB_LIBTYPE=STATIC \
    -j"$(nproc)"

mkdir -p "$INSTALL_DIR/include" "$INSTALL_DIR/lib"
cp "$RAYLIB_SRC_DIR/src/raylib.h" \
   "$RAYLIB_SRC_DIR/src/raymath.h" \
   "$RAYLIB_SRC_DIR/src/rlgl.h" "$INSTALL_DIR/include/"
cp "$RAYLIB_SRC_DIR/src/libraylib.a" "$INSTALL_DIR/lib/"

echo "[build_raylib_linux] installed raylib $RAYLIB_TAG -> $INSTALL_DIR"
