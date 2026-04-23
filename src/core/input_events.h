#ifndef RHEOSPH_INPUT_EVENTS_H
#define RHEOSPH_INPUT_EVENTS_H

#include <optional>
#include <variant>
#include <vector>
namespace core {

struct MouseMovedEvent {
  double dx, dy;
} __attribute__((aligned(16)));

struct MouseScrollEvent {
  double dy;
  double x;
  double y;
} __attribute__((aligned(32)));

struct InputEvent {
  bool is_holding_shift = false;
  bool is_holding_control = false;
  bool is_holding_mouse_left_click = false;
  std::optional<double> prev_mouse_xpos;
  std::optional<double> prev_mouse_ypos;
  std::vector<std::variant<MouseMovedEvent, MouseScrollEvent>> events;
} __attribute__((aligned(64)));

}  // namespace core

#endif  // !RHEOSPH_INPUT_EVENTS_H
