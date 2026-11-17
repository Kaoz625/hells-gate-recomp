// Frame-rate independence for the UI.
//
// The game runs its simulation at the video mode's refresh rate
// (video_mode_refresh_rate), with the time step derived from it. The UI does
// not: the APT (Flash) player is given a fixed time per update, and the screen
// logic (menus, HUD, minigames) counts updates, so above 60 fps the UI runs
// fast. All of it runs in one listener of the per-frame update message, the
// APT manager's handler; that handler is run at 60 Hz with the real time
// elapsed and the button presses since its last run. Refresh rates that are
// multiples of 60 keep the UI updates evenly spaced; other rates judder.

#include "guest_memory.h"

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging/macros.h>
#include <rex/ppc/context.h>

#include <chrono>
#include <cstdint>

namespace {

using namespace guest;

constexpr double kUiRate = 60.0;

// Per-frame update message (the struct sent by the UI step 0x8256E9D8); the
// handler runs the UI for the two message ids stored at these globals.
constexpr uint32_t kUpdateMessage = 0x82CC8930;
constexpr uint32_t kUiUpdateIds[2] = {0x82CC8974, 0x82CC8954};

// Game frame time in seconds (float).
constexpr uint32_t kGameFrameTime = 0x82CBEB60;

// Input manager pads: 4 records of 68 bytes from +60; buttons at +0, the
// previous frame's buttons at +2 (presses = buttons & ~previous).
constexpr uint32_t kPadsOffset = 60, kPadStride = 68, kPadCount = 4;

bool UiGated() {
  static const bool gated = rex::cvar::Query<double>("video_mode_refresh_rate") > kUiRate + 0.5;
  return gated;
}

// One UI update stream: runs once per 60 Hz real-time tick.
struct UiStream {
  int64_t last_tick = -1;
  float skipped_time = 0.0f;
  uint16_t buttons_at_last_run[kPadCount] = {};

  bool Due() {
    using Clock = std::chrono::steady_clock;
    static const Clock::time_point start = Clock::now();
    int64_t tick = int64_t(std::chrono::duration<double>(Clock::now() - start).count() * kUiRate);
    if (tick == last_tick) return false;
    last_tick = tick;
    return true;
  }
};

UiStream g_ui_streams[2];
uint32_t g_input_manager = 0;

}  // namespace

// Input manager update: remember the object for the pad records.
REX_EXTERN(__imp__sub_8252EDC0);
REX_HOOK_RAW(sub_8252EDC0) {
  g_input_manager = ctx.r3.u32;
  __imp__sub_8252EDC0(ctx, base);
}

// APT manager message handler (r4 = message).
REX_EXTERN(__imp__sub_82629450);
REX_HOOK_RAW(sub_82629450) {
  int stream = -1;
  if (UiGated() && ctx.r4.u32 == kUpdateMessage) {
    uint32_t id = Load32(base, kUpdateMessage);
    for (int i = 0; i < 2; ++i) {
      if (id == Load32(base, kUiUpdateIds[i])) stream = i;
    }
  }
  if (stream < 0) {
    __imp__sub_82629450(ctx, base);
    return;
  }
  UiStream& s = g_ui_streams[stream];
  float frame_time = LoadFloat(base, kGameFrameTime);
  if (!s.Due()) {
    s.skipped_time += frame_time;
    return;
  }
  // Frame time and button presses since the last run.
  StoreFloat(base, kGameFrameTime, frame_time + s.skipped_time);
  s.skipped_time = 0.0f;
  uint16_t previous[kPadCount] = {};
  uint32_t pads = g_input_manager ? g_input_manager + kPadsOffset : 0;
  for (uint32_t i = 0; pads && i < kPadCount; ++i) {
    uint32_t pad = pads + i * kPadStride;
    previous[i] = Load16(base, pad + 2);
    Store16(base, pad + 2, s.buttons_at_last_run[i]);
  }
  __imp__sub_82629450(ctx, base);
  for (uint32_t i = 0; pads && i < kPadCount; ++i) {
    uint32_t pad = pads + i * kPadStride;
    s.buttons_at_last_run[i] = Load16(base, pad);
    Store16(base, pad + 2, previous[i]);
  }
  StoreFloat(base, kGameFrameTime, frame_time);
}

// Vblanks per frame. The present function sub_824DB9E0 waits until the guest
// vblank count (0x82CA892C) reaches the last present + N, N = the top of a
// small stack (entries 8 bytes apart from 0x82CA8948, depth at 0x829AFE24).
// N is 0 or 1 in play; scenes that push 2 run at half the refresh rate (30 fps
// at 60 Hz). dante_max_vblanks_per_frame caps N for the call; each change of
// the game's N is logged with the game frame time (PRESENT-N lines).
REXCVAR_DEFINE_INT32(dante_max_vblanks_per_frame, 0, "Graphics",
                     "Cap the vblanks the game waits per frame (1: scenes the game runs at half rate - "
                     "30 fps cutscenes - run at the full rate; 0: as the game asks)")
    .range(0, 4);

namespace {
constexpr uint32_t kVblankStack = 0x82CA8948;
constexpr uint32_t kVblankStackDepth = 0x829AFE24;
uint32_t g_last_vblanks = ~0u;
uint32_t g_present_count = 0;
float g_frame_time_sum = 0.0f;
std::chrono::steady_clock::time_point g_window_start;
}  // namespace

REX_EXTERN(__imp__sub_824DB9E0);
REX_HOOK_RAW(sub_824DB9E0) {
  uint32_t depth = Load32(base, kVblankStackDepth);
  uint32_t slot = (depth >= 1 && depth <= 16) ? kVblankStack + depth * 8 - 8 : 0;
  uint32_t vblanks = slot ? Load32(base, slot) : 0;
  using Clock = std::chrono::steady_clock;
  ++g_present_count;
  g_frame_time_sum += LoadFloat(base, kGameFrameTime);
  if (vblanks != g_last_vblanks || g_present_count == 600) {
    double real = std::chrono::duration<double>(Clock::now() - g_window_start).count();
    if (g_last_vblanks != ~0u && g_present_count) {
      REXLOG_INFO("PRESENT-N: game asks {} vblanks/frame (was {}, depth {}); last {} frames: real {:.2f} ms/frame, "
                  "game frame time {:.2f} ms",
                  vblanks, g_last_vblanks, depth, g_present_count, 1000.0 * real / g_present_count,
                  1000.0 * g_frame_time_sum / g_present_count);
    }
    g_last_vblanks = vblanks;
    g_present_count = 0;
    g_frame_time_sum = 0.0f;
    g_window_start = Clock::now();
  }
  int32_t cap = REXCVAR_GET(dante_max_vblanks_per_frame);
  if (cap > 0 && slot && vblanks > uint32_t(cap)) {
    Store32(base, slot, uint32_t(cap));
    __imp__sub_824DB9E0(ctx, base);
    if (Load32(base, slot) == uint32_t(cap)) Store32(base, slot, vblanks);
    return;
  }
  __imp__sub_824DB9E0(ctx, base);
}
