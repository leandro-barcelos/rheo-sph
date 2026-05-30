#ifndef RHEOSPH_KEYBOARD_EVENT_H
#define RHEOSPH_KEYBOARD_EVENT_H

#include <format>
#include <glm/vec2.hpp>

#include "event.h"
#include "rheo-sph/src/core/key_codes.h"

namespace events {

class KeyPressedEvent : public Event {
 public:
  EVENT_TYPE("KeyPressedEvent")

  explicit KeyPressedEvent(int key_code, int repeat_count)
      : key_(static_cast<core::KeyCode>(key_code)),
        repeat_count_(repeat_count) {}

  [[nodiscard]] std::string ToString() const override {
    return std::format("KeyPressedEvent: {} ({} repeats)",
                       static_cast<int>(key_), repeat_count_);
  }

  [[nodiscard]] core::KeyCode GetKey() const { return key_; }
  [[nodiscard]] bool IsRepeated() const { return repeat_count_ > 0; }

 private:
  core::KeyCode key_{0};
  int repeat_count_{0};
};

class KeyReleasedEvent : public Event {
 public:
  EVENT_TYPE("KeyReleasedEvent")

  explicit KeyReleasedEvent(int key_code)
      : key_(static_cast<core::KeyCode>(key_code)) {}

  [[nodiscard]] std::string ToString() const override {
    return std::format("KeyReleasedEvent: {}", static_cast<int>(key_));
  }

  [[nodiscard]] core::KeyCode GetKey() const { return key_; }

 private:
  core::KeyCode key_{0};
};

class KeyTypedEvent : public Event {
 public:
  EVENT_TYPE("KeyTypedEvent")

  explicit KeyTypedEvent(int key_code)
      : key_(static_cast<core::KeyCode>(key_code)) {}

  [[nodiscard]] std::string ToString() const override {
    return std::format("KeyTypedEvent: {}", static_cast<int>(key_));
  }

  [[nodiscard]] core::KeyCode GetKey() const { return key_; }

 private:
  core::KeyCode key_{0};
};

}  // namespace events

#endif  // !RHEOSPH_KEYBOARD_EVENT_H
