// In-game cheat menu support (DANTESINFERNO_DEV_CHEATS builds): the cheat list, the choices kept in
// <user data>/cheats.toml, the menu's open request (L3+R3 on a pad, a three-finger tap on iPhone/iPad, a key)
// and the guest input mask while the menu is open.

#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>

namespace cheats {

struct Cheat {
  const char* cvar;    // bool cvar, toggled live
  const char* label;   // menu row
  const char* detail;  // one line under the label
};

std::span<const Cheat> List();

bool Enabled(const char* cvar);
// Sets the cvar now and writes cheats.toml.
void Set(const char* cvar, bool enabled);

// Reads <user_data_root>/cheats.toml (choices made in the menu) over the config's values.
void Load(const std::filesystem::path& user_data_root);

// Menu state, shared with the guest input hook.
bool MenuOpen();
void SetMenuOpen(bool open);
// Called (any thread) when the player asks for the menu; the app shows it on the UI thread.
void SetOpenRequestHandler(std::function<void()> handler);
void RequestOpen();

// Guest input hook (src/cheats/cheat_hooks.cpp): the pad buttons the game just read. Detects the open combo
// and says whether the game's pad state must be blanked (menu open, or buttons still held after it closed).
bool OnGuestPad(uint16_t buttons);

// iPhone/iPad (cheat_touch_ios.mm): three-finger tap opens the menu; while open, taps go to the menu.
// No-ops elsewhere.
void InstallTouchGesture();
void SetTouchCapture(bool capture);
// Taps for the menu, in 0..1 window coordinates (touch capture view); false when none is queued.
void PushTap(float x, float y);
bool PopTap(float& x, float& y);

}  // namespace cheats
