#!/usr/bin/env bash
# Cross-compile raylib for Windows on a Linux host using mingw-w64. Builds both
# x86_64 and i686 static archives and installs headers + libraylib.a into
# third_party/raylib-install-win64/{include,lib} and -win32/{include,lib} (the
# paths the Makefile's `windows` target expects).
#
# Run once on a fresh clone before `make windows`; CI runs it before the
# Windows cross-compile job. Building against the local mingw toolchain
# guarantees an ABI match (libgcc / winpthread). Pinned to raylib 6.0.

set -euo pipefail

RAYLIB_TAG="${RAYLIB_TAG:-6.0}"
RAYLIB_SRC_DIR="${RAYLIB_SRC_DIR:-third_party/raylib}"
INSTALL_WIN64="${INSTALL_WIN64:-third_party/raylib-install-win64}"
INSTALL_WIN32="${INSTALL_WIN32:-third_party/raylib-install-win32}"

if [ ! -d "$RAYLIB_SRC_DIR" ]; then
    git clone --depth 1 --branch "$RAYLIB_TAG" \
        https://github.com/raysan5/raylib "$RAYLIB_SRC_DIR"
fi

build_win() {
    local label="$1" cc="$2" ar="$3" dir="$4"
    echo "[build_raylib_windows] building raylib for $label"
    make -C "$RAYLIB_SRC_DIR/src" clean
    make -C "$RAYLIB_SRC_DIR/src" \
        PLATFORM=PLATFORM_DESKTOP_GLFW \
        OS=Windows_NT \
        CC="$cc" \
        AR="$ar" \
        RAYLIB_LIBTYPE=STATIC \
        -j"$(nproc)"
    mkdir -p "$dir/include" "$dir/lib"
    cp "$RAYLIB_SRC_DIR/src/raylib.h" \
       "$RAYLIB_SRC_DIR/src/raymath.h" \
       "$RAYLIB_SRC_DIR/src/rlgl.h" "$dir/include/"
    cp "$RAYLIB_SRC_DIR/src/libraylib.a" "$dir/lib/"
    echo "[build_raylib_windows] installed -> $dir"
}

build_win win64 x86_64-w64-mingw32-gcc x86_64-w64-mingw32-ar "$INSTALL_WIN64"
build_win win32 i686-w64-mingw32-gcc   i686-w64-mingw32-ar   "$INSTALL_WIN32"
