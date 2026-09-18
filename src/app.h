#ifndef APP_H
#define APP_H

// iOS entry points. On iOS, UIKit owns main() and the run loop, so the app shell
// (ios/ios_main.mm) drives the game via these instead: app_init() once at
// startup, then app_frame() every CADisplayLink tick. Implemented in main.c
// (guarded for PLATFORM_IOS, where the normal main() is compiled out). Defined
// in C, called from Objective-C++ (ios_main.mm), so these need C linkage.
#ifdef __cplusplus
extern "C" {
#endif
void app_init(void);
void app_frame(void);
#ifdef __cplusplus
}
#endif

#endif // APP_H
