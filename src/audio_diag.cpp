// Audio diagnostics (blz/ios-audio, Dante iPhone 1100 audio pattern). Built only with
// DANTESINFERNO_AUDIO_DIAG=ON; never in a normal build.
//
// Render device object (vtable at 0x82038B74, XAudio client): +40 request event, +44 frame-ready event,
// +48 quit event, +24 frame buffer (256 frames x 6 ch float), +56 frames submitted.
// sub_8284AA10 = the XAudio render callback (sets +40, waits +44, submits +24).
// sub_8284A9B0(this+4, ?, r5) = "frame done" from the mixer; r5 == 1 -> the buffer is cleared to silence.

#include <rex/hook.h>
#include <rex/logging/macros.h>
#include <rex/ppc/context.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <cstring>
#include <fmt/format.h>

namespace {
std::mutex g_mutex;
std::string g_done_pattern;
std::map<uint64_t, uint64_t> g_done_callers;  // (lr << 8 | r5) -> count
uint64_t g_done_calls = 0;

double NowMs() {
  using C = std::chrono::steady_clock;
  static const C::time_point start = C::now();
  return std::chrono::duration<double, std::milli>(C::now() - start).count();
}
}  // namespace

REX_EXTERN(__imp__sub_8284A9B0);
REX_HOOK_RAW(sub_8284A9B0) {
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_done_calls;
    ++g_done_callers[(uint64_t(ctx.lr) << 8) | (ctx.r5.u32 & 0xFF)];
    if (g_done_calls > 3000 && g_done_pattern.size() < 96) g_done_pattern += ctx.r5.u32 == 1 ? '0' : '1';
    if (g_done_pattern.size() == 96) {
      REXLOG_INFO("AUDIO-DIAG frame-done pattern (0 = r5==1 silence fill): {}", g_done_pattern);
      for (auto& [k, n] : g_done_callers)
        REXLOG_INFO("AUDIO-DIAG frame-done caller lr {:08X} r5 {} x{}", uint32_t(k >> 8), uint32_t(k & 0xFF), n);
      g_done_pattern += '.';
    }
  }
  {
    static std::atomic<int> tl{0};
    static const auto t0 = std::chrono::steady_clock::now();
    if (std::chrono::steady_clock::now() - t0 > std::chrono::seconds(40) && tl.fetch_add(1) < 120)
      REXLOG_INFO("AUDIO-TL frame-done {}", ctx.r5.u32 == 1 ? "SILENT" : "sound");
  }
  __imp__sub_8284A9B0(ctx, base);
}

namespace {
uint32_t Load32(uint8_t* base, uint32_t a) {
  uint32_t v;
  std::memcpy(&v, base + a, 4);
  return __builtin_bswap32(v);
}
std::string g_render_pattern;
std::map<uint64_t, uint64_t> g_render_targets;  // (stream vcall target << 32 | +140 after) -> count
uint64_t g_render_calls = 0;
}  // namespace

// Per-frame render of the mixer voice (this = r3): clears +140/+144, asks the stream object [+8] (vtable slot +40)
// for a block into [+120]+56 with results at +136/+140, then reports frame-done (silence when +140 == 0).
REX_EXTERN(__imp__sub_82865140);
REX_HOOK_RAW(sub_82865140) {
  const uint32_t self = ctx.r3.u32;
  const uint32_t stream = Load32(base, self + 8);
  const uint32_t target = Load32(base, Load32(base, stream) + 40);
  __imp__sub_82865140(ctx, base);
  const uint32_t produced = Load32(base, self + 136), flag = Load32(base, self + 140);
  std::lock_guard<std::mutex> lock(g_mutex);
  ++g_render_calls;
  ++g_render_targets[(uint64_t(target) << 32) | (flag ? 1 : 0)];
  if (g_render_calls > 3000 && g_render_pattern.size() < 64)
    g_render_pattern += fmt::format("{}:{} ", flag ? 1 : 0, int32_t(produced));
  if (g_render_pattern.size() >= 64 && g_render_pattern.back() != '.') {
    const uint32_t in_params = Load32(base, self + 120) + 56;
    REXLOG_INFO("AUDIO-DIAG render (+140:+136 after): {} stream {:08X} self {:08X} input params {:08X} host {}",
                g_render_pattern, stream, self, in_params, static_cast<void*>(base + in_params));
    for (auto& [k, n] : g_render_targets)
      REXLOG_INFO("AUDIO-DIAG render target {:08X} flag {} x{}", uint32_t(k >> 32), uint32_t(k & 1), n);
    g_render_pattern += '.';
  }
}

// Engine processing pass (one per device frame) and frame-done, as a timeline next to XMA-TRACE.
namespace {
bool TimelineOn(std::atomic<int>& n) {
  static const auto start = std::chrono::steady_clock::now();
  return std::chrono::steady_clock::now() - start > std::chrono::seconds(40) && n.fetch_add(1) < 120;
}
std::atomic<int> g_tl_pass{0};
}  // namespace
REX_EXTERN(__imp__sub_82848608);
REX_HOOK_RAW(sub_82848608) {
  if (TimelineOn(g_tl_pass)) REXLOG_INFO("AUDIO-TL pass begin");
  __imp__sub_82848608(ctx, base);
}
