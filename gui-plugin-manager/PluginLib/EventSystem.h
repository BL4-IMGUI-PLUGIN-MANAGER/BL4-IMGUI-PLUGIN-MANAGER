#pragma once
#include <functional>
#include <vector>
#include <map>
#include <string>
#include <mutex>

namespace PluginAPI {

// Event types that plugins can hook into
enum class EventType {
    MenuOpened,
    MenuClosed,
    GameTick,
    PluginLoaded,
    PluginUnloaded,
    MAX_EVENTS
};

// Event callback type - simple function pointer
typedef void (*EventCallback)(void);

// Event dispatcher for managing callbacks
class EventDispatcher {
public:
    static EventDispatcher& Get() {
        static EventDispatcher instance;
        return instance;
    }

    // Register a callback for an event
    void Subscribe(EventType event, const std::string& id, EventCallback callback) {
        std::lock_guard<std::mutex> lock(m_Mutex);

        if (static_cast<int>(event) >= static_cast<int>(EventType::MAX_EVENTS)) {
            return;
        }
        m_callbacks[static_cast<int>(event)][id] = callback;
    }

    // Unregister a callback
    void Unsubscribe(EventType event, const std::string& id) {
        std::lock_guard<std::mutex> lock(m_Mutex);

        if (static_cast<int>(event) >= static_cast<int>(EventType::MAX_EVENTS)) {
            return;
        }
        auto& callbacks = m_callbacks[static_cast<int>(event)];
        auto it = callbacks.find(id);
        if (it != callbacks.end()) {
            callbacks.erase(it);
        }
    }

    // Dispatch an event to all subscribers
    void Dispatch(EventType event) {
        std::lock_guard<std::mutex> lock(m_Mutex);

        if (static_cast<int>(event) >= static_cast<int>(EventType::MAX_EVENTS)) {
            return;
        }
        auto& callbacks = m_callbacks[static_cast<int>(event)];
        for (auto& pair : callbacks) {
            if (pair.second) {
                pair.second();
            }
        }
    }

private:
    EventDispatcher() {
        // Initialize callback maps for all event types
        for (int i = 0; i < static_cast<int>(EventType::MAX_EVENTS); ++i) {
            m_callbacks[i];  // Ensure map exists
        }
    }

    std::map<int, std::map<std::string, EventCallback>> m_callbacks;
    mutable std::mutex m_Mutex;  // Protects m_callbacks from concurrent access
};

}  // namespace PluginAPI
