// MoltenVK settings for iPhone/iPad (blz/ios-native), set before MoltenVK reads its configuration (it does so
// on first use, long after static initialisation). A value already in the environment (devicectl launch
// --environment-variables) wins, so each one can still be A/B tested without a rebuild.
#if defined(__APPLE__)
#include <cstdlib>

namespace {
struct MoltenVkEnvironment {
  MoltenVkEnvironment() {
    // Encode Metal command buffers on MoltenVK's own queue thread instead of inside vkQueueSubmit on the
    // caller: renderer=dante submits from the game's render thread ("Presentation"), where MoltenVK's encode
    // was ~48% of that thread's time (Time Profiler, iPhone 15 Pro Max, 17 Nov 2026).
    setenv("MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS", "0", 0);
  }
} g_moltenvk_environment;
}  // namespace
#endif
