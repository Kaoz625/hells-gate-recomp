#pragma once

namespace rex::input {
class InputSystem;
}

namespace autopilot {
// Adds the scripted pad (dante_autopilot) to `input` when the setting is non-empty.
void Install(rex::input::InputSystem* input);
}  // namespace autopilot
