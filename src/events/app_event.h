#ifndef RHEOSPH_APP_EVENT_H
#define RHEOSPH_APP_EVENT_H

#include "rheo-sph/src/events/event.h"
namespace events {

class QuitRequestedEvent : public Event {
 public:
  EVENT_TYPE("QuitRequestedEvent")

  [[nodiscard]] std::string ToString() const override {
    return "QuitRequestedEvent";
  }
};

}  // namespace events

#endif  // !RHEOSPH_APP_EVENT_H