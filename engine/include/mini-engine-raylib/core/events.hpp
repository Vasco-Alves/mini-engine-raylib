#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <cstdint>

#include "mini-engine-raylib/core/logger.hpp"

namespace me {

	// 1. The Base Event Class
	class Event {
	public:
		virtual ~Event() = default;
		bool handled = false; // If true, stops the event from propagating further
	};

	// 2. Specific Events
	namespace events {

		// Logger Event
		class LogEvent : public Event {
		public:
			std::string message;
			me::logger::LogLevel level;

			LogEvent(const std::string& msg, me::logger::LogLevel lvl) : message(msg), level(lvl) {}
		};

		// Scene / File Events
		class SceneLoadedEvent : public Event {
		public:
			std::string filepath;
			SceneLoadedEvent(const std::string& path) : filepath(path) {}
		};

		// Editor UI Events
		class EntitySelectedEvent : public Event {
		public:
			uint32_t entity_id; // 0xFFFFFFFF means deselect
			EntitySelectedEvent(uint32_t id) : entity_id(id) {}
		};

		// Engine Core State Events (For Physics/Lua)
		class PlayStateChangedEvent : public Event {
		public:
			bool is_playing;
			PlayStateChangedEvent(bool playing) : is_playing(playing) {}
		};

		// Play / Stop Events
		class PauseStateChangedEvent : public Event {
		public:
			bool is_paused;
			PauseStateChangedEvent(bool paused) : is_paused(paused) {}
		};

	} // namespace me::events

	// 3. The Central Event Bus
	class EventBus {
	private:
		// We store type-erased callbacks, and cast them back when an event fires
		using EventCallback = std::function<void(Event*)>;
		std::unordered_map<std::type_index, std::vector<EventCallback>> m_Subscribers;

	public:
		// Subscribe to a specific event type
		template<typename T, typename F>
		void subscribe(F&& callback) {
			m_Subscribers[typeid(T)].push_back([cb = std::forward<F>(callback)](Event* e) {
				cb(static_cast<T*>(e)); // Safely cast back to the specific event
				});
		}

		// Publish an event immediately to all listeners
		template<typename T, typename... Args>
		void publish(Args&&... args) {
			T eventInstance(std::forward<Args>(args)...);

			auto it = m_Subscribers.find(typeid(T));
			if (it != m_Subscribers.end()) {
				for (auto& callback : it->second) {
					if (eventInstance.handled) break;
					callback(&eventInstance);
				}
			}
		}
	};

	// 4. Global Singleton Access
	inline EventBus& get_event_bus() {
		static EventBus s_Instance;
		return s_Instance;
	}

} // namespace me
