// Scripted controller for performance measurements (blz/ios-native, 17 Nov 2026): the iPhone could not be
// tapped remotely (no USB, no WebDriverAgent), so the game is driven to the scenes that are measured by a pad
// script. Off unless `dante_autopilot` is set. This is SCRIPTED input: it never counts as a play test.
//
// dante_autopilot = "S[-E]:BUTTON[/P]; ..."   (S, E in seconds after start-up, P in ms)
//   "20:start"          tap Start at 20 s (held 150 ms)
//   "40-70:a/700"       tap A every 700 ms from 40 s to 70 s
//   "70-90:lup"         hold the left stick up from 70 s to 90 s
// "file": remote control - Documents/autopilot_cmd.txt (devicectl copy) is re-read whenever it changes and
// its script runs with times counted from that moment.
// Buttons: a b x y start back lb rb lt rt up down left right ls rs; sticks: lup ldown lleft lright rup rdown
// rleft rright.

#include "autopilot_input.h"

#include <rex/cvar.h>
#include <rex/input/input_driver.h>
#include <rex/input/input_system.h>
#include <rex/logging/macros.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

REXCVAR_DEFINE_STRING(dante_autopilot, "", "Diagnostics",
                      "Scripted pad for performance runs: 'S[-E]:BUTTON[/P];...' (seconds, ms). Empty = off");

namespace autopilot {
namespace {

using namespace rex;
using namespace rex::input;
constexpr DeviceId kAutopilotDevice = static_cast<DeviceId>(0x4155544F);  // 'AUTO'
constexpr double kTapSeconds = 0.15;

struct Step {
  double start = 0, end = 0;  // seconds
  double period = 0;          // seconds; 0 = one tap at start (end == start) or held for the range
  uint16_t buttons = 0;
  uint8_t lt = 0, rt = 0;
  int16_t lx = 0, ly = 0, rx = 0, ry = 0;
};

bool ParseControl(const std::string& name, Step& s) {
  static const struct {
    const char* name;
    uint16_t button;
  } kButtons[] = {{"a", X_INPUT_GAMEPAD_A},          {"b", X_INPUT_GAMEPAD_B},
                  {"x", X_INPUT_GAMEPAD_X},          {"y", X_INPUT_GAMEPAD_Y},
                  {"start", X_INPUT_GAMEPAD_START},  {"back", X_INPUT_GAMEPAD_BACK},
                  {"lb", X_INPUT_GAMEPAD_LEFT_SHOULDER}, {"rb", X_INPUT_GAMEPAD_RIGHT_SHOULDER},
                  {"up", X_INPUT_GAMEPAD_DPAD_UP},   {"down", X_INPUT_GAMEPAD_DPAD_DOWN},
                  {"left", X_INPUT_GAMEPAD_DPAD_LEFT}, {"right", X_INPUT_GAMEPAD_DPAD_RIGHT},
                  {"ls", X_INPUT_GAMEPAD_LEFT_THUMB}, {"rs", X_INPUT_GAMEPAD_RIGHT_THUMB}};
  for (const auto& b : kButtons) {
    if (name == b.name) {
      s.buttons = b.button;
      return true;
    }
  }
  constexpr int16_t kFull = 32767;
  if (name == "lt") s.lt = 255;
  else if (name == "rt") s.rt = 255;
  else if (name == "lup") s.ly = kFull;
  else if (name == "ldown") s.ly = -kFull;
  else if (name == "lleft") s.lx = -kFull;
  else if (name == "lright") s.lx = kFull;
  else if (name == "rup") s.ry = kFull;
  else if (name == "rdown") s.ry = -kFull;
  else if (name == "rleft") s.rx = -kFull;
  else if (name == "rright") s.rx = kFull;
  else return false;
  return true;
}

std::vector<Step> Parse(const std::string& script) {
  std::vector<Step> steps;
  std::stringstream in(script);
  std::string item;
  while (std::getline(in, item, ';')) {
    item.erase(0, item.find_first_not_of(" \t"));
    if (item.empty()) continue;
    size_t colon = item.find(':');
    if (colon == std::string::npos) continue;
    Step s;
    std::string when = item.substr(0, colon), what = item.substr(colon + 1);
    size_t dash = when.find('-');
    s.start = std::atof(when.c_str());
    s.end = dash == std::string::npos ? s.start : std::atof(when.c_str() + dash + 1);
    size_t slash = what.find('/');
    if (slash != std::string::npos) {
      s.period = std::atof(what.c_str() + slash + 1) / 1000.0;
      what.resize(slash);
    }
    while (!what.empty() && what.back() == ' ') what.pop_back();
    if (!ParseControl(what, s)) {
      REXLOG_WARN("AUTOPILOT: unknown control '{}' in '{}'", what, item);
      continue;
    }
    steps.push_back(s);
  }
  return steps;
}

class AutopilotDriver final : public InputDriver {
 public:
  AutopilotDriver(std::vector<Step> steps, std::string command_file)
      : InputDriver(nullptr, 0),
        steps_(std::move(steps)),
        command_file_(std::move(command_file)),
        start_(std::chrono::steady_clock::now()) {}

  X_STATUS Setup() override { return X_STATUS_SUCCESS; }

  void EnumerateDevices(std::vector<DeviceInfo>& out) override {
    DeviceInfo info;
    info.id = kAutopilotDevice;
    info.name = "Autopilot (scripted)";
    info.synthetic = true;
    out.push_back(info);
  }

  X_RESULT GetDeviceState(DeviceId id, X_INPUT_STATE* out_state) override {
    if (id != kAutopilotDevice) return X_ERROR_DEVICE_NOT_CONNECTED;
    if (!out_state) return X_ERROR_SUCCESS;
    const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
    if (!command_file_.empty() && t - last_poll_ >= 0.25) {
      last_poll_ = t;
      struct stat st {};
      if (stat(command_file_.c_str(), &st) == 0 && st.st_mtime + st.st_size * 1e-9 != command_stamp_) {
        command_stamp_ = st.st_mtime + st.st_size * 1e-9;
        std::ifstream in(command_file_);
        std::string script((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::vector<Step> steps = Parse(script);
        for (Step& step : steps) {
          step.start += t;
          step.end += t;
        }
        REXLOG_WARN("AUTOPILOT: command file ({} steps): {}", steps.size(), script);
        std::lock_guard lock(mutex_);
        steps_ = std::move(steps);
      }
    }
    X_INPUT_STATE state{};
    std::lock_guard steps_lock(mutex_);
    for (const Step& s : steps_) {
      bool active;
      if (s.end <= s.start) {
        active = t >= s.start && t < s.start + kTapSeconds;
      } else if (s.period > 0) {
        active = t >= s.start && t < s.end && std::fmod(t - s.start, s.period) < kTapSeconds;
      } else {
        active = t >= s.start && t < s.end;
      }
      if (!active) continue;
      state.gamepad.buttons = uint16_t(state.gamepad.buttons) | s.buttons;
      if (s.lt) state.gamepad.left_trigger = s.lt;
      if (s.rt) state.gamepad.right_trigger = s.rt;
      if (s.lx) state.gamepad.thumb_lx = s.lx;
      if (s.ly) state.gamepad.thumb_ly = s.ly;
      if (s.rx) state.gamepad.thumb_rx = s.rx;
      if (s.ry) state.gamepad.thumb_ry = s.ry;
    }
    if (std::memcmp(&state.gamepad, &last_.gamepad, sizeof(state.gamepad)) != 0) {
      ++packet_;
      last_ = state;
    }
    state.packet_number = packet_;
    *out_state = state;
    return X_ERROR_SUCCESS;
  }

  X_RESULT GetDeviceCapabilities(DeviceId id, uint32_t, X_INPUT_CAPABILITIES* out_caps) override {
    if (id != kAutopilotDevice) return X_ERROR_DEVICE_NOT_CONNECTED;
    if (out_caps) {
      std::memset(out_caps, 0, sizeof(*out_caps));
      out_caps->type = XINPUT_DEVTYPE_GAMEPAD;
      out_caps->sub_type = 0x01;
      out_caps->gamepad.buttons = 0xF3FF;
      out_caps->gamepad.left_trigger = out_caps->gamepad.right_trigger = 0xFF;
      out_caps->gamepad.thumb_lx = out_caps->gamepad.thumb_ly = int16_t(0xFFC0);
      out_caps->gamepad.thumb_rx = out_caps->gamepad.thumb_ry = int16_t(0xFFC0);
    }
    return X_ERROR_SUCCESS;
  }

  X_RESULT SetDeviceVibration(DeviceId id, X_INPUT_VIBRATION*) override {
    return id == kAutopilotDevice ? X_ERROR_SUCCESS : X_ERROR_DEVICE_NOT_CONNECTED;
  }

  X_RESULT GetDeviceKeystroke(DeviceId id, uint32_t, X_INPUT_KEYSTROKE*) override {
    return id == kAutopilotDevice ? X_ERROR_EMPTY : X_ERROR_DEVICE_NOT_CONNECTED;
  }

 private:
  std::vector<Step> steps_;
  std::string command_file_;
  double last_poll_ = -1.0;
  double command_stamp_ = 0.0;
  std::chrono::steady_clock::time_point start_;
  std::mutex mutex_;
  X_INPUT_STATE last_{};
  uint32_t packet_ = 0;
};

}  // namespace

void Install(rex::input::InputSystem* input) {
  std::string script = REXCVAR_GET(dante_autopilot);
  if (!input || script.empty()) return;
  std::string command_file;
  std::vector<Step> steps;
  if (script == "file") {
    const char* home = std::getenv("HOME");
    command_file = std::string(home ? home : ".") + "/Documents/autopilot_cmd.txt";
    std::remove(command_file.c_str());  // only commands sent during this run count
  } else {
    steps = Parse(script);
  }
  REXLOG_WARN("AUTOPILOT: scripted pad ON ({} steps{}): {}", steps.size(),
              command_file.empty() ? "" : ", command file", script);
  input->AddDriver(std::make_unique<AutopilotDriver>(std::move(steps), std::move(command_file)));
}

}  // namespace autopilot
