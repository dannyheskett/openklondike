#!/usr/bin/env bash
# Build raylib from source for the web (WebAssembly, via Emscripten) and install
# its headers + static archive into third_party/raylib-install-web/{include,lib}
# (the paths the Makefile's `web` target expects). The install dir is gitignored,
# so CI runs this before `make web`. Pinned to raylib 6.0 to match the other
# platforms; bump RAYLIB_TAG to move.
#
# Requires the Emscripten SDK on PATH (emcc). raylib's src/Makefile compiles with
# emcc directly when PLATFORM=PLATFORM_WEB.

set -euo pipefail

RAYLIB_TAG="${RAYLIB_TAG:-6.0}"
RAYLIB_SRC_DIR="${RAYLIB_SRC_DIR:-third_party/raylib}"
INSTALL_DIR="${RAYLIB_WEB_INSTALL_DIR:-third_party/raylib-install-web}"

command -v emcc >/dev/null 2>&1 || { echo "[build_raylib_web] emcc not found; activate emsdk first" >&2; exit 1; }

if [ ! -d "$RAYLIB_SRC_DIR" ]; then
    git clone --depth 1 --branch "$RAYLIB_TAG" \
        https://github.com/raysan5/raylib "$RAYLIB_SRC_DIR"
fi

# Fix raylib's web touchend handling: upstream removes only ONE lifted touch
# per touchend event, but lifting several fingers at once (a two-finger tap)
# arrives as a single event with multiple changed touches, leaving the touch
# count stuck above zero with no further events coming. Idempotent: skipped
# when the patch is already applied (pre-existing local clone).
PATCH="$(cd "$(dirname "$0")" && pwd)/raylib-web-touchend.patch"
if git -C "$RAYLIB_SRC_DIR" apply --reverse --check "$PATCH" 2>/dev/null; then
    echo "[build_raylib_web] touchend patch already applied"
else
    git -C "$RAYLIB_SRC_DIR" apply "$PATCH"
    echo "[build_raylib_web] applied touchend patch"
fi

# Static archive for the web target. GLES2 is the WebGL-compatible backend.
make -C "$RAYLIB_SRC_DIR/src" clean
make -C "$RAYLIB_SRC_DIR/src" \
    PLATFORM=PLATFORM_WEB \
    GRAPHICS=GRAPHICS_API_OPENGL_ES2 \
    CUSTOM_CFLAGS=-w \
    -j"$(nproc)"

mkdir -p "$INSTALL_DIR/include" "$INSTALL_DIR/lib"
cp "$RAYLIB_SRC_DIR/src/raylib.h" \
   "$RAYLIB_SRC_DIR/src/raymath.h" \
   "$RAYLIB_SRC_DIR/src/rlgl.h" "$INSTALL_DIR/include/"

# The web build's archive may be named libraylib.web.a or libraylib.a.
if [ -f "$RAYLIB_SRC_DIR/src/libraylib.web.a" ]; then
    cp "$RAYLIB_SRC_DIR/src/libraylib.web.a" "$INSTALL_DIR/lib/libraylib.a"
else
    cp "$RAYLIB_SRC_DIR/src/libraylib.a" "$INSTALL_DIR/lib/libraylib.a"
fi

echo "[build_raylib_web] installed raylib $RAYLIB_TAG (web) -> $INSTALL_DIR"
