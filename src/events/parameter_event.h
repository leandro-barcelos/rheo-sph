#ifndef RHEOSPH_PARAMETER_EVENT_H
#define RHEOSPH_PARAMETER_EVENT_H

#include <format>
#include "rheo-sph/src/events/event.h"

namespace events {

class DEMResolutionChangedEvent : public Event {
 public:
  EVENT_TYPE("DEMResolutionChangedEvent")

  explicit DEMResolutionChangedEvent(float dem_resolution)
      : dem_resolution_(dem_resolution) {}

  [[nodiscard]] std::string ToString() const override {
    return std::format("DEMResolutionChangedEvent: {}",
        dem_resolution_);
  }

 private:
  float dem_resolution_;
};

}  // namespace events

#endif  // !RHEOSPH_PARAMETER_EVENT_H
