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

SRC := src/main.c src/game.c src/tick.c src/input.c \
       src/render.c src/render_fixed.c src/render_scaled.c src/gfx_raylib.c \
       src/safe_area.c \
       src/sound.c src/audio_raylib.c \
       src/recorder.c src/encode_h264.c src/encode_mux.c

# Shared standard/warning flags and vendored-header include paths.
CFLAGS_COMMON := -std=c99 -Wall -Wextra -I$(MINIH264_INC) -I$(MINIMP4_INC) -Isrc

# Release version: a single integer. The release workflow passes
# OPENKLONDIKE_VERSION explicitly; for local `make dist` it derives from the
# latest release-N tag (or 0 if there are none). Only used to name archives.
# RELEASE_VERSION is the project-neutral name the release workflow passes, so every
# repo's release.yml is byte-identical. OPENKLONDIKE_VERSION still works as an explicit
# override (command-line vars beat ?=), and a bare `make dist` still derives from tags.
RELEASE_VERSION ?= $(shell git tag --list 'release-*' 2>/dev/null | sed -n 's/^release-\([1-9][0-9]*\)$$/\1/p' | sort -n | tail -1 | grep . || echo 0)
OPENKLONDIKE_VERSION ?= $(RELEASE_VERSION)
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
# Android build (NativeActivity APK, no Gradle). CI-only: needs the NDK + SDK
# build-tools, both provided by the setup-android action. Mirrors raylib's
# upstream Makefile.Android flow: cross-compile the game + the NDK's
# native_app_glue into libopenklondike.so, then package + sign an APK with
# aapt / zipalign / apksigner. The recorder is stubbed on Android, so the
# encode_h264/encode_mux TUs and the minih264/minimp4 include paths are dropped.
#
# Requires env: ANDROID_NDK, ANDROID_SDK_ROOT.
# ---------------------------------------------------------------------------
ANDROID_API          ?= 24
ANDROID_ABI          := arm64-v8a
ANDROID_BUILD_TOOLS  ?= 35.0.0
ANDROID_PLATFORM_VER ?= 35

# versionCode must be a monotonically increasing integer for Play uploads; drive
# it off the release number (unique + monotonic). Clamp to >=1 for local builds
# where OPENKLONDIKE_VERSION is 0 (no release tags yet). versionName is the
# human-facing string. Both are injected at package time (aapt/aapt2 flags), so
# the manifest values are just fallbacks.
ANDROID_VERSION_CODE ?= $(OPENKLONDIKE_VERSION)
ifeq ($(ANDROID_VERSION_CODE),0)
ANDROID_VERSION_CODE := 1
endif
ANDROID_VERSION_NAME ?= 1.0.$(ANDROID_VERSION_CODE)

RAYLIB_ANDROID := third_party/raylib-install-android/$(ANDROID_ABI)

ANDROID_TOOLCHAIN := $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64
ANDROID_CC        := $(ANDROID_TOOLCHAIN)/bin/aarch64-linux-android$(ANDROID_API)-clang
NATIVE_APP_GLUE   := $(ANDROID_NDK)/sources/android/native_app_glue

ANDROID_SDK_BT := $(ANDROID_SDK_ROOT)/build-tools/$(ANDROID_BUILD_TOOLS)
ANDROID_JAR    := $(ANDROID_SDK_ROOT)/platforms/android-$(ANDROID_PLATFORM_VER)/android.jar

ANDROID_SRC     := $(filter-out src/encode_h264.c src/encode_mux.c,$(SRC))
ANDROID_CFLAGS  := -std=c99 -Wall -Wextra -Isrc -DPLATFORM_ANDROID -fPIC \
                   -I$(RAYLIB_ANDROID)/include -I$(NATIVE_APP_GLUE)

# raylib wraps fopen at link time (-Wl,--wrap=fopen) so file access routes
# through the Android asset manager; libraylib.a references __real_fopen, which
# only exists when this flag is present. Without it, dlopen of libopenklondike.so
# fails at launch with "cannot locate symbol __real_fopen".
#
# -z max-page-size=16384 gives the .so 16 KB-aligned LOAD segments. Google Play
# requires 16 KB page-size support for apps targeting Android 15+ (devices with
# 16 KB memory pages); NDK r26's linker still defaults to 4 KB, so we set it
# explicitly. Verified after linking (check_elf_align.sh).
ANDROID_LDFLAGS := -shared -L$(RAYLIB_ANDROID)/lib -lraylib \
                   -Wl,--wrap=fopen \
                   -Wl,-z,max-page-size=16384,-z,common-page-size=16384 \
                   -llog -landroid -lEGL -lGLESv2 -lOpenSLES -lm -lc -ldl

ANDROID_OBJ_DIR := build/obj-android
ANDROID_OBJ     := $(ANDROID_SRC:src/%.c=$(ANDROID_OBJ_DIR)/%.o) \
                   $(ANDROID_OBJ_DIR)/native_app_glue.o

ANDROID_APK_DIR  := build/android
ANDROID_LIB      := $(ANDROID_APK_DIR)/lib/$(ANDROID_ABI)/libopenklondike.so
ANDROID_APK      := build/openklondike.apk
ANDROID_KEYSTORE ?= build/debug.keystore

# One tiny Java class (OpenklondikeActivity, a NativeActivity subclass that hides
# the system bars and forwards the display cutout to native) is compiled to a
# classes.dex and bundled. No Gradle: javac -> d8, both from the JDK + SDK
# build-tools already on the CI runner.
ANDROID_JAVA_SRC := android/java/com/danheskett/openklondike/OpenklondikeActivity.java
ANDROID_DEX      := build/dex/classes.dex
JAVAC            ?= javac

android: $(ANDROID_APK)

# native_app_glue is vendored NDK source (not ours); it trips -Wextra's
# unused-parameter, so build this one object without it to keep the log clean.
$(ANDROID_OBJ_DIR)/native_app_glue.o: $(NATIVE_APP_GLUE)/android_native_app_glue.c | $(ANDROID_OBJ_DIR)
	$(ANDROID_CC) $(ANDROID_CFLAGS) -Wno-unused-parameter -c $< -o $@

$(ANDROID_OBJ_DIR)/%.o: src/%.c | $(ANDROID_OBJ_DIR)
	$(ANDROID_CC) $(ANDROID_CFLAGS) -MMD -MP -c $< -o $@

$(ANDROID_LIB): $(ANDROID_OBJ)
	@mkdir -p $(dir $@)
	$(ANDROID_CC) $(ANDROID_OBJ) -o $@ $(ANDROID_LDFLAGS)
	@scripts/check_elf_align.sh $(ANDROID_TOOLCHAIN)/bin/llvm-readelf $@

# Compile the Activity against the platform jar, then dex it. d8 emits
# classes.dex into build/dex/. -source/-target 8 keeps the bytecode dex-friendly;
# android.jar on the classpath resolves the framework APIs (java.* comes from the
# JDK's own boot classpath).
$(ANDROID_DEX): $(ANDROID_JAVA_SRC)
	@rm -rf build/java-classes && mkdir -p build/java-classes $(dir $@)
	$(JAVAC) -source 1.8 -target 1.8 -Xlint:-options \
	    -classpath $(ANDROID_JAR) -d build/java-classes $(ANDROID_JAVA_SRC)
	$(ANDROID_SDK_BT)/d8 --min-api $(ANDROID_API) --lib $(ANDROID_JAR) \
	    --output build/dex build/java-classes/com/danheskett/openklondike/*.class

# Throwaway debug keystore for signing. Real distributable builds sign with a
# keystore supplied from a CI secret instead.
$(ANDROID_KEYSTORE):
	@mkdir -p $(dir $@)
	keytool -genkeypair -keystore $@ -storepass android -keypass android \
	    -alias openklondike -keyalg RSA -keysize 2048 -validity 10000 \
	    -dname "CN=openklondike, O=openklondike, C=US"

$(ANDROID_APK): $(ANDROID_LIB) $(ANDROID_DEX) $(ANDROID_KEYSTORE) \
                android/AndroidManifest.xml android/res/values/styles.xml
	# -S compiles android/res (the theme and the launcher icons).
	$(ANDROID_SDK_BT)/aapt package -f -M android/AndroidManifest.xml \
	    -S android/res -I $(ANDROID_JAR) \
	    --version-code $(ANDROID_VERSION_CODE) --version-name $(ANDROID_VERSION_NAME) \
	    -F build/openklondike.unaligned.apk
	# Store the native lib at lib/<abi>/ inside the APK (path is relative to cwd).
	(cd $(ANDROID_APK_DIR) && $(ANDROID_SDK_BT)/aapt add \
	    ../../build/openklondike.unaligned.apk lib/$(ANDROID_ABI)/libopenklondike.so)
	# Store classes.dex at the APK root (path relative to cwd = build/dex).
	(cd build/dex && $(ANDROID_SDK_BT)/aapt add \
	    ../openklondike.unaligned.apk classes.dex)
	$(ANDROID_SDK_BT)/zipalign -f 4 \
	    build/openklondike.unaligned.apk build/openklondike.aligned.apk
	$(ANDROID_SDK_BT)/apksigner sign --ks $(ANDROID_KEYSTORE) \
	    --ks-pass pass:android --key-pass pass:android \
	    --out $@ build/openklondike.aligned.apk
	@rm -f build/openklondike.unaligned.apk build/openklondike.aligned.apk
	@echo "[android] built $@"

# ---------------------------------------------------------------------------
# Android App Bundle (.aab) for Google Play. Play only accepts AABs for new
# apps, and the legacy `aapt` (v1) above cannot emit one, so this path uses
# `aapt2` (proto resources) + `bundletool`. Kept fully separate from the
# sideload APK target: same libopenklondike.so, different packaging + a real
# upload key. Signed with the upload key; Google's Play App Signing re-signs the
# delivered APKs, so this signature only has to satisfy the Play upload check.
#
# Signing defaults to the throwaway debug keystore so `make dist-android-play`
# works locally to exercise the pipeline; CI overrides PLAY_* with the real
# upload keystore (from a secret) to produce an uploadable bundle.
# ---------------------------------------------------------------------------
ANDROID_AAB        := build/openklondike.aab
BUNDLETOOL_VERSION ?= 1.17.2
BUNDLETOOL         ?= build/bundletool.jar
# Checksum of bundletool-all-$(BUNDLETOOL_VERSION).jar. Bump both together.
BUNDLETOOL_SHA256  ?= 2d4ad908faea64047c1cc9cb747e6aa667c6ab192e09607bd16b67246a8cd6ae

PLAY_KEYSTORE   ?= $(ANDROID_KEYSTORE)
PLAY_KEY_ALIAS  ?= openklondike
PLAY_STORE_PASS ?= android
PLAY_KEY_PASS   ?= android

android-play: $(ANDROID_AAB)

# Downloaded to .tmp and renamed only after the checksum matches. The rename
# matters as much as the check: this jar is run with `java -jar` in the same job
# that has just decoded the Play upload keystore to disk, and leaving a rejected
# download at $@ would let the next run's existence test accept it unverified.
$(BUNDLETOOL):
	@mkdir -p $(dir $@)
	curl -fsSL -o $@.tmp \
	    https://github.com/google/bundletool/releases/download/$(BUNDLETOOL_VERSION)/bundletool-all-$(BUNDLETOOL_VERSION).jar
	echo "$(BUNDLETOOL_SHA256)  $@.tmp" | sha256sum -c - || { rm -f $@.tmp; exit 1; }
	mv $@.tmp $@

# Exported rather than passed on the command line, so the passwords reach
# jarsigner through the environment and appear in neither the build log nor the
# process table.
$(ANDROID_AAB): export PLAY_STORE_PASS_ENV = $(PLAY_STORE_PASS)
$(ANDROID_AAB): export PLAY_KEY_PASS_ENV   = $(PLAY_KEY_PASS)
$(ANDROID_AAB): $(ANDROID_LIB) $(ANDROID_DEX) $(BUNDLETOOL) $(PLAY_KEYSTORE) \
                android/AndroidManifest.xml android/res/values/styles.xml
	@rm -rf build/aab && mkdir -p build/aab/module/manifest \
	    build/aab/module/lib/$(ANDROID_ABI) build/aab/module/dex
	# Compile android/res, then link into a *protobuf* APK (bundletool's input).
	$(ANDROID_SDK_BT)/aapt2 compile --dir android/res -o build/aab/res.zip
	$(ANDROID_SDK_BT)/aapt2 link --proto-format -o build/aab/proto.apk \
	    -I $(ANDROID_JAR) --manifest android/AndroidManifest.xml \
	    -R build/aab/res.zip --auto-add-overlay \
	    --version-code $(ANDROID_VERSION_CODE) --version-name $(ANDROID_VERSION_NAME)
	# Re-lay the proto APK into bundletool's base-module layout, add the .so.
	cd build/aab && unzip -qo proto.apk -d proto
	mv build/aab/proto/AndroidManifest.xml build/aab/module/manifest/AndroidManifest.xml
	mv build/aab/proto/resources.pb        build/aab/module/resources.pb
	mv build/aab/proto/res                 build/aab/module/res
	cp $(ANDROID_LIB) build/aab/module/lib/$(ANDROID_ABI)/libopenklondike.so
	cp $(ANDROID_DEX) build/aab/module/dex/classes.dex
	cd build/aab/module && zip -qr ../module.zip manifest resources.pb res lib dex
	java -jar $(BUNDLETOOL) build-bundle --modules=build/aab/module.zip --output=$@
	# Sign the bundle (JAR signature) with the upload key.
	@jarsigner -keystore $(PLAY_KEYSTORE) -storepass:env PLAY_STORE_PASS_ENV \
	    -keypass:env PLAY_KEY_PASS_ENV -sigalg SHA256withRSA -digestalg SHA-256 \
	    $@ $(PLAY_KEY_ALIAS)
	@echo "[android] built $@ (versionCode $(ANDROID_VERSION_CODE), versionName $(ANDROID_VERSION_NAME))"

# ---------------------------------------------------------------------------
# Web build (WebAssembly via Emscripten). CI-only: needs emcc (emsdk) on PATH.
# Carries BOTH board layouts and picks at runtime -- a phone browser gets the
# touch board, a desktop browser the fixed one plus a mouse -- and drives the
# loop from emscripten_set_main_loop. The recorder is stubbed, so the
# encode_h264/mux TUs and minih264/minimp4 includes are dropped (as on Android).
# Outputs build/web/openklondike.{html,js,wasm} with the shell in web/shell.html.
# ---------------------------------------------------------------------------
RAYLIB_WEB := third_party/raylib-install-web

WEB_SRC     := $(filter-out src/encode_h264.c src/encode_mux.c,$(SRC))
WEB_CFLAGS  := -std=c99 -Wall -Wextra -Isrc -DPLATFORM_WEB -Os \
               -I$(RAYLIB_WEB)/include
# Fixed memory (not ALLOW_MEMORY_GROWTH): a growable WASM heap yields resizable
# ArrayBuffers, which modern browsers reject in WebGL texImage2D. 64 MiB is ample
# for this game.
WEB_LDFLAGS := -Os -sUSE_GLFW=3 -sINITIAL_MEMORY=67108864 \
               --shell-file web/shell.html $(RAYLIB_WEB)/lib/libraylib.a

WEB_OUT_DIR := build/web
WEB_OUT     := $(WEB_OUT_DIR)/openklondike.html

web: $(WEB_OUT)

$(WEB_OUT): $(WEB_SRC) $(wildcard src/*.h) web/shell.html | $(WEB_OUT_DIR)
	emcc $(WEB_CFLAGS) $(WEB_SRC) -o $@ $(WEB_LDFLAGS)
	@echo "[web] built $@"

# Serve the built game locally. Browsers refuse to fetch the .wasm over
# file://, so a real HTTP server is required to run it at all.
web-serve: $(WEB_OUT)
	@echo "openklondike: http://localhost:8080/openklondike.html"
	@cd $(WEB_OUT_DIR) && python3 -m http.server 8080

# ---------------------------------------------------------------------------
# iOS (native Metal, no raylib). CI-only: needs Xcode on a macOS runner. The
# shared game TUs compile -DPLATFORM_IOS (touch board, no raylib); the ios/
# Objective-C++ files provide the Metal renderer, the platform/touch layer, and
# the UIKit app shell. Two products:
#   ios-sim  -- Simulator .app (arm64 simulator, unsigned) for CI screenshots.
#   ios      -- device .ipa (arm64, unsigned). AWS Device Farm re-signs it with
#               its own profile, so it needs no Apple Developer account.
# The .app/.ipa are assembled by hand (clang + Info.plist + zip), no Xcode
# project -- mirroring the no-Gradle Android approach. C sources are built with
# clang (C99); the .mm sources with clang++ (Obj-C++); linked with clang++.
# ---------------------------------------------------------------------------
IOS_MIN        ?= 15.0
IOS_APP_NAME   := Openklondike
IOS_BUNDLE_ID  := com.danheskett.openklondike
# CFBundleVersion must increase with every App Store upload, so it tracks the
# release number exactly like ANDROID_VERSION_CODE. Clamped to >= 1 for local
# builds with no release tags yet.
IOS_BUILD_NUMBER ?= $(OPENKLONDIKE_VERSION)
ifeq ($(IOS_BUILD_NUMBER),0)
IOS_BUILD_NUMBER := 1
endif
IOS_VERSION_NAME ?= 1.0.$(IOS_BUILD_NUMBER)
# Signing is opt-in: set IOS_SIGN_IDENTITY (and IOS_PROFILE) to produce an
# App Store-submittable .ipa. Unset, the build stays unsigned for Device Farm,
# which re-signs on upload. Mirrors how the Play AAB gates on a keystore.
IOS_SIGN_IDENTITY ?=
IOS_PROFILE       ?=
IOS_TEAM_ID       ?=
# The raylib-backed TUs (gfx_raylib.c, audio_raylib.c) and the recorder encoders
# are replaced by the ios/ backends, so they are not in this list.
IOS_C_SRC      := src/main.c src/game.c src/tick.c src/input.c \
                  src/render.c src/render_fixed.c src/render_scaled.c \
                  src/safe_area.c src/sound.c src/recorder.c
IOS_MM_SRC     := ios/ios_main.mm ios/gfx_metal.mm ios/plat_ios.mm ios/audio_ios.mm
IOS_CFLAGS     := -std=c99   -Wall -Wextra -Isrc -Iios -DPLATFORM_IOS -O2
IOS_MMFLAGS    := -std=c++14 -fobjc-arc -Wall -Wextra -Isrc -Iios -DPLATFORM_IOS -O2
IOS_FRAMEWORKS := -framework UIKit -framework Metal -framework QuartzCore \
                  -framework CoreGraphics -framework AVFoundation -framework Foundation
IOS_DEPS       := $(IOS_C_SRC) $(IOS_MM_SRC) $(wildcard src/*.h ios/*.h) ios/Info.plist \
                  $(wildcard ios/Assets.xcassets/*/* ios/Assets.xcassets/*)

# $(call ios_build,<sdk>,<target-triple>,<app-dir>,<obj-dir>) -- compile + link
# the app binary into <app-dir>/$(IOS_APP_NAME) and copy the Info.plist.
define ios_build
	@rm -rf $(4) && mkdir -p $(3) $(4)
	for f in $(IOS_C_SRC);  do xcrun -sdk $(1) clang   -target $(2) $(IOS_CFLAGS)  -c $$f -o $(4)/$$(basename $$f .c).o  || exit 1; done
	for f in $(IOS_MM_SRC); do xcrun -sdk $(1) clang++ -target $(2) $(IOS_MMFLAGS) -c $$f -o $(4)/$$(basename $$f .mm).o || exit 1; done
	xcrun -sdk $(1) clang++ -target $(2) $(4)/*.o $(IOS_FRAMEWORKS) -o $(3)/$(IOS_APP_NAME)
	cp ios/Info.plist $(3)/Info.plist
endef

IOS_SIM_APP := build/ios-sim/$(IOS_APP_NAME).app
ios-sim: $(IOS_SIM_APP)
$(IOS_SIM_APP): $(IOS_DEPS)
	$(call ios_build,iphonesimulator,arm64-apple-ios$(IOS_MIN)-simulator,build/ios-sim/$(IOS_APP_NAME).app,build/ios-sim/obj)
	@echo "[ios] built $(IOS_SIM_APP)"

# Device .ipa: unsigned; a .ipa is just a zip of Payload/<App>.app. Xcode injects
# device-platform Info.plist keys that a hand-assembled bundle lacks; add the
# ones tools require -- notably CFBundleSupportedPlatforms, which AWS Device Farm
# checks on upload ("could not find the platform value in the Info.plist").
IOS_IPA := build/openklondike.ipa
IOS_APP_DIR := build/ios-device/Payload/$(IOS_APP_NAME).app
ios: $(IOS_IPA)
$(IOS_IPA): $(IOS_DEPS)
	$(call ios_build,iphoneos,arm64-apple-ios$(IOS_MIN),$(IOS_APP_DIR),build/ios-device/obj)
	plist=$(IOS_APP_DIR)/Info.plist; \
	/usr/libexec/PlistBuddy \
	    -c "Add :CFBundleSupportedPlatforms array" \
	    -c "Add :CFBundleSupportedPlatforms:0 string iPhoneOS" \
	    -c "Add :DTPlatformName string iphoneos" \
	    -c "Add :UIRequiredDeviceCapabilities array" \
	    -c "Add :UIRequiredDeviceCapabilities:0 string arm64" \
	    -c "Set :CFBundleVersion $(IOS_BUILD_NUMBER)" \
	    -c "Set :CFBundleShortVersionString $(IOS_VERSION_NAME)" \
	    "$$plist"
	@# App icon. A bundle with no icon is auto-rejected at upload. actool
	@# compiles the catalog to Assets.car and emits the CFBundleIcons /
	@# CFBundleIconName keys into a partial plist, which we merge in.
	xcrun actool ios/Assets.xcassets --compile $(IOS_APP_DIR) \
	    --platform iphoneos --minimum-deployment-target $(IOS_MIN) \
	    --target-device iphone --app-icon AppIcon \
	    --output-partial-info-plist build/ios-device/assetcatalog.plist >/dev/null
	/usr/libexec/PlistBuddy -c "Merge build/ios-device/assetcatalog.plist" \
	    $(IOS_APP_DIR)/Info.plist
	@# actool only writes CFBundleIconName nested under CFBundleIcons, but the
	@# App Store also requires it at the top level -- without it the upload
	@# fails with ITMS-90713 "Missing Info.plist value". plutil -replace adds
	@# the key when absent, unlike PlistBuddy's Add/Set split.
	plutil -replace CFBundleIconName -string AppIcon $(IOS_APP_DIR)/Info.plist
	@# Toolchain provenance. Xcode injects these; a hand-assembled bundle has
	@# none, and App Store Connect reads DTXcodeBuild to identify the toolchain
	@# -- without it, review refuses the build as "using a beta version of
	@# Xcode". DTPlatformVersion is the SDK version, NOT the deployment target.
	plist=$(IOS_APP_DIR)/Info.plist; \
	xcode_ver=$$(xcodebuild -version | sed -n '1s/^Xcode //p'); \
	xcode_build=$$(xcodebuild -version | sed -n '2s/^Build version //p'); \
	sdk_ver=$$(xcrun --sdk iphoneos --show-sdk-version); \
	sdk_build=$$(xcrun --sdk iphoneos --show-sdk-build-version); \
	plat_ver=$$(xcrun --sdk iphoneos --show-sdk-platform-version); \
	dtxcode=$$(echo $$xcode_ver | awk -F. '{printf "%02d%d%d", $$1, $$2+0, $$3+0}'); \
	plutil -replace DTXcode            -string "$$dtxcode"            $$plist; \
	plutil -replace DTXcodeBuild       -string "$$xcode_build"        $$plist; \
	plutil -replace DTSDKName          -string "iphoneos$$sdk_ver"    $$plist; \
	plutil -replace DTSDKBuild         -string "$$sdk_build"          $$plist; \
	plutil -replace DTPlatformVersion  -string "$$plat_ver"           $$plist; \
	plutil -replace DTPlatformBuild    -string "$$sdk_build"          $$plist; \
	plutil -replace DTCompiler         -string "com.apple.compilers.llvm.clang.1_0" $$plist; \
	plutil -replace BuildMachineOSBuild -string "$$(sw_vers -buildVersion)" $$plist; \
	echo "[ios] toolchain: Xcode $$xcode_ver ($$xcode_build), iphoneos SDK $$sdk_ver ($$sdk_build)"
	@# Sign, when an identity is supplied. Entitlements must be a subset of the
	@# provisioning profile's, so keep them minimal.
	@if [ -n "$(IOS_SIGN_IDENTITY)" ]; then \
	    if [ -z "$(IOS_PROFILE)" ]; then echo "error: IOS_SIGN_IDENTITY set but IOS_PROFILE is empty" >&2; exit 1; fi; \
	    if [ -z "$(IOS_TEAM_ID)" ]; then echo "error: IOS_SIGN_IDENTITY set but IOS_TEAM_ID is empty" >&2; exit 1; fi; \
	    cp "$(IOS_PROFILE)" $(IOS_APP_DIR)/embedded.mobileprovision; \
	    ents=build/ios-device/entitlements.plist; \
	    printf '%s\n' \
	      '<?xml version="1.0" encoding="UTF-8"?>' \
	      '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' \
	      '<plist version="1.0"><dict>' \
	      '  <key>application-identifier</key><string>$(IOS_TEAM_ID).$(IOS_BUNDLE_ID)</string>' \
	      '  <key>com.apple.developer.team-identifier</key><string>$(IOS_TEAM_ID)</string>' \
	      '  <key>get-task-allow</key><false/>' \
	      '</dict></plist>' > $$ents; \
	    codesign --force --timestamp=none \
	        --sign "$(IOS_SIGN_IDENTITY)" --entitlements $$ents $(IOS_APP_DIR); \
	    codesign --verify --strict --verbose=2 $(IOS_APP_DIR); \
	fi
	cd build/ios-device && rm -f ../openklondike.ipa && zip -qr ../openklondike.ipa Payload
	@if [ -n "$(IOS_SIGN_IDENTITY)" ]; then \
	    echo "[ios] built $(IOS_IPA) (signed: $(IOS_SIGN_IDENTITY), build $(IOS_BUILD_NUMBER))"; \
	else \
	    echo "[ios] built $(IOS_IPA) (unsigned; AWS Device Farm re-signs on upload)"; \
	fi

# ---------------------------------------------------------------------------
# Unit tests (no raylib/window needed). Each test TU includes the sources under
# test directly to reach their file-static helpers and state.
#   test_game   — game.c + tick.c (deal, legality, scoring, the fixed-timestep
#                 clock that keeps the play timer at 60 Hz on any display)
#   test_layout — the two board layouts, compiled -DPLATFORM_IOS so ok_types.h
#                 supplies the geometry types without raylib, and with
#                 OK_FIXED forced on so both can be exercised in one binary
#   test_input  — input.c's touch-gesture recognizer, driven frame-by-frame
#                 through a scripted touch surface. It is the same C every touch
#                 platform compiles.
# ---------------------------------------------------------------------------
TEST_BIN        := build/test_game
TEST_LAYOUT_BIN := build/test_layout
TEST_INPUT_BIN  := build/test_input

test: $(TEST_BIN) $(TEST_LAYOUT_BIN) $(TEST_INPUT_BIN)
	./$(TEST_BIN)
	./$(TEST_LAYOUT_BIN)
	./$(TEST_INPUT_BIN)

$(TEST_BIN): tests/test_game.c $(wildcard src/*.c src/*.h) | $(OBJ_DIR)
	gcc $(CFLAGS_COMMON) -O0 -g tests/test_game.c -o $(TEST_BIN) -lm

$(TEST_LAYOUT_BIN): tests/test_layout.c $(wildcard src/*.c src/*.h) | $(OBJ_DIR)
	gcc $(CFLAGS_COMMON) -O0 -g tests/test_layout.c -o $(TEST_LAYOUT_BIN) -lm

$(TEST_INPUT_BIN): tests/test_input.c $(wildcard src/*.c src/*.h) | $(OBJ_DIR)
	gcc $(CFLAGS_COMMON) -O0 -g -DPLATFORM_IOS tests/test_input.c -o $(TEST_INPUT_BIN) -lm

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

# The APK is a self-contained installable, so it ships as-is (versioned name).
dist-android: $(ANDROID_APK)
	@mkdir -p $(DIST)
	cp $(ANDROID_APK) $(DIST)/openklondike-$(VERSION_SLUG)-android-arm64.apk

# The Play upload artifact: the signed App Bundle (see the android-play target).
dist-android-play: $(ANDROID_AAB)
	@mkdir -p $(DIST)
	cp $(ANDROID_AAB) $(DIST)/openklondike-$(VERSION_SLUG)-android.aab

dist-ios: $(IOS_IPA)
	@mkdir -p $(DIST)
	cp $(IOS_IPA) $(DIST)/openklondike-$(VERSION_SLUG)-ios-arm64.ipa

# The web build ships as a zip of the HTML/JS/WASM (serve over http to play).
dist-web: $(WEB_OUT)
	@rm -rf $(STAGING)/web && mkdir -p $(STAGING)/web/openklondike-$(VERSION_SLUG)-web $(DIST)
	cp $(WEB_OUT_DIR)/openklondike.html $(WEB_OUT_DIR)/openklondike.js $(WEB_OUT_DIR)/openklondike.wasm \
	    $(STAGING)/web/openklondike-$(VERSION_SLUG)-web/
	cp $(DOCS) $(STAGING)/web/openklondike-$(VERSION_SLUG)-web/
	(cd $(STAGING)/web && zip -qr ../../../$(DIST)/openklondike-$(VERSION_SLUG)-web-wasm.zip openklondike-$(VERSION_SLUG)-web)

# ---------------------------------------------------------------------------
$(OBJ_DIR) $(REL_OBJ_DIR) $(WIN64_OBJ_DIR) $(WIN32_OBJ_DIR) $(MAC_OBJ_DIR) $(ANDROID_OBJ_DIR) $(WEB_OUT_DIR):
	mkdir -p $@

clean:
	rm -rf build dist

# Pull in auto-generated header dependencies (ignored if not yet present).
-include $(OBJ:.o=.d) $(REL_OBJ:.o=.d) $(WIN64_OBJ:.o=.d) $(WIN32_OBJ:.o=.d) $(MAC_OBJ:.o=.d)

.PHONY: all run release run-release windows mac web web-serve test clean \
        android android-play ios ios-sim \
        dist dist-linux dist-windows dist-mac dist-web dist-android dist-android-play dist-ios
