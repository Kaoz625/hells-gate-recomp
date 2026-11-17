// iPhone/iPad side of the cheat menu: a three-finger tap anywhere opens it, and while it is open a clear
// view over everything (the game and its touch controls) takes the taps for the menu, so the touch controls
// underneath do not fire.

#include "cheats.h"

#import <UIKit/UIKit.h>

#if !__has_feature(objc_arc)
#error "cheat_touch_ios.mm must be compiled with -fobjc-arc"
#endif

@interface RexCheatTapTarget : NSObject <UIGestureRecognizerDelegate>
@end

@implementation RexCheatTapTarget
- (void)tapped:(UITapGestureRecognizer*)recognizer {
  if (recognizer.state == UIGestureRecognizerStateRecognized && !cheats::MenuOpen()) cheats::RequestOpen();
}
- (BOOL)gestureRecognizer:(UIGestureRecognizer*)a
    shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer*)b {
  return YES;
}
@end

@interface RexCheatCaptureView : UIView
@end

@implementation RexCheatCaptureView
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
}
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  CGSize size = self.bounds.size;
  if (size.width <= 0 || size.height <= 0) return;
  for (UITouch* touch in touches) {
    CGPoint p = [touch locationInView:self];
    cheats::PushTap(float(p.x / size.width), float(p.y / size.height));
  }
}
@end

namespace {

UIWindow* KeyWindow() {
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    if (![scene isKindOfClass:[UIWindowScene class]]) continue;
    for (UIWindow* window in ((UIWindowScene*)scene).windows) {
      if (window.isKeyWindow) return window;
    }
  }
  return nil;
}

RexCheatTapTarget* g_target = nil;
UITapGestureRecognizer* g_tap = nil;
RexCheatCaptureView* g_capture = nil;

void InstallOnMain() {
  if (g_tap) return;
  UIWindow* window = KeyWindow();
  if (!window) {
    // The window comes up after the app's setup; try again shortly.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC), dispatch_get_main_queue(), ^{
      InstallOnMain();
    });
    return;
  }
  g_target = [RexCheatTapTarget new];
  g_tap = [[UITapGestureRecognizer alloc] initWithTarget:g_target action:@selector(tapped:)];
  g_tap.numberOfTouchesRequired = 3;
  g_tap.cancelsTouchesInView = NO;
  g_tap.delaysTouchesBegan = NO;
  g_tap.delaysTouchesEnded = NO;
  g_tap.delegate = g_target;
  [window addGestureRecognizer:g_tap];
  NSLog(@"CHEATS: three-finger tap installed on the key window");
}

}  // namespace

namespace cheats {

void InstallTouchGesture() {
  dispatch_async(dispatch_get_main_queue(), ^{
    InstallOnMain();
  });
}

void SetTouchCapture(bool capture) {
  dispatch_async(dispatch_get_main_queue(), ^{
    if (capture) {
      UIWindow* window = KeyWindow();
      if (!window) return;
      if (!g_capture) {
        g_capture = [[RexCheatCaptureView alloc] initWithFrame:window.bounds];
        g_capture.backgroundColor = UIColor.clearColor;
        g_capture.multipleTouchEnabled = NO;
        g_capture.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
      }
      g_capture.frame = window.bounds;
      [window addSubview:g_capture];
      [window bringSubviewToFront:g_capture];
    } else if (g_capture) {
      [g_capture removeFromSuperview];
    }
  });
}

}  // namespace cheats
