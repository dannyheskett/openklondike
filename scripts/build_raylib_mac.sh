#!/usr/bin/env bash
# Build a universal (arm64 + x86_64) raylib static archive on macOS and install
# headers + libraylib.a into third_party/raylib-install-mac/{include,lib} (the
# paths the Makefile's `mac` target expects).
#
# Run once on a fresh clone before `make mac`; CI runs it before the macOS job.
# raylib's Makefile can't emit mixed-arch objects in one pass, so we build each
# arch and lipo them together — the recipe raylib's own CI uses. Pinned to 6.0.

set -euo pipefail

RAYLIB_TAG="${RAYLIB_TAG:-6.0}"
RAYLIB_SRC_DIR="${RAYLIB_SRC_DIR:-third_party/raylib}"
INSTALL_MAC="${INSTALL_MAC:-third_party/raylib-install-mac}"

if [ ! -d "$RAYLIB_SRC_DIR" ]; then
    git clone --depth 1 --branch "$RAYLIB_TAG" \
        https://github.com/raysan5/raylib "$RAYLIB_SRC_DIR"
fi

ARCH_DIR="$(pwd)/build/raylib-mac"
rm -rf "$ARCH_DIR"
mkdir -p "$ARCH_DIR/arm64" "$ARCH_DIR/x86_64"

build_arch() {
    local arch="$1"
    echo "[build_raylib_mac] building raylib for $arch"
    make -C "$RAYLIB_SRC_DIR/src" clean
    make -C "$RAYLIB_SRC_DIR/src" \
        PLATFORM=PLATFORM_DESKTOP_GLFW \
        RAYLIB_LIBTYPE=STATIC \
        CUSTOM_CFLAGS="-arch $arch" \
        -j"$(sysctl -n hw.ncpu)"
    cp "$RAYLIB_SRC_DIR/src/libraylib.a" "$ARCH_DIR/$arch/libraylib.a"
}

build_arch arm64
build_arch x86_64

mkdir -p "$INSTALL_MAC/include" "$INSTALL_MAC/lib"
cp "$RAYLIB_SRC_DIR/src/raylib.h" \
   "$RAYLIB_SRC_DIR/src/raymath.h" \
   "$RAYLIB_SRC_DIR/src/rlgl.h" "$INSTALL_MAC/include/"
lipo -create -output "$INSTALL_MAC/lib/libraylib.a" \
    "$ARCH_DIR/arm64/libraylib.a" \
    "$ARCH_DIR/x86_64/libraylib.a"

echo "[build_raylib_mac] installed universal raylib $RAYLIB_TAG -> $INSTALL_MAC"
lipo -info "$INSTALL_MAC/lib/libraylib.a"
