// Storage for the top display-cutout geometry, plus the JNI entry point the
// Android Activity calls to populate it. Compiles on every platform (the
// accessor just returns zeros where nothing sets the values); the JNI export
// only exists on Android.
#include "safe_area.h"

static int s_top;          // safe-inset top, device px
static int s_cutout_left;  // cutout bounding box, device px
static int s_cutout_right;

void safe_area_get(int* top, int* cutout_left, int* cutout_right) {
    *top          = s_top;
    *cutout_left  = s_cutout_left;
    *cutout_right = s_cutout_right;
}

#if defined(PLATFORM_ANDROID)
#include <jni.h>

// Called from OpenklondikeActivity.pushSafeInsets(). Coordinates are already in
// the surface's pixel space (the app renders at native resolution), so the
// renderer can compare them against GetScreenWidth() directly.
JNIEXPORT void JNICALL
Java_com_danheskett_openklondike_OpenklondikeActivity_nativeSetSafeInsets(
    JNIEnv* env, jobject thiz, jint top, jint cutout_left, jint cutout_right) {
    (void)env;
    (void)thiz;
    s_top          = top;
    s_cutout_left  = cutout_left;
    s_cutout_right = cutout_right;
}
#endif
