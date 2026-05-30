#ifndef RHEOSPH_EVENT_MANAGER_H
#define RHEOSPH_EVENT_MANAGER_H

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "event_handler.h"

namespace events {

using EventType = uint32_t;
using EventId = uint64_t;

class EventManager {
 public:
  void Shutdown();

  void Subscribe(EventType event_type,
                 std::unique_ptr<IEventHandlerWrapper>&& handler,
                 EventId event_id);
  void Unsubscribe(EventType event_type, std::string const& handler_name,
                   EventId event_id);
  void TriggerEvent(Event const& event, EventId event_id);
  void QueueEvent(std::unique_ptr<Event>&& event, EventId event_id);
  void DispatchEvents();

 private:
  std::vector<std::pair<std::unique_ptr<Event>, EventId>> events_queue_;
  std::unordered_map<EventType,
                     std::vector<std::unique_ptr<IEventHandlerWrapper>>>
      subscribers_;
  std::unordered_map<
      EventType,
      std::unordered_map<EventId,
                         std::vector<std::unique_ptr<IEventHandlerWrapper>>>>
      subscribers_by_event_id_;
};

extern EventManager event_manager;  // NOLINT

template <typename EventType>
inline void Subscribe(const EventHandler<EventType>& callback,
                      EventId event_id = 0,
                      const bool unsubscribe_on_success = false) {
  std::unique_ptr<IEventHandlerWrapper> handler = std::make_unique<
      EventHandlerWrapper<EventType>>(callback, unsubscribe_on_success);
  event_manager.Subscribe(EventType::GetStaticEventType(), std::move(handler),
                          event_id);
}

template <typename EventType>
inline void Unsubscribe(const EventHandler<EventType>& callback,
                        EventId event_id = 0) {
  const std::string handler_name = callback.target_type().name();
  event_manager.Unsubscribe(EventType::GetStaticEventType(), handler_name,
                            event_id);
}

inline void TriggerEvent(const Event& triggered_event, EventId event_id = 0) {
  event_manager.TriggerEvent(triggered_event, event_id);
}

inline void QueueEvent(std::unique_ptr<Event>&& queued_event,  // NOLINT
                       EventId event_id = 0) {
  event_manager.QueueEvent(std::forward<std::unique_ptr<Event>>(queued_event),
                           event_id);
}

}  // namespace events

#endif  // !RHEOSPH_EVENT_MANAGER_H
