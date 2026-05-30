#ifndef RHEOSPH_UI_EVENT_H
#define RHEOSPH_UI_EVENT_H

#include <string>

#include "event.h"

namespace events {

class UiFocusedEvent : public Event {
 public:
  EVENT_TYPE("UiFocusedEvent")

  UiFocusedEvent() = default;

  [[nodiscard]] std::string ToString() const override {
    return "UiFocusedEvent";
  }
};

class UiUnfocusedEvent : public Event {
 public:
  EVENT_TYPE("UiUnfocusedEvent")

  UiUnfocusedEvent() = default;

  [[nodiscard]] std::string ToString() const override {
    return "UiUnfocusedEvent";
  }
};

}  // namespace events

#endif  // !RHEOSPH_UI_EVENT_H