#ifndef OPENKLONDIKE_APP_H
#define OPENKLONDIKE_APP_H

// iOS entry points. On iOS, UIKit owns main() and the run loop, so the app shell
// (ios/ios_main.mm) drives the game via these instead: ok_app_init() once at
// startup, then ok_app_frame() every CADisplayLink tick. Implemented in main.c
// (guarded for PLATFORM_IOS, where the normal main() is compiled out). Defined
// in C, called from Objective-C++ (ios_main.mm), so these need C linkage.
#ifdef __cplusplus
extern "C" {
#endif
void ok_app_init(void);
void ok_app_frame(void);
#ifdef __cplusplus
}
#endif

#endif // OPENKLONDIKE_APP_H
