#ifndef OPENKLONDIKE_OK_TYPES_H
#define OPENKLONDIKE_OK_TYPES_H

// Geometry / colour types and the handful of raylib query functions the shared
// game code uses, decoupled from raylib so the iOS build (which links no raylib)
// still compiles. On every raylib platform this is just <raylib.h>, so those
// builds are completely unchanged. On iOS we provide layout-identical structs,
// the named colours the renderer uses, and declarations for the queries that
// plat_ios implements (screen size, touch, gestures, time, focus).

#if !defined(PLATFORM_IOS)

#include <raylib.h>

#else // PLATFORM_IOS: no raylib -- provide the compatible surface ourselves.

#include <stdbool.h>

typedef struct { float x, y; }                Vector2;
typedef struct { float x, y, width, height; } Rectangle;
typedef struct { unsigned char r, g, b, a; }  Color; // identical layout to raylib

// raylib's named colours, exact RGBA, so the renderer's colour literals resolve.
#define BLACK     ((Color){   0,   0,   0, 255 })
#define WHITE     ((Color){ 255, 255, 255, 255 })
#define LIGHTGRAY ((Color){ 200, 200, 200, 255 })
#define GRAY      ((Color){ 130, 130, 130, 255 })
#define DARKGRAY  ((Color){  80,  80,  80, 255 })
#define YELLOW    ((Color){ 253, 249,   0, 255 })
#define RED       ((Color){ 230,  41,  55, 255 })

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// Pure-geometry helper raylib normally provides.
static inline bool CheckCollisionPointRec(Vector2 p, Rectangle r) {
    return p.x >= r.x && p.x < r.x + r.width &&
           p.y >= r.y && p.y < r.y + r.height;
}

// Gesture ids (values match raylib's enum so input.c comparisons are unchanged).
enum {
    GESTURE_TAP        = 1,
    GESTURE_DOUBLETAP  = 2,
    GESTURE_SWIPE_UP   = 16,
    GESTURE_SWIPE_DOWN = 32,
};

// These are defined in Objective-C++ (plat_ios.mm) but called from the C game
// code, so they need C linkage to link. TextFormat matches raylib's semantics
// (a pointer into an internal rotating buffer); the queries are UIKit-backed and
// keep raylib's names so the renderer / input.c call sites are unchanged.
#ifdef __cplusplus
extern "C" {
#endif
const char* TextFormat(const char* text, ...);
int     GetScreenWidth(void);
int     GetScreenHeight(void);
int     GetTouchPointCount(void);
Vector2 GetTouchPosition(int index);
int     GetGestureDetected(void);
double  GetTime(void);
bool    IsWindowFocused(void);
bool    WindowShouldClose(void);
#ifdef __cplusplus
}
#endif

#endif // PLATFORM_IOS

#endif // OPENKLONDIKE_OK_TYPES_H
