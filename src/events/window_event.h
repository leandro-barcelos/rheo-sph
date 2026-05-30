#ifndef RHEOSPH_WINDOW_EVENT_H
#define RHEOSPH_WINDOW_EVENT_H

#include <format>
#include <glm/vec2.hpp>

#include "event.h"

namespace events {

class WindowResizedEvent : public Event {
 public:
  EVENT_TYPE("WindowResizedEvent")

  explicit WindowResizedEvent(int width, int height)
      : width_(width), height_(height) {}

  [[nodiscard]] std::string ToString() const override {
    return std::format("WindowResizedEvent: {}x{}", width_, height_);
  }

  [[nodiscard]] int GetWidth() const { return width_; }
  [[nodiscard]] int GetHeight() const { return height_; }

 private:
  int width_;
  int height_;
};

}  // namespace events

#endif  // !RHEOSPH_WINDOW_EVENT_H
