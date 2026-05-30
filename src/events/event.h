#ifndef RHEOSPH_EVENT_H
#define RHEOSPH_EVENT_H

#include <cstdint>
#include <string>

#include "CRC.h"  // NOLINT(misc-unused-include)

namespace events {

[[nodiscard]] inline std::uint32_t CalculateEventType(const char* event_name) {
  return CRC::Calculate(event_name, std::char_traits<char>::length(event_name),
                        CRC::CRC_32());
}

class Event {
 public:
  Event(const Event&) = default;
  Event(Event&&) = delete;
  Event() = default;
  Event& operator=(const Event&) = default;
  Event& operator=(Event&&) = delete;
  virtual ~Event() = default;

  [[nodiscard]] virtual std::uint32_t GetEventType() const = 0;
  [[nodiscard]] virtual std::string ToString() const {
    return std::to_string(GetEventType());
  };
  [[nodiscard]] bool IsHandled() const { return is_handled_; }
  void SetHandled(const bool handled = true) const { is_handled_ = handled; }

 private:
  mutable bool is_handled_{false};
};

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define EVENT_TYPE(event_type_name)                                          \
  [[nodiscard]] static std::uint32_t GetStaticEventType() {                  \
    static const std::uint32_t kEventType =                                  \
        CalculateEventType(event_type_name);                                 \
    return kEventType;                                                       \
  }                                                                          \
  [[nodiscard]] std::uint32_t GetEventType() const override {                \
    return GetStaticEventType();                                             \
  }

inline std::ostream& operator<<(std::ostream& ostream, const Event& event) {
  return ostream << event.ToString();
}

}  // namespace events

#endif  // !RHEOSPH_EVENT_H
