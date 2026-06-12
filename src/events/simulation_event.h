#ifndef RHEOSPH_SIMULATION_EVENT_H
#define RHEOSPH_SIMULATION_EVENT_H

#include <format>

#include "rheo-sph/src/events/event.h"

namespace events {

class PlayButtonPressedEvent : public Event {
 public:
  EVENT_TYPE("PlayButtonPressedEvent")

  [[nodiscard]] std::string ToString() const override {
    return "PlayButtonPressedEvent";
  }
};

class PauseButtonPressedEvent : public Event {
 public:
  EVENT_TYPE("PauseButtonPressedEvent")

  [[nodiscard]] std::string ToString() const override {
    return "PauseButtonPressedEvent";
  }
};

class ResetButtonPressedEvent : public Event {
 public:
  EVENT_TYPE("ResetButtonPressedEvent")

  [[nodiscard]] std::string ToString() const override {
    return "ResetButtonPressedEvent";
  }
};

}  // namespace events

#endif  // !RHEOSPH_SIMULATION_EVENT_H