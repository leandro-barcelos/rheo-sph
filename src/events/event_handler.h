#ifndef RHEOSPH_EVENT_HANDLER_H
#define RHEOSPH_EVENT_HANDLER_H

#include <functional>
#include <string>

#include "event.h"

namespace events {

template <typename EventType>
using EventHandler = std::function<void(EventType const& event_type)>;

class IEventHandlerWrapper {
 public:
  IEventHandlerWrapper() = default;
  IEventHandlerWrapper(IEventHandlerWrapper const&) = default;
  IEventHandlerWrapper(IEventHandlerWrapper&&) = delete;
  IEventHandlerWrapper& operator=(IEventHandlerWrapper const&) = default;
  IEventHandlerWrapper& operator=(IEventHandlerWrapper&&) = delete;
  virtual ~IEventHandlerWrapper() = default;

  void Exec(Event const& event) { Call(event); }

  [[nodiscard]] virtual std::string GetType() const = 0;
  [[nodiscard]] virtual bool IsDestroyOnSuccess() const = 0;

 private:
  virtual void Call(Event const& event) = 0;
};

template <typename EventType>
class EventHandlerWrapper : public IEventHandlerWrapper {
 public:
  explicit EventHandlerWrapper(EventHandler<EventType> const& handler,
                               bool const destroy_on_success = false)
      : handler_(handler),
        handler_type_(handler_.target_type().name()),
        destroy_on_success_(destroy_on_success) {}

 private:
  void Call(Event const& event) override {
    if (event.GetEventType() == EventType::GetStaticEventType()) {
      handler_(static_cast<EventType const&>(event));
    }
  }

  [[nodiscard]] std::string GetType() const override { return handler_type_; }
  [[nodiscard]] bool IsDestroyOnSuccess() const override {
    return destroy_on_success_;
  }

  EventHandler<EventType> handler_;
  std::string handler_type_;
  bool destroy_on_success_{false};
};

}  // namespace events

#endif  // !RHEOSPH_EVENT_HANDLER_H
