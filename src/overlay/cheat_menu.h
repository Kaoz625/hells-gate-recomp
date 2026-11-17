// Cheat menu: a panel listing the cheats (src/cheats/cheats.h) with their state. Pad: D-pad / left stick
// to move, A to toggle, B or Start to close. Touch (iPhone/iPad): tap a row to toggle it, tap outside the
// panel or on Close to close. Mouse: click. The game keeps running; it sees an idle pad while this is open.

#pragma once

#include <rex/ui/imgui_dialog.h>

#include <chrono>
#include <cstdint>
#include <functional>

namespace overlay {

class CheatMenu : public rex::ui::ImGuiDialog {
 public:
  // `pad`: player 1's buttons (XINPUT bits) and left stick Y, or false when no pad answers.
  using PadReader = std::function<bool(uint16_t& buttons, int16_t& stick_y)>;
  // `on_close` runs after the menu closed itself (B, Start, Close).
  CheatMenu(rex::ui::ImGuiDrawer* drawer, PadReader pad, std::function<void()> on_close);
  ~CheatMenu() override;

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  void HandlePad();
  void Activate(int row);  // row == count: Close

  PadReader pad_;
  std::function<void()> on_close_;
  int selected_ = 0;
  uint16_t last_buttons_ = 0xFFFF;  // ignore what is held when the menu opens
  int last_stick_dir_ = 0;
  std::chrono::steady_clock::time_point repeat_at_{};
  bool closing_ = false;
};

}  // namespace overlay
