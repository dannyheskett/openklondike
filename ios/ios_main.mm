// iOS app shell: a UIKit application hosting a CAMetalLayer view driven by a
// CADisplayLink. No Storyboard, no scene manifest — a classic AppDelegate
// window. The display link runs the shared game loop (ok_app_frame); touches and
// gesture recognizers feed the platform layer (plat_ios) that the game polls.
#import <UIKit/UIKit.h>

#import "app.h"
#import "ok_types.h"
#import "gfx_metal.h"
#import "plat_ios.h"

@interface OKMetalView : UIView
@property (nonatomic) BOOL started;
@property (nonatomic) NSInteger originX; // safe-area left inset, pixels
@property (nonatomic) NSInteger originY; // safe-area top inset, pixels
@end

@implementation OKMetalView

+ (Class)layerClass { return [CAMetalLayer class]; }

- (void)didMoveToWindow {
    [super didMoveToWindow];
    if (!self.window || self.started) return;
    self.started = YES;

    self.multipleTouchEnabled = YES;
    gfx_metal_attach((CAMetalLayer*)self.layer);
    [self updateDrawableSize];
    ok_app_init();

    // Gesture recognizers (don't swallow the raw touches the game also reads).
    UITapGestureRecognizer* tap =
        [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(onTap:)];
    tap.cancelsTouchesInView = NO;
    [self addGestureRecognizer:tap];
    UISwipeGestureRecognizer* up =
        [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(onSwipeUp:)];
    up.direction = UISwipeGestureRecognizerDirectionUp;
    up.cancelsTouchesInView = NO;
    [self addGestureRecognizer:up];
    UISwipeGestureRecognizer* down =
        [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(onSwipeDown:)];
    down.direction = UISwipeGestureRecognizerDirectionDown;
    down.cancelsTouchesInView = NO;
    [self addGestureRecognizer:down];
    UISwipeGestureRecognizer* left =
        [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(onSwipeLeft:)];
    left.direction = UISwipeGestureRecognizerDirectionLeft;
    left.cancelsTouchesInView = NO;
    [self addGestureRecognizer:left];
    UISwipeGestureRecognizer* right =
        [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(onSwipeRight:)];
    right.direction = UISwipeGestureRecognizerDirectionRight;
    right.cancelsTouchesInView = NO;
    [self addGestureRecognizer:right];

    CADisplayLink* link = [CADisplayLink displayLinkWithTarget:self selector:@selector(onFrame:)];
    link.preferredFramesPerSecond = 60;
    [link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

- (void)layoutSubviews { [super layoutSubviews]; [self updateDrawableSize]; }
- (void)safeAreaInsetsDidChange { [super safeAreaInsetsDidChange]; [self updateDrawableSize]; }

- (void)updateDrawableSize {
    CAMetalLayer* layer = (CAMetalLayer*)self.layer;
    CGFloat scale = self.window ? self.window.screen.scale : [UIScreen mainScreen].scale;
    self.contentScaleFactor = scale;
    layer.contentsScale = scale;
    CGSize full = CGSizeMake(self.bounds.size.width * scale, self.bounds.size.height * scale);
    if (full.width  < 1) full.width  = 1;
    if (full.height < 1) full.height = 1;
    layer.drawableSize = full;

    // Draw inside the safe area: the game's (0,0)..(w,h) maps to the region
    // between the notch/Dynamic Island and the home indicator. The area outside
    // clears to black, which matches the game background, so it's seamless.
    UIEdgeInsets ins = self.safeAreaInsets;
    NSInteger ox = (NSInteger)(ins.left * scale);
    NSInteger oy = (NSInteger)(ins.top  * scale);
    NSInteger sw = (NSInteger)full.width  - ox - (NSInteger)(ins.right  * scale);
    NSInteger sh = (NSInteger)full.height - oy - (NSInteger)(ins.bottom * scale);
    if (sw < 1) sw = 1;
    if (sh < 1) sh = 1;
    self.originX = ox;
    self.originY = oy;
    gfx_metal_set_viewport((int)full.width, (int)full.height, (int)ox, (int)oy);
    plat_ios_set_screen((int)sw, (int)sh);
}

- (void)onFrame:(CADisplayLink*)link { (void)link; ok_app_frame(); }

// --- Touches: publish the set of active points (in drawable pixels). --------
- (void)publishTouches:(UIEvent*)event {
    NSSet<UITouch*>* all = [event allTouches];
    Vector2 pts[16];
    int n = 0;
    CGFloat scale = self.contentScaleFactor;
    for (UITouch* t in all) {
        if (t.phase == UITouchPhaseEnded || t.phase == UITouchPhaseCancelled) continue;
        if (n >= 16) break;
        CGPoint p = [t locationInView:self];
        // Into the game's safe-area coordinate space (matches what's drawn).
        pts[n].x = (float)(p.x * scale - self.originX);
        pts[n].y = (float)(p.y * scale - self.originY);
        n++;
    }
    plat_ios_set_touches(pts, n);
}
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { (void)touches; [self publishTouches:event]; }
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { (void)touches; [self publishTouches:event]; }
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { (void)touches; [self publishTouches:event]; }
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { (void)touches; [self publishTouches:event]; }

- (void)onTap:(UITapGestureRecognizer*)g       { (void)g; plat_ios_post_gesture(GESTURE_TAP); }
- (void)onSwipeUp:(UISwipeGestureRecognizer*)g  { (void)g; plat_ios_post_gesture(GESTURE_SWIPE_UP); }
- (void)onSwipeDown:(UISwipeGestureRecognizer*)g{ (void)g; plat_ios_post_gesture(GESTURE_SWIPE_DOWN); }
- (void)onSwipeLeft:(UISwipeGestureRecognizer*)g  { (void)g; plat_ios_post_gesture(GESTURE_SWIPE_LEFT); }
- (void)onSwipeRight:(UISwipeGestureRecognizer*)g { (void)g; plat_ios_post_gesture(GESTURE_SWIPE_RIGHT); }

@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow* window;
@end

@implementation AppDelegate

- (BOOL)application:(UIApplication*)application
        didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
    (void)application; (void)launchOptions;
    CGRect bounds = [UIScreen mainScreen].bounds;
    self.window = [[UIWindow alloc] initWithFrame:bounds];
    UIViewController* vc = [[UIViewController alloc] init];
    vc.view = [[OKMetalView alloc] initWithFrame:bounds];
    self.window.rootViewController = vc;
    [self.window makeKeyAndVisible];

    [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationDidBecomeActiveNotification
        object:nil queue:nil usingBlock:^(NSNotification* n){ (void)n; plat_ios_set_focus(true); }];
    [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationWillResignActiveNotification
        object:nil queue:nil usingBlock:^(NSNotification* n){ (void)n; plat_ios_set_focus(false); }];
    return YES;
}

@end

int main(int argc, char* argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}
