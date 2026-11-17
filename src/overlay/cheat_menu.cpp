#include "cheat_menu.h"

#include "overlay_style.h"

#include "../cheats/cheats.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace overlay {
namespace {

constexpr uint16_t kDpadUp = 0x0001, kDpadDown = 0x0002, kStart = 0x0010, kButtonA = 0x1000,
                   kButtonB = 0x2000;
constexpr int16_t kStickThreshold = 16000;
constexpr auto kRepeatDelay = std::chrono::milliseconds(380), kRepeatRate = std::chrono::milliseconds(140);

int RowCount() { return int(cheats::List().size()) + 1; }  // + Close

}  // namespace

CheatMenu::CheatMenu(rex::ui::ImGuiDrawer* drawer, PadReader pad, std::function<void()> on_close)
    : rex::ui::ImGuiDialog(drawer), pad_(std::move(pad)), on_close_(std::move(on_close)) {
  cheats::SetMenuOpen(true);
}

CheatMenu::~CheatMenu() { cheats::SetMenuOpen(false); }

void CheatMenu::Activate(int row) {
  if (closing_) return;
  auto list = cheats::List();
  if (row >= 0 && row < int(list.size())) {
    cheats::Set(list[row].cvar, !cheats::Enabled(list[row].cvar));
    return;
  }
  closing_ = true;
  if (on_close_) on_close_();
}

void CheatMenu::HandlePad() {
  uint16_t buttons = 0;
  int16_t stick_y = 0;
  if (!pad_ || !pad_(buttons, stick_y)) return;
  if (last_buttons_ == 0xFFFF) last_buttons_ = buttons;  // first frame: held buttons are not presses
  const uint16_t pressed = buttons & ~last_buttons_;
  last_buttons_ = buttons;

  // Up/down from the D-pad or the stick, with auto-repeat while held.
  int dir = (buttons & kDpadUp) || stick_y > kStickThreshold     ? -1
            : (buttons & kDpadDown) || stick_y < -kStickThreshold ? 1
                                                                 : 0;
  const auto now = std::chrono::steady_clock::now();
  if (dir != 0 && dir != last_stick_dir_) {
    selected_ = (selected_ + dir + RowCount()) % RowCount();
    repeat_at_ = now + kRepeatDelay;
  } else if (dir != 0 && now >= repeat_at_) {
    selected_ = std::clamp(selected_ + dir, 0, RowCount() - 1);
    repeat_at_ = now + kRepeatRate;
  }
  last_stick_dir_ = dir;

  if (pressed & kButtonA) Activate(selected_);
  if (pressed & (kButtonB | kStart)) Activate(RowCount() - 1);
}

void CheatMenu::OnDraw(ImGuiIO& io) {
  if (closing_) return;
  HandlePad();
  if (closing_) return;

  const float s = UiScale(io);
  auto list = cheats::List();
  const int rows = RowCount();

  // Panel geometry: centred, rows sized for a thumb on a phone.
  const float width = std::min(820.0f * s, io.DisplaySize.x - 48.0f * s);
  const float pad = 28.0f * s, row_h = 74.0f * s, gap = 6.0f * s;
  const float header_h = 96.0f * s, footer_h = 52.0f * s;
  const float height = std::min(header_h + rows * (row_h + gap) + footer_h + pad,
                                io.DisplaySize.y - 24.0f * s);
  const float row_step = (height - header_h - footer_h - pad) / rows;
  const float row_draw_h = row_step - gap;
  const ImVec2 min(std::round((io.DisplaySize.x - width) * 0.5f), std::round((io.DisplaySize.y - height) * 0.5f));
  const ImVec2 max(min.x + width, min.y + height);
  auto row_rect = [&](int i, ImVec2& a, ImVec2& b) {
    a = ImVec2(min.x + pad * 0.5f, min.y + header_h + i * row_step);
    b = ImVec2(max.x - pad * 0.5f, a.y + row_draw_h);
  };
  auto hit_row = [&](float x, float y) {
    for (int i = 0; i < rows; ++i) {
      ImVec2 a, b;
      row_rect(i, a, b);
      if (x >= a.x && x < b.x && y >= a.y && y < b.y) return i;
    }
    return (x < min.x || x >= max.x || y < min.y || y >= max.y) ? rows - 1 : -1;  // outside = Close
  };

  // Taps (touch capture view) and mouse clicks.
  float tx, ty;
  while (!closing_ && cheats::PopTap(tx, ty)) {
    int row = hit_row(tx * io.DisplaySize.x, ty * io.DisplaySize.y);
    if (row >= 0) {
      selected_ = row;
      Activate(row);
    }
  }
  if (!closing_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    int row = hit_row(io.MousePos.x, io.MousePos.y);
    if (row >= 0) {
      selected_ = row;
      Activate(row);
    }
  }
  if (closing_) return;

  ImDrawList* list_draw = ImGui::GetForegroundDrawList();
  list_draw->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, 120));
  const float rounding = 18.0f * s;
  DrawShadow(list_draw, min, max, rounding, 16.0f * s, 0.7f);
  list_draw->AddRectFilled(min, max, color::kPanel, rounding);
  list_draw->AddRect(min, max, color::kPanelBorder, rounding, 0, std::max(1.0f, s));

  DrawText(list_draw, Font::kHeading, s, ImVec2(min.x + pad, min.y + 18.0f * s), color::kGold, "Cheats");
  const char* note = "Live - remembered next time";
  ImVec2 note_size = MeasureText(Font::kCaption, s, note);
  DrawText(list_draw, Font::kCaption, s, ImVec2(max.x - pad - note_size.x, min.y + 38.0f * s),
           color::kTextMuted, note);

  for (int i = 0; i < rows; ++i) {
    ImVec2 a, b;
    row_rect(i, a, b);
    const bool sel = i == selected_;
    if (sel) {
      list_draw->AddRectFilled(a, b, IM_COL32(255, 255, 255, 22), 12.0f * s);
      list_draw->AddRect(a, b, WithAlpha(color::kGold, 0.85f), 12.0f * s, 0, 2.0f * s);
    }
    const float tx0 = a.x + 20.0f * s;
    if (i == rows - 1) {
      ImVec2 sz = MeasureText(Font::kBodyStrong, s, "Close");
      DrawText(list_draw, Font::kBodyStrong, s, ImVec2((a.x + b.x - sz.x) * 0.5f, (a.y + b.y - sz.y) * 0.5f),
               color::kText, "Close");
      continue;
    }
    const bool on = cheats::Enabled(list[i].cvar);
    const float label_y = a.y + (row_draw_h - FontSize(Font::kBodyStrong, s) - FontSize(Font::kCaption, s)) * 0.5f - 2.0f * s;
    DrawText(list_draw, Font::kBodyStrong, s, ImVec2(tx0, label_y), color::kText, list[i].label);
    DrawText(list_draw, Font::kCaption, s, ImVec2(tx0, label_y + FontSize(Font::kBodyStrong, s) + 2.0f * s),
             color::kTextMuted, Ellipsize(Font::kCaption, s, list[i].detail, (b.x - a.x) - 200.0f * s));
    // ON/OFF pill.
    const char* state = on ? "ON" : "OFF";
    const float pill_w = 84.0f * s, pill_h = 38.0f * s;
    ImVec2 pa(b.x - 20.0f * s - pill_w, (a.y + b.y - pill_h) * 0.5f), pb(pa.x + pill_w, pa.y + pill_h);
    list_draw->AddRectFilled(pa, pb, on ? WithAlpha(color::kGood, 0.9f) : IM_COL32(255, 255, 255, 30), pill_h * 0.5f);
    ImVec2 sz = MeasureText(Font::kBodyStrong, s, state);
    DrawText(list_draw, Font::kBodyStrong, s, ImVec2((pa.x + pb.x - sz.x) * 0.5f, (pa.y + pb.y - sz.y) * 0.5f),
             on ? IM_COL32(10, 30, 16, 255) : color::kTextMuted, state);
  }

  const char* hint = "A: toggle   B: close   Tap a row   -   Open: L3+R3 or 3-finger tap";
  std::string fitted = Ellipsize(Font::kCaption, s, hint, width - pad * 2);
  ImVec2 hint_size = MeasureText(Font::kCaption, s, fitted);
  DrawText(list_draw, Font::kCaption, s,
           ImVec2((min.x + max.x - hint_size.x) * 0.5f, max.y - footer_h * 0.5f - hint_size.y * 0.5f - 6.0f * s),
           color::kTextFaint, fitted);
}

}  // namespace overlay
