# ---------------------------------------------------------------------------
# openklondike build
#
# Per-object compilation with automatic header-dependency tracking (-MMD -MP).
# Each target (linux dev/release, win64, win32) builds into its own object dir
# so their differing flags never clash.
# ---------------------------------------------------------------------------
RAYLIB       := third_party/raylib-install
RAYLIB_WIN64 := third_party/raylib-install-win64
RAYLIB_WIN32 := third_party/raylib-install-win32

# Single-header video pipeline: minih264 (encoder) + minimp4 (muxer). No -l
# flag — both compile into their own dedicated translation units.
MINIH264_INC := third_party/minih264
MINIMP4_INC  := third_party/minimp4

SRC := src/main.c src/game.c src/input.c src/render.c src/sound.c \
       src/recorder.c src/encode_h264.c src/encode_mux.c

# Shared standard/warning flags and vendored-header include paths.
CFLAGS_COMMON := -std=c99 -Wall -Wextra -I$(MINIH264_INC) -I$(MINIMP4_INC) -Isrc

# Release version: a single integer. The release workflow passes
# OPENKLONDIKE_VERSION explicitly; for local `make dist` it derives from the
# latest release-N tag (or 0 if there are none). Only used to name archives.
OPENKLONDIKE_VERSION ?= $(shell git tag --list 'release-*' 2>/dev/null | sed -n 's/^release-\([1-9][0-9]*\)$$/\1/p' | sort -n | tail -1 | grep . || echo 0)
VERSION_SLUG       := build-$(OPENKLONDIKE_VERSION)

# ---------------------------------------------------------------------------
# Linux (dev + release, static linking)
# ---------------------------------------------------------------------------
CFLAGS   := $(CFLAGS_COMMON) -O2 -I$(RAYLIB)/include
RELFLAGS := $(CFLAGS_COMMON) -O3 -I$(RAYLIB)/include
# Static link raylib and its dependencies.
LDFLAGS  := -L$(RAYLIB)/lib -Wl,-Bstatic -lraylib -Wl,-Bdynamic -lm -lpthread -ldl -lrt -lX11

OBJ_DIR     := build/obj
REL_OBJ_DIR := build/obj-release
OBJ     := $(SRC:src/%.c=$(OBJ_DIR)/%.o)
REL_OBJ := $(SRC:src/%.c=$(REL_OBJ_DIR)/%.o)

OUT         := build/openklondike
OUT_RELEASE := build/openklondike-release

all: $(OUT)

$(OBJ_DIR)/%.o: src/%.c | $(OBJ_DIR)
	gcc $(CFLAGS) -MMD -MP -c $< -o $@

$(OUT): $(OBJ)
	gcc $(OBJ) -o $@ $(LDFLAGS)

$(REL_OBJ_DIR)/%.o: src/%.c | $(REL_OBJ_DIR)
	gcc $(RELFLAGS) -MMD -MP -c $< -o $@

$(OUT_RELEASE): $(REL_OBJ)
	gcc $(REL_OBJ) -o $@ $(LDFLAGS)

release: $(OUT_RELEASE)

run: $(OUT)
	./$(OUT)

run-release: $(OUT_RELEASE)
	./$(OUT_RELEASE)

# ---------------------------------------------------------------------------
# Windows cross-compile (x64 + x86, static, fully self-contained)
# mingw-w64 predefines _WIN32, so no -D is needed.
# ---------------------------------------------------------------------------
WIN_CFLAGS  := $(CFLAGS_COMMON) -O2
WIN_LDFLAGS := -Wl,-Bstatic -lraylib -lopengl32 -lgdi32 -lwinmm -lpthread -Wl,-Bdynamic -mwindows -static -static-libgcc

WIN64_CC := x86_64-w64-mingw32-gcc
WIN32_CC := i686-w64-mingw32-gcc

WIN64_OBJ_DIR := build/obj-win64
WIN32_OBJ_DIR := build/obj-win32
WIN64_OBJ := $(SRC:src/%.c=$(WIN64_OBJ_DIR)/%.o)
WIN32_OBJ := $(SRC:src/%.c=$(WIN32_OBJ_DIR)/%.o)

OUT_WIN64 := build/openklondike-x64.exe
OUT_WIN32 := build/openklondike-x86.exe

windows: $(OUT_WIN64) $(OUT_WIN32)

$(WIN64_OBJ_DIR)/%.o: src/%.c | $(WIN64_OBJ_DIR)
	$(WIN64_CC) $(WIN_CFLAGS) -I$(RAYLIB_WIN64)/include -MMD -MP -c $< -o $@

$(OUT_WIN64): $(WIN64_OBJ)
	$(WIN64_CC) $(WIN64_OBJ) -o $@ -L$(RAYLIB_WIN64)/lib $(WIN_LDFLAGS)

$(WIN32_OBJ_DIR)/%.o: src/%.c | $(WIN32_OBJ_DIR)
	$(WIN32_CC) $(WIN_CFLAGS) -I$(RAYLIB_WIN32)/include -MMD -MP -c $< -o $@

$(OUT_WIN32): $(WIN32_OBJ)
	$(WIN32_CC) $(WIN32_OBJ) -o $@ -L$(RAYLIB_WIN32)/lib $(WIN_LDFLAGS)

# ---------------------------------------------------------------------------
# macOS build (universal arm64 + x86_64). CI-only: needs a macOS runner with an
# Xcode toolchain. raylib links several system frameworks for windowing, input,
# and OpenGL.
# ---------------------------------------------------------------------------
RAYLIB_MAC  := third_party/raylib-install-mac
MAC_CC      := clang
MAC_ARCHES  := -arch arm64 -arch x86_64
MAC_CFLAGS  := $(CFLAGS_COMMON) -O2 $(MAC_ARCHES) -I$(RAYLIB_MAC)/include
MAC_LDFLAGS := $(MAC_ARCHES) -L$(RAYLIB_MAC)/lib -lraylib -lpthread \
               -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL

MAC_OBJ_DIR := build/obj-mac
MAC_OBJ := $(SRC:src/%.c=$(MAC_OBJ_DIR)/%.o)
OUT_MAC := build/openklondike-mac

mac: $(OUT_MAC)

$(MAC_OBJ_DIR)/%.o: src/%.c | $(MAC_OBJ_DIR)
	$(MAC_CC) $(MAC_CFLAGS) -MMD -MP -c $< -o $@

$(OUT_MAC): $(MAC_OBJ)
	$(MAC_CC) $(MAC_OBJ) -o $@ $(MAC_LDFLAGS)

# ---------------------------------------------------------------------------
# Unit tests (game logic only — no raylib/window needed). The test TU includes
# game.c directly to reach its file-static helpers.
# ---------------------------------------------------------------------------
TEST_BIN := build/test_game

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): tests/test_game.c $(wildcard src/*.c src/*.h) | $(OBJ_DIR)
	gcc $(CFLAGS_COMMON) -O0 -g tests/test_game.c -o $(TEST_BIN)

# ---------------------------------------------------------------------------
# Distribution archives. Each dist-<platform> stages the platform binary plus
# README.md + LICENSE + NOTICE and packages it under dist/. Driven by the
# release workflow; runnable locally for the platforms you can build.
# ---------------------------------------------------------------------------
DIST    := dist
STAGING := build/staging
DOCS    := README.md LICENSE NOTICE

dist: dist-linux dist-windows dist-mac

dist-linux: release
	@rm -rf $(STAGING)/linux && mkdir -p $(STAGING)/linux/openklondike-$(VERSION_SLUG) $(DIST)
	cp $(OUT_RELEASE) $(STAGING)/linux/openklondike-$(VERSION_SLUG)/openklondike
	cp $(DOCS) $(STAGING)/linux/openklondike-$(VERSION_SLUG)/
	(cd $(STAGING)/linux && tar -czf ../../../$(DIST)/openklondike-$(VERSION_SLUG)-linux-x86_64.tar.gz openklondike-$(VERSION_SLUG))

dist-windows: $(OUT_WIN64) $(OUT_WIN32)
	@rm -rf $(STAGING)/win && mkdir -p $(DIST) \
	    $(STAGING)/win/openklondike-$(VERSION_SLUG)-x64 \
	    $(STAGING)/win/openklondike-$(VERSION_SLUG)-x86
	cp $(OUT_WIN64) $(STAGING)/win/openklondike-$(VERSION_SLUG)-x64/openklondike.exe
	cp $(DOCS) $(STAGING)/win/openklondike-$(VERSION_SLUG)-x64/
	cp $(OUT_WIN32) $(STAGING)/win/openklondike-$(VERSION_SLUG)-x86/openklondike.exe
	cp $(DOCS) $(STAGING)/win/openklondike-$(VERSION_SLUG)-x86/
	(cd $(STAGING)/win && zip -qr ../../../$(DIST)/openklondike-$(VERSION_SLUG)-windows-x64.zip openklondike-$(VERSION_SLUG)-x64)
	(cd $(STAGING)/win && zip -qr ../../../$(DIST)/openklondike-$(VERSION_SLUG)-windows-x86.zip openklondike-$(VERSION_SLUG)-x86)

dist-mac: $(OUT_MAC)
	@rm -rf $(STAGING)/mac && mkdir -p $(STAGING)/mac/openklondike-$(VERSION_SLUG) $(DIST)
	cp $(OUT_MAC) $(STAGING)/mac/openklondike-$(VERSION_SLUG)/openklondike
	codesign --force --sign - $(STAGING)/mac/openklondike-$(VERSION_SLUG)/openklondike
	cp $(DOCS) $(STAGING)/mac/openklondike-$(VERSION_SLUG)/
	(cd $(STAGING)/mac && zip -qr ../../../$(DIST)/openklondike-$(VERSION_SLUG)-macos-universal.zip openklondike-$(VERSION_SLUG))

# ---------------------------------------------------------------------------
$(OBJ_DIR) $(REL_OBJ_DIR) $(WIN64_OBJ_DIR) $(WIN32_OBJ_DIR) $(MAC_OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf build dist

# Pull in auto-generated header dependencies (ignored if not yet present).
-include $(OBJ:.o=.d) $(REL_OBJ:.o=.d) $(WIN64_OBJ:.o=.d) $(WIN32_OBJ:.o=.d) $(MAC_OBJ:.o=.d)

.PHONY: all run release run-release windows mac test dist dist-linux dist-windows dist-mac clean
