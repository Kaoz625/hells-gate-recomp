#include "cheats.h"

#include <rex/cvar.h>
#include <rex/logging/macros.h>

#include <toml++/toml.hpp>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <fstream>
#include <mutex>
#include <string>

namespace cheats {
namespace {

// The cvars are defined next to their hooks (damage_hooks.cpp, cheat_hooks.cpp).
constexpr Cheat kCheats[] = {
    {"dante_cheat_god", "God mode", "No damage, no hit reactions or knockback"},
    {"dante_cheat_inf_health", "Infinite health", "Health refills to full on every hit"},
    {"dante_cheat_one_hit", "One-hit kills", "Any hit kills an enemy"},
    {"dante_cheat_inf_mana", "Infinite magic", "Magic stays full"},
    {"dante_cheat_max_souls", "Max souls", "Souls set to the maximum when they change"},
    {"dante_cheat_max_level", "Max Holy + Unholy", "Holy and Unholy power to the top level"},
};

std::filesystem::path g_file;
std::mutex g_mutex;
std::function<void()> g_open_handler;
std::atomic<bool> g_menu_open{false};
std::deque<std::array<float, 2>> g_taps;

// Pad combo: L3 + R3 held together (the game uses neither click for anything that both press at once).
constexpr uint16_t kLeftThumb = 0x0040, kRightThumb = 0x0080;
constexpr auto kComboHold = std::chrono::milliseconds(250);
std::chrono::steady_clock::time_point g_combo_since{};
bool g_combo_fired = false;
bool g_masking = false;  // menu open, or buttons still held since it closed

void Save() {
  if (g_file.empty()) return;
  std::ofstream out(g_file, std::ios::trunc);
  if (!out) {
    REXLOG_WARN("CHEATS: cannot write {}", g_file.string());
    return;
  }
  out << "# Cheat menu choices (L3+R3 or three-finger tap in game). Delete this file to reset.\n";
  for (const Cheat& c : kCheats) out << c.cvar << " = " << (Enabled(c.cvar) ? "true" : "false") << "\n";
}

}  // namespace

std::span<const Cheat> List() { return kCheats; }

bool Enabled(const char* cvar) { return rex::cvar::Query<bool>(cvar); }

void Set(const char* cvar, bool enabled) {
  rex::cvar::SetFlagByName(cvar, enabled ? "true" : "false");
  REXLOG_INFO("CHEAT {} {}", cvar, enabled ? "on" : "off");
  std::lock_guard lock(g_mutex);
  Save();
}

void Load(const std::filesystem::path& user_data_root) {
  std::lock_guard lock(g_mutex);
  g_file = user_data_root / "cheats.toml";
  std::error_code ec;
  if (!std::filesystem::is_regular_file(g_file, ec)) return;
  try {
    auto table = toml::parse_file(g_file.string());
    for (const Cheat& c : kCheats) {
      if (auto v = table[c.cvar].value<bool>()) rex::cvar::SetFlagByName(c.cvar, *v ? "true" : "false");
    }
    REXLOG_INFO("CHEATS: choices loaded from {}", g_file.string());
  } catch (const toml::parse_error& e) {
    REXLOG_WARN("CHEATS: {} unreadable ({}), ignored", g_file.string(), e.description());
  }
}

bool MenuOpen() { return g_menu_open.load(std::memory_order_relaxed); }

void SetMenuOpen(bool open) {
  g_menu_open = open;
  SetTouchCapture(open);
  if (!open) {
    std::lock_guard lock(g_mutex);
    g_taps.clear();
  }
}

void SetOpenRequestHandler(std::function<void()> handler) {
  std::lock_guard lock(g_mutex);
  g_open_handler = std::move(handler);
}

void RequestOpen() {
  std::function<void()> handler;
  {
    std::lock_guard lock(g_mutex);
    handler = g_open_handler;
  }
  if (handler) handler();
}

bool OnGuestPad(uint16_t buttons) {
  const bool open = MenuOpen();
  if (!open) {
    const bool combo = (buttons & (kLeftThumb | kRightThumb)) == (kLeftThumb | kRightThumb);
    const auto now = std::chrono::steady_clock::now();
    if (!combo) {
      g_combo_since = {};
      g_combo_fired = false;
    } else if (g_combo_since == std::chrono::steady_clock::time_point{}) {
      g_combo_since = now;
    } else if (!g_combo_fired && now - g_combo_since >= kComboHold) {
      g_combo_fired = true;
      RequestOpen();
      g_masking = true;
    }
  }
  if (open) g_masking = true;
  else if (g_masking && buttons == 0) g_masking = false;
  return g_masking;
}

void PushTap(float x, float y) {
  std::lock_guard lock(g_mutex);
  if (g_taps.size() < 8) g_taps.push_back({x, y});
}

bool PopTap(float& x, float& y) {
  std::lock_guard lock(g_mutex);
  if (g_taps.empty()) return false;
  x = g_taps.front()[0];
  y = g_taps.front()[1];
  g_taps.pop_front();
  return true;
}

#if !(defined(__APPLE__) && TARGET_OS_IPHONE)
void InstallTouchGesture() {}
void SetTouchCapture(bool) {}
#endif

}  // namespace cheats
