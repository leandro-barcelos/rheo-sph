#ifndef RHEOSPH_MOUSE_EVENT_H
#define RHEOSPH_MOUSE_EVENT_H

#include <format>
#include <glm/vec2.hpp>

#include "event.h"
#include "rheo-sph/src/core/mouse_codes.h"

namespace events {

class MouseMovedEvent : public Event {
 public:
  EVENT_TYPE("MouseMovedEvent")

  explicit MouseMovedEvent(float const x_position, float const y_position)
      : mouse_x_(x_position), mouse_y_(y_position) {}

  [[nodiscard]] std::string ToString() const override {
    return std::format("MouseMovedEvent: {}, {}", mouse_x_, mouse_y_);
  }

  [[nodiscard]] float GetMouseX() const { return mouse_x_; }
  [[nodiscard]] float GetMouseY() const { return mouse_y_; }

 private:
  float mouse_x_{0.0F};
  float mouse_y_{0.0F};
};

class MouseScrolledEvent : public Event {
 public:
  EVENT_TYPE("MouseScrolledEvent")

  explicit MouseScrolledEvent(float const delta_y) : y_offset_(delta_y) {}

  [[nodiscard]] std::string ToString() const override {
    return std::format("MouseScrolledEvent: {}", y_offset_);
  }

  [[nodiscard]] float GetYOffset() const { return y_offset_; }

 private:
  float y_offset_{0.0F};
};

class MouseButtonPressedEvent : public Event {
 public:
  EVENT_TYPE("MouseButtonPressedEvent")

  explicit MouseButtonPressedEvent(int button, int repeat_count)
      : button_(static_cast<core::MouseCode>(button)),
        repeat_count_(repeat_count) {}

  [[nodiscard]] std::string ToString() const override {
    return std::format("MouseButtonPressedEvent: {} ({} repeats)",
                       static_cast<int>(button_), repeat_count_);
  }

  [[nodiscard]] core::MouseCode GetButton() const { return button_; }
  [[nodiscard]] bool IsRepeated() const { return repeat_count_ > 0; }

 private:
  core::MouseCode button_{0};
  int repeat_count_{0};
};

class MouseButtonReleasedEvent : public Event {
 public:
  EVENT_TYPE("MouseButtonReleasedEvent")

  explicit MouseButtonReleasedEvent(int button)
      : button_(static_cast<core::MouseCode>(button)) {}

  [[nodiscard]] std::string ToString() const override {
    return std::format("MouseButtonReleasedEvent: {}",
                       static_cast<int>(button_));
  }

  [[nodiscard]] core::MouseCode GetButton() const { return button_; }

 private:
  core::MouseCode button_{0};
};

}  // namespace events

#endif  // !RHEOSPH_MOUSE_EVENT_H
