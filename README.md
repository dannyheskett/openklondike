# openklondike

Klondike Solitaire written in C, in the spirit of the classic Windows 95 game:
clean, familiar, and frill-free. It runs natively on Windows, macOS, Linux,
Android, and iOS, and in the browser via WebAssembly.

Rendering, input, and audio go through raylib 6.0 on every platform except iOS,
which uses a native Metal backend with no raylib (see
[Architecture](#architecture)). The game logic (`src/game.c`) is
platform-independent and shared unchanged.

## Platforms

| Platform | Build | Board | Orientation | Input |
|----------|-------|-------|-------------|-------|
| Linux / Windows / macOS | native (raylib) | fixed | resizable window | mouse |
| Web (WASM) | Emscripten (raylib) | fixed **or** scaled, chosen at runtime | follows the browser | mouse + touch |
| Android | NativeActivity (raylib) | scaled | portrait **and** landscape | touch |
| iOS | native Metal (no raylib) | scaled | portrait **and** landscape | touch |

Note that "fixed" and "scaled" describe the *board*, not the device orientation:
the scaled board is the one phones use, and it works in either orientation.

## The two boards

There is one renderer and two layouts. Native desktop compiles only the fixed
layout; Android and iOS compile only the scaled one; the web build compiles both
and selects at runtime from the pointer type
(`matchMedia('(pointer: coarse)')`), so a phone gets the touch board and a
desktop browser gets the same board as the native app.

- **Fixed** (desktop) — cards are exactly 80×112 and never scale. The board is
  laid out at a fixed pixel size and centred in the window; resizing moves only
  the margins. The window enforces a 704×704 minimum, which is what lets the
  card size stay fixed. A browser window cannot be constrained that way, so on
  web the fixed layout shrinks to fit rather than running off the edge.
- **Scaled** (touch) — the seven tableau columns are fitted to the live screen
  width, and every other metric (gaps, fans, bar heights, font sizes) is derived
  from the resulting card size at the same ratios the fixed layout uses. So the
  two are the same design at different sizes, not two designs. The face-up fan
  is spread wider than the desktop ratio, because a fingertip needs a bigger
  target than the sliver a mouse can hit. It is recomputed from the live screen
  size every frame, so **rotating the device just re-fits the board** — held
  sideways it asks for less vertical room per column (letting the draw-time fan
  compression take up the slack rather than shrinking the cards) and spends the
  spare width on wider column gaps.

Either way, a column deeper than the board is tall — up to six face-down cards
under a full King-to-Ace run — compresses its fan until it fits above the status
bar, so no card is ever off-screen.

## Controls

**Mouse** (desktop, and desktop browsers):

- **Left-click the stock** to deal to the waste; click the empty stock to
  recycle the waste back
- **Drag** a card, or a valid descending alternating-colour run, onto a tableau
  column or a foundation
- **Double-click** a card to send it straight to its foundation
- **Right-click** a card as a shortcut for the same thing
- **Escape**: menu &nbsp;·&nbsp; **Alt+Enter**: toggle fullscreen
- **Up / Down** (or W / S) + **Enter / Space**: menu navigation;
  **Left / Right** (or A / D) cycle a value on the Options screen

**Touch** (Android, iOS, and mobile browsers) — the grammar every mobile card
game uses:

- **Tap the stock**: deal, or recycle when it is empty
- **Tap a card**: send it home — to a foundation if it will go, otherwise onto
  the best tableau build
- **Drag**: move a card or a run. It lifts clear of your fingertip once the
  gesture is unambiguously a drag, and the pile it would legally land on is
  highlighted before you let go
- **Two-finger tap**: menu (the game stays resumable)
- **Tap a menu row** to choose it — on the Options screen, tapping a row cycles
  its value; **swipe up / down** moves the selection, **left / right** cycles

## Building

raylib is built once from source into a gitignored install directory (per
platform) before the game is built. Each `scripts/build_raylib_*.sh` clones
raylib (pinned via `RAYLIB_TAG`, default `6.0`) and installs its headers and
`libraylib.a`. CI runs these scripts before each build.

### Desktop

```bash
./scripts/build_raylib_linux.sh      # once, on a fresh clone
make                                 # -> build/openklondike   (dev, -O2)
make run
make release                         # -> build/openklondike-release (-O3)
```

Windows (mingw-w64 cross-compile) and macOS (universal arm64 + x86_64):

```bash
./scripts/build_raylib_windows.sh && make windows  # -> build/openklondike-x64.exe, -x86.exe
./scripts/build_raylib_mac.sh     && make mac      # -> build/openklondike-mac
```

### Android (needs the Android SDK + NDK)

```bash
./scripts/build_raylib_android.sh
make android        # -> build/openklondike.apk   (debug-signed, sideloadable)
make android-play   # -> build/openklondike.aab   (Play App Bundle; PLAY_* signing vars)
```

The app is a `NativeActivity` (no Gradle); a small `OpenklondikeActivity` Java
class (compiled with `javac` + `d8`) enables immersive full-screen and forwards
the display cutout to the renderer. arm64-v8a, `targetSdk` 35, 16 KB-page
aligned.

### iOS (needs macOS + Xcode; no raylib)

```bash
make ios-sim   # -> build/ios-sim/Openklondike.app   (Simulator, arm64)
make ios       # -> build/openklondike.ipa           (device arm64, unsigned)
```

The `.ipa` is unsigned; AWS Device Farm re-signs it on upload, and Sideloadly /
a free Apple ID can install it on a device. C sources build with `clang`, the
Objective-C++ backend (`ios/`) with `clang++`.

### Web (needs Emscripten)

```bash
./scripts/build_raylib_web.sh
make web        # -> build/web/openklondike.{html,js,wasm}
make web-serve  # serve it at http://localhost:8080/openklondike.html
```

Serve `build/web` over HTTP (not `file://`) and open `openklondike.html`.

## Tests

Unit tests with no raylib and no window — `make test` runs all three:

- **`test_game`** — the deal, move legality, run moves, auto-flip, scoring, the
  stock/waste cycle, the win, and the fixed-timestep clock that keeps the play
  timer at 60 Hz on any display refresh.
- **`test_layout`** — both board layouts: that the desktop board is exactly the
  size it has always been, that the touch board fits every device shape inside
  its margins, that the deepest possible column stays on screen, and that a
  press on a card picks up that card and no other at every scale.
- **`test_input`** — the touch-gesture recognizer, driven frame-by-frame through
  a scripted touch surface: tap vs. drag, the tap slop, the long press, the
  two-finger tap, and swipes.

```bash
make test
```

There is also a manual real-device check: the
[`devicefarm`](.github/workflows/devicefarm.yml) workflow builds the APK and the
`.ipa` and runs an AWS Device Farm fuzz test on real hardware.

## Continuous integration and releases

Every pull request to `main` builds all platforms via GitHub Actions
([`ci.yml`](.github/workflows/ci.yml)) — Linux, Windows (x64/x86), macOS
(universal), Android (APK + a packaging check of the AAB), Web (WASM), and iOS
(a Metal app booted in the Simulator and screenshotted) — and runs `make test`.
All checks must pass to merge.

Pushing to `main` cuts the next `release-N` via
[`release.yml`](.github/workflows/release.yml), which attaches per-platform
archives, the Android APK, the iOS `.ipa`, and the WASM bundle to the GitHub
Release.

The store uploads (Play internal track, TestFlight) are wired up but dormant:
each gates on its own secrets, none of which are configured, so today they
report "skipping" and the release still ships the sideload APK and the unsigned
`.ipa`. See [`android/play-assets/KEYSTORE.md`](android/play-assets/KEYSTORE.md)
and [`ios/app-store-assets/TESTFLIGHT.md`](ios/app-store-assets/TESTFLIGHT.md)
for the step-by-step on turning each one on.

## Recording (desktop only)

The desktop builds can record a frame-fidelity H.264 MP4 of the session (one
video frame per rendered frame, constant 60 fps, no external tools). Recording
is compiled out of the mobile and web builds.

- Toggle **Record: On/Off** from the menu (writes an auto-named
  `openklondike-YYYYMMDD-HHMMSS.mp4` in the working directory), or start it from
  the command line:

```bash
./build/openklondike --record            # auto-named file
./build/openklondike --record clip.mp4   # explicit path
```

The game is re-rendered at a fixed canvas size for capture, so recordings stay
frame-exact regardless of the live window size.

## Architecture

- `src/game.c` — pure game logic (no rendering/input/audio), shared by all
  platforms and covered by `make test`.
- `src/render.c` draws every pixel through a small immediate-mode primitive
  layer (`src/gfx.h`): `src/gfx_raylib.c` wraps raylib (desktop / web /
  android); `ios/gfx_metal.mm` is a native Metal implementation. The primitive
  set is larger than a block game's because a playing card is a rounded
  rectangle carrying vector suit pips — rounded fills and outlines, circles for
  the club trefoil, raw triangles for the heart/spade/diamond fans.
- All geometry comes out of a `Layout` (`src/render_internal.h`), computed by
  `src/render_fixed.c` (fixed) or `src/render_scaled.c` (scaled). Those
  two files supply nothing but numbers; every pixel is drawn by the shared code,
  which is what makes the two boards one renderer rather than two to keep in
  sync.
- `src/ok_types.h` supplies raylib-compatible geometry types so the shared code
  compiles without raylib on iOS.
- Audio is a similar seam (`src/audio.h`): `src/audio_raylib.c` (raylib) vs
  `ios/audio_ios.mm` (AVAudioEngine). Effects are synthesized at startup (no
  audio files); sound is off by default.
- `src/tick.c` is a fixed-timestep accumulator. The play clock and its timed
  scoring penalty are counted in simulation steps, so a game on a 120 Hz phone
  runs at the same wall speed as one on a 60 Hz desktop.
- iOS backend: `ios/plat_ios.mm` (touch / screen / timing) and `ios/ios_main.mm`
  (UIKit app + `CAMetalLayer` view + `CADisplayLink` loop).
- All text is drawn in the bundled Nunito SemiBold typeface (SIL OFL, see
  `NOTICE`), embedded so there is no runtime asset file. The raylib backends
  `LoadFontFromMemory` the TTF from `src/font_nunito.h` (generated by
  `scripts/embed_ttf.py`); iOS samples a pre-baked glyph atlas in
  `src/font_atlas.h` (generated by `scripts/gen_font_atlas.c`). Both use the
  same proportional tracking, so every platform lays text out identically.

## Dependencies

- A C99 compiler (GCC or Clang); a C++ / Objective-C++ compiler for the iOS
  backend.
- [raylib](https://github.com/raysan5/raylib) 6.0 (static) on all platforms
  except iOS, built by the `scripts/build_raylib_*.sh` helpers.
- The MP4 recorder uses two vendored public-domain (CC0) single-header
  libraries: [minih264](third_party/minih264) (H.264 encoder) and
  [minimp4](third_party/minimp4) (MP4 muxer). No external tools or shared
  libraries.

## Project structure

```
openklondike/
├── src/            # shared C sources + gfx/audio raylib backends
├── ios/            # native Metal / UIKit backend (Objective-C++) + store assets
├── android/        # NativeActivity manifest, resources, Java activity + Play assets
├── web/            # Emscripten HTML shell
├── scripts/        # raylib build scripts, asset/font/screenshot generators
├── third_party/    # vendored single-header libs + the bundled Nunito font
├── tests/          # game-logic, layout, and gesture unit tests
├── docs/           # design notes
├── Makefile
├── LICENSE         # MIT (this project's own code)
└── NOTICE          # third-party attributions
```

## License

openklondike's own code is released under the [MIT License](LICENSE). The
vendored `minih264` and `minimp4` libraries are public domain (CC0), and the
bundled Nunito typeface is under the SIL Open Font License; see
[NOTICE](NOTICE) for attributions.
