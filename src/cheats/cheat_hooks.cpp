// Hooks for the cheat menu (DANTESINFERNO_DEV_CHEATS): guest pad masking while the menu is open, infinite
// health, and the player stats (magic, souls, Holy/Unholy power).
//
// Where the stat cheats hook: an Xbox 360 trainer for this game (RETROBYTE / XYZEngine, title 454108CF) patches
// the same two places in the TU2 image this port is built from - only the addresses and the values it sets were
// taken from it:
//  - 0x8223905C in sub_82239018, the damage component's SetHealth(component, f1 = health, r5): health at +44,
//    maximum at +48 (the trainer's "Demi God" pins the player's health there);
//  - 0x82306F30 in sub_82306E28, the stat setter SetStat(stat, f1 = value): stat id at +0, value at +4,
//    clamped to [+8, +12], flags +16, listeners at +20 (count +24) (the trainer's "Inf Mana", "Max Money",
//    "Max Level" and "Instant Inf Redemption" replace the value stored there for chosen stats).
// The stats are told apart by their id here (dante_stat_trace logs them), not by the trainer's heap
// addresses.

#include "cheats.h"

#include "../guest_memory.h"

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging/macros.h>
#include <rex/ppc/context.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>

REXCVAR_DEFINE_BOOL(dante_cheat_inf_health, false, "Cheats",
                    "The player's health refills to full on every change (hits still land)");
REXCVAR_DEFINE_BOOL(dante_cheat_inf_mana, false, "Cheats", "Magic stays full");
REXCVAR_DEFINE_BOOL(dante_cheat_max_souls, false, "Cheats", "Souls go to the maximum when they change");
REXCVAR_DEFINE_BOOL(dante_cheat_max_level, false, "Cheats", "Holy and Unholy power go to the top");
REXCVAR_DEFINE_BOOL(dante_stat_trace, false, "Diagnostics",
                    "Log player stat changes (SetStat sub_82306E28): id, address, caller, old/new value, range");

bool IsPlayerTeam(const uint8_t* base, uint32_t entity);  // damage_hooks.cpp

namespace {

using namespace guest;

// Damage component (sub_82239018).
constexpr uint32_t kComponentOwner = 72, kHealth = 44, kHealthMax = 48;
// Stat (sub_82306E28).
constexpr uint32_t kStatId = 0, kStatValue = 4, kStatMin = 8, kStatMax = 12, kStatFlags = 16;

// X_INPUT_STATE in guest memory: packet number, then the gamepad.
constexpr uint32_t kPadButtons = 4, kPadSize = 12;

std::mutex g_trace_mutex;
std::unordered_map<uint32_t, uint32_t> g_trace_counts;  // stat address -> lines logged

}  // namespace

// XInputGetState thunk (`b XamInputGetState`, the game's only pad read): r3 user, r4 flags, r5 state.
// While the cheat menu is open (and until every button is let go after it closes) the game sees an idle pad.
REX_EXTERN(__imp__sub_8281E7B0);
REX_HOOK_RAW(sub_8281E7B0) {
  const uint32_t state = ctx.r5.u32;
  __imp__sub_8281E7B0(ctx, base);
  if (ctx.r3.u32 != 0 || !IsPlausibleAddress(state)) return;
  if (cheats::OnGuestPad(Load16(base, state + kPadButtons))) {
    std::memset(Ptr(base, state + kPadButtons), 0, kPadSize);
  }
}

// SetHealth(component, f1 = health): with infinite health the player's health is set to its maximum.
REX_EXTERN(__imp__sub_82239018);
REX_HOOK_RAW(sub_82239018) {
  if (REXCVAR_GET(dante_cheat_inf_health) && IsPlausibleAddress(ctx.r3.u32) &&
      IsPlayerTeam(base, Load32(base, ctx.r3.u32 + kComponentOwner))) {
    const float max = LoadFloat(base, ctx.r3.u32 + kHealthMax);
    if (max > 0.0f) ctx.f1.f64 = max;
  }
  __imp__sub_82239018(ctx, base);
}

// SetStat(stat, f1 = value).
REX_EXTERN(__imp__sub_82306E28);
REX_HOOK_RAW(sub_82306E28) {
  const uint32_t stat = ctx.r3.u32;
  if (!IsPlausibleAddress(stat)) {
    __imp__sub_82306E28(ctx, base);
    return;
  }
  const uint32_t id = Load32(base, stat + kStatId);
  const float old_value = LoadFloat(base, stat + kStatValue);
  const float requested = float(ctx.f1.f64);
  if (REXCVAR_GET(dante_stat_trace) && requested != old_value && uint32_t(ctx.lr) != 0x82307B70) {
    std::lock_guard lock(g_trace_mutex);
    uint32_t& lines = g_trace_counts[stat];
    if (lines < 40) {
      ++lines;
      REXLOG_INFO("STAT: id {:08X} at {:08X} caller {:08X}: {} -> {} (range {} .. {}, flags {:08X})", id,
                  stat, uint32_t(ctx.lr), old_value, requested, LoadFloat(base, stat + kStatMin),
                  LoadFloat(base, stat + kStatMax), Load32(base, stat + kStatFlags));
    }
  }
  __imp__sub_82306E28(ctx, base);
}

// FindStat(set, r4 = stat id, r5 = group): with dante_stat_trace, logs every stat id once with its value, so
// the ids of souls, magic, Holy/Unholy can be read off a running game.
REX_EXTERN(__imp__sub_82309100);
REX_HOOK_RAW(sub_82309100) {
  const uint32_t id = ctx.r4.u32;
  const uint32_t lr = uint32_t(ctx.lr);
  __imp__sub_82309100(ctx, base);
  if (!REXCVAR_GET(dante_stat_trace) || !IsPlausibleAddress(ctx.r3.u32)) return;
  static std::mutex mutex;
  static std::unordered_map<uint32_t, uint32_t> seen;
  std::lock_guard lock(mutex);
  if (seen.size() > 4000 || !seen.emplace(id, ctx.r3.u32).second) return;
  const uint32_t stat = ctx.r3.u32;
  REXLOG_INFO("STAT-FIND: id {:08X} at {:08X} caller {:08X} value {} (range {} .. {}, flags {:08X})", id, stat,
              lr, LoadFloat(base, stat + kStatValue), LoadFloat(base, stat + kStatMin),
              LoadFloat(base, stat + kStatMax), Load32(base, stat + kStatFlags));
}
