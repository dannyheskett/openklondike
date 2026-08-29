#ifndef OPENKLONDIKE_PLATFORM_H
#define OPENKLONDIKE_PLATFORM_H

// OK_TOUCH selects the touch-first frontend: the adaptive board layout that
// scales the cards to the screen, and the tap/drag gesture grammar mobile card
// games use. It is enabled on Android, iOS, and the WebAssembly build (which
// targets mobile browsers but also accepts a mouse for desktop browsers).
// Desktop native builds leave it unset and use the fixed-size board with a
// mouse.
//
// raylib defines PLATFORM_ANDROID / PLATFORM_WEB for its own sources; our build
// passes the matching -D for the game translation units.
#if defined(PLATFORM_ANDROID) || defined(PLATFORM_WEB) || defined(PLATFORM_IOS)
#define OK_TOUCH 1
#endif

// The two board layouts, selected by availability:
//   OK_SCALED    — the touch layout. The seven tableau columns are fitted to the
//                  live screen width, so the cards scale; every metric (fans,
//                  gaps, font sizes) is derived from the resulting card size.
//   OK_FIXED — the desktop layout. Cards are a fixed 80x112 and the board is
//                  centred in the window, exactly as before; only the surrounding
//                  margins flex. The window enforces a minimum size big enough to
//                  hold it (see MIN_W/MIN_H), so the fixed size always fits.
// Native desktop compiles only the fixed layout; Android and iOS only the scaled
// one; the web build compiles BOTH and chooses at runtime (desktop browser ->
// fixed, phone -> scaled), so a laptop browser gets the same look as the native
// app.
//
// These name the BOARD, not the device orientation. The scaled board runs in
// both orientations on a phone -- see android/AndroidManifest.xml and
// ios/Info.plist -- and re-fits itself on every rotation.
#ifdef OK_TOUCH
#define OK_SCALED 1
#endif
#if !defined(PLATFORM_ANDROID) && !defined(PLATFORM_IOS)
#define OK_FIXED 1
#endif

// True only on the build that has both layouts and must pick at runtime (web).
#if defined(OK_SCALED) && defined(OK_FIXED)
#define OK_RUNTIME_LAYOUT 1
#endif

#endif // OPENKLONDIKE_PLATFORM_H
