// iPhone/iPad (blz/ios-native): the native renderer must not submit GPU work while the app is not active -
// Metal refuses it ("Insufficient Permission (to submit GPU work from background)"), MoltenVK reports
// VK_ERROR_DEVICE_LOST and DiligentCore throws on the next swap chain resize (crash 17 Nov 2026 11:27). The guest's
// render thread waits in AfterSwap until the app is active again. Compiled as Objective-C++ (CMake flags).
#include "ios_lifecycle.h"

#if defined(__APPLE__)
#import <UIKit/UIKit.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace renderer {
namespace {
std::atomic<bool> g_active{true};
}

void InstallAppLifecycleObservers() {
  static bool installed = false;
  if (installed) return;
  installed = true;
  NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
  [center addObserverForName:UIApplicationWillResignActiveNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification*) { g_active = false; }];
  [center addObserverForName:UIApplicationDidEnterBackgroundNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification*) { g_active = false; }];
  [center addObserverForName:UIApplicationDidBecomeActiveNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification*) { g_active = true; }];
}

bool AppIsActive() { return g_active.load(); }

bool WaitWhileInactive() {
  if (g_active.load()) return false;
  while (!g_active.load()) std::this_thread::sleep_for(std::chrono::milliseconds(50));
  return true;
}
}  // namespace renderer
#endif
