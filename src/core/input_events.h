#ifndef RHEOSPH_INPUT_EVENTS_H
#define RHEOSPH_INPUT_EVENTS_H

#include <cstdint>
#include <optional>
#include <vector>
namespace core {

enum Key : uint8_t { kF1, kO, kS };

struct CursorPosition {
  double x = 0.0;
  double y = 0.0;
} __attribute__((aligned(16)));

struct MouseDragEvent {
  CursorPosition anchor_position;
  std::optional<CursorPosition> current_position;
} __attribute__((aligned(64)));

struct ScrollEvent {
  CursorPosition cursor_position;
  double delta_x = 0.0;
  double delta_y = 0.0;
} __attribute__((aligned(32)));

struct InputModifiers {
  bool shift = false;
  bool control = false;
} __attribute__((aligned(2)));

struct InputState {
  InputModifiers modifiers;
  std::optional<MouseDragEvent> mouse_drag_event;
  std::optional<ScrollEvent> scroll_event;
  std::vector<Key> pressed_keys;
} __attribute__((aligned(128)));

}  // namespace core

#endif  // !RHEOSPH_INPUT_EVENTS_H
