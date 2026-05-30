#include "event_manager.h"

#include <stdexcept>

namespace events {

EventManager event_manager;  // NOLINT

void EventManager::Shutdown() {
  events_queue_.clear();
  subscribers_.clear();
  subscribers_by_event_id_.clear();
}

void EventManager::Subscribe(EventType event_type,
                             std::unique_ptr<IEventHandlerWrapper>&& handler,
                             EventId event_id) {
  if (event_id != 0U) {
    auto subscribers = subscribers_by_event_id_.find(event_type);

    if (subscribers != subscribers_by_event_id_.end()) {
      auto& handlers_map = subscribers->second;
      auto handlers = handlers_map.find(event_id);
      if (handlers != handlers_map.end()) {
        handlers->second.emplace_back(std::move(handler));
        return;
      }
    }
    subscribers_by_event_id_[event_type][event_id].emplace_back(
        std::move(handler));

  } else {
    auto subscribers = subscribers_.find(event_type);
    if (subscribers != subscribers_.end()) {
      auto& handlers = subscribers->second;
      for (auto& iterator : handlers) {
        if (iterator->GetType() == handler->GetType()) {
          throw std::runtime_error(
              "[ERROR] Events: Attempting to double-register callback");
          return;
        }
      }
      handlers.emplace_back(std::move(handler));
    } else {
      subscribers_[event_type].emplace_back(std::move(handler));
    }
  }
}

void EventManager::Unsubscribe(EventType event_type,
                               const std::string& handler_name,
                               EventId event_id) {
  if (event_id != 0U) {
    auto subscribers = subscribers_by_event_id_.find(event_type);
    if (subscribers != subscribers_by_event_id_.end()) {
      auto& handlers_map = subscribers->second;
      auto handlers = handlers_map.find(event_id);
      if (handlers != handlers_map.end()) {
        auto& callbacks = handlers->second;
        for (auto it = callbacks.begin(); it != callbacks.end(); ++it) {
          if (it->get()->GetType() == handler_name) {
            it = callbacks.erase(it);
            return;
          }
        }
      }
    }
  } else {
    auto handlers_it = subscribers_.find(event_type);
    if (handlers_it != subscribers_.end()) {
      auto& handlers = handlers_it->second;
      for (auto it = handlers.begin(); it != handlers.end(); ++it) {
        if (it->get()->GetType() == handler_name) {
          it = handlers.erase(it);
          return;
        }
      }
    }
  }
}

void EventManager::TriggerEvent(const Event& event, EventId event_id) {
  auto subscribers = subscribers_.find(event.GetEventType());
  if (subscribers != subscribers_.end()) {
    for (auto& handler : subscribers->second) {
      handler->Exec(event);
    }
  }

  auto handlers_map = subscribers_by_event_id_.find(event.GetEventType());
  if (handlers_map != subscribers_by_event_id_.end()) {
    auto handlers = handlers_map->second.find(event_id);
    if (handlers != handlers_map->second.end()) {
      auto& callbacks = handlers->second;
      for (auto it = callbacks.begin(); it != callbacks.end();) {
        auto& handler = *it;
        handler->Exec(event);
        if (handler->IsDestroyOnSuccess()) {
          it = callbacks.erase(it);
        } else {
          ++it;
        }
      }
    }
  }
}

void EventManager::QueueEvent(std::unique_ptr<Event>&& event,
                              EventId event_id) {
  events_queue_.emplace_back(std::move(event), event_id);
}

void EventManager::DispatchEvents() {
  for (auto event_it = events_queue_.begin();
       event_it != events_queue_.end();) {
    if (!event_it->first->IsHandled()) {
      TriggerEvent(*event_it->first, event_it->second);
    }
    event_it = events_queue_.erase(event_it);
  }
}

}  // namespace events