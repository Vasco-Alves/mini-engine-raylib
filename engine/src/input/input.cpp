#include "mini-engine-raylib/input/input.hpp"

#include <raylib.h>

#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>
#include <utility>

namespace me::input {

	namespace {
		std::unordered_map<std::string, std::vector<Key>>         s_action_key_bindings;
		std::unordered_map<std::string, std::vector<MouseButton>> s_action_mouse_bindings;

		struct AxisBinding { Axis axis; float scale; };
		std::unordered_map<std::string, std::vector<AxisBinding>> s_axis_bindings;

		struct DigitalBinding { Key negative; Key positive; float scale; };
		std::unordered_map<std::string, std::vector<DigitalBinding>> s_digital_axis;

		std::unordered_map<std::string, float> s_axis_deadzone;
		std::unordered_map<std::string, std::pair<float, float>> s_axis_clamp;

		static int to_raylib_key(Key k) {
			switch (k) {
			case Key::Escape: return KEY_ESCAPE;
			case Key::Enter: return KEY_ENTER;
			case Key::Space: return KEY_SPACE;
			case Key::Tab: return KEY_TAB;
			case Key::Backspace: return KEY_BACKSPACE;
			case Key::Left: return KEY_LEFT;
			case Key::Right: return KEY_RIGHT;
			case Key::Up: return KEY_UP;
			case Key::Down: return KEY_DOWN;
			case Key::F1: return KEY_F1; case Key::F2: return KEY_F2;
			case Key::F3: return KEY_F3; case Key::F4: return KEY_F4;
			case Key::F5: return KEY_F5; case Key::F6: return KEY_F6;
			case Key::F7: return KEY_F7; case Key::F8: return KEY_F8;
			case Key::F9: return KEY_F9; case Key::F10: return KEY_F10;
			case Key::F11: return KEY_F11; case Key::F12: return KEY_F12;
			case Key::D0: return KEY_ZERO;  case Key::D1: return KEY_ONE;
			case Key::D2: return KEY_TWO;   case Key::D3: return KEY_THREE;
			case Key::D4: return KEY_FOUR;  case Key::D5: return KEY_FIVE;
			case Key::D6: return KEY_SIX;   case Key::D7: return KEY_SEVEN;
			case Key::D8: return KEY_EIGHT; case Key::D9: return KEY_NINE;
			case Key::A: return KEY_A; case Key::B: return KEY_B; case Key::C: return KEY_C;
			case Key::D: return KEY_D; case Key::E: return KEY_E; case Key::F: return KEY_F;
			case Key::G: return KEY_G; case Key::H: return KEY_H; case Key::I: return KEY_I;
			case Key::J: return KEY_J; case Key::K: return KEY_K; case Key::L: return KEY_L;
			case Key::M: return KEY_M; case Key::N: return KEY_N; case Key::O: return KEY_O;
			case Key::P: return KEY_P; case Key::Q: return KEY_Q; case Key::R: return KEY_R;
			case Key::S: return KEY_S; case Key::T: return KEY_T; case Key::U: return KEY_U;
			case Key::V: return KEY_V; case Key::W: return KEY_W; case Key::X: return KEY_X;
			case Key::Y: return KEY_Y; case Key::Z: return KEY_Z;
			}
			return KEY_NULL;
		}

		static int to_raylib_mouse_button(MouseButton b) {
			switch (b) {
			case MouseButton::Left:   return MOUSE_BUTTON_LEFT;
			case MouseButton::Right:  return MOUSE_BUTTON_RIGHT;
			case MouseButton::Middle: return MOUSE_BUTTON_MIDDLE;
			case MouseButton::Button4:return MOUSE_BUTTON_SIDE;
			case MouseButton::Button5:return MOUSE_BUTTON_EXTRA;
			}
			return MOUSE_BUTTON_LEFT;
		}

		static float sample_axis_raw(Axis a) {
			switch (a) {
			case Axis::MouseX:    return GetMouseDelta().x;
			case Axis::MouseY:    return GetMouseDelta().y;
			case Axis::MouseWheel:return GetMouseWheelMove();
			}
			return 0.0f;
		}

		static float sample_digital(Key neg, Key pos) {
			const bool n = IsKeyDown(to_raylib_key(neg));
			const bool p = IsKeyDown(to_raylib_key(pos));
			return (p ? 1.0f : 0.0f) - (n ? 1.0f : 0.0f);
		}

		template<typename Pred>
		static bool any_key_bound(const std::string& action, Pred pred) {
			auto it = s_action_key_bindings.find(action);
			if (it == s_action_key_bindings.end()) return false;
			for (Key k : it->second) {
				if (pred(to_raylib_key(k))) return true;
			}
			return false;
		}

		template<typename Pred>
		static bool any_mouse_bound(const std::string& action, Pred pred) {
			auto it = s_action_mouse_bindings.find(action);
			if (it == s_action_mouse_bindings.end()) return false;
			for (MouseButton b : it->second) {
				if (pred(to_raylib_mouse_button(b))) return true;
			}
			return false;
		}

		constexpr float kDefaultDeadzone = 0.0f;
		constexpr float kDefaultClampMin = -1.0f;
		constexpr float kDefaultClampMax = 1.0f;

		static float apply_deadzone(float v, float deadzone) {
			const float av = std::fabs(v);
			if (av < deadzone) return 0.0f;
			return v;
		}

		static float apply_clamp(float v, float minV, float maxV) {
			if (v < minV) return minV;
			if (v > maxV) return maxV;
			return v;
		}
	}

	void poll() {}

	void lock_cursor() { DisableCursor(); }
	void unlock_cursor() { EnableCursor(); }

	void bind_action(const std::string& action, Key key) {
		auto& v = s_action_key_bindings[action];
		if (std::find(v.begin(), v.end(), key) == v.end()) v.push_back(key);
	}

	void bind_action(const std::string& action, MouseButton button) {
		auto& v = s_action_mouse_bindings[action];
		if (std::find(v.begin(), v.end(), button) == v.end()) v.push_back(button);
	}

	bool unbind_action(const std::string& action, Key key) {
		auto it = s_action_key_bindings.find(action);
		if (it == s_action_key_bindings.end()) return false;
		auto& v = it->second;
		auto old = v.size();
		v.erase(std::remove(v.begin(), v.end(), key), v.end());
		return v.size() != old;
	}

	bool unbind_action(const std::string& action, MouseButton button) {
		auto it = s_action_mouse_bindings.find(action);
		if (it == s_action_mouse_bindings.end()) return false;
		auto& v = it->second;
		auto old = v.size();
		v.erase(std::remove(v.begin(), v.end(), button), v.end());
		return v.size() != old;
	}

	void clear_action(const std::string& action) {
		s_action_key_bindings.erase(action);
		s_action_mouse_bindings.erase(action);
	}

	bool action_down(const std::string& action) {
		return any_key_bound(action, &IsKeyDown) || any_mouse_bound(action, &IsMouseButtonDown);
	}

	bool action_pressed(const std::string& action) {
		return any_key_bound(action, &IsKeyPressed) || any_mouse_bound(action, &IsMouseButtonPressed);
	}

	bool action_released(const std::string& action) {
		return any_key_bound(action, &IsKeyReleased) || any_mouse_bound(action, &IsMouseButtonReleased);
	}

	std::vector<Key> get_key_bindings(const std::string& action) {
		auto it = s_action_key_bindings.find(action);
		return (it != s_action_key_bindings.end()) ? it->second : std::vector<Key>{};
	}

	std::vector<MouseButton> get_mouse_bindings(const std::string& action) {
		auto it = s_action_mouse_bindings.find(action);
		return (it != s_action_mouse_bindings.end()) ? it->second : std::vector<MouseButton>{};
	}

	void bind_axis(const std::string& axis_name, Axis axis, float scale) {
		s_axis_bindings[axis_name].push_back(AxisBinding{ axis, scale });
	}

	bool unbind_axis(const std::string& axis_name, Axis axis, float scale) {
		auto it = s_axis_bindings.find(axis_name);
		if (it == s_axis_bindings.end()) return false;
		auto& v = it->second;
		auto old = v.size();
		v.erase(std::remove_if(v.begin(), v.end(),
			[&](const AxisBinding& b) { return b.axis == axis && b.scale == scale; }), v.end());
		return v.size() != old;
	}

	void clear_axis(const std::string& axis_name) {
		s_axis_bindings.erase(axis_name);
		s_digital_axis.erase(axis_name);
		s_axis_deadzone.erase(axis_name);
		s_axis_clamp.erase(axis_name);
	}

	void bind_digital_axis(const std::string& axis_name, Key negative, Key positive, float scale) {
		auto& v = s_digital_axis[axis_name];
		auto it = std::find_if(v.begin(), v.end(), [&](const DigitalBinding& b) {
			return b.negative == negative && b.positive == positive && b.scale == scale;
			});
		if (it == v.end()) v.push_back(DigitalBinding{ negative, positive, scale });
	}

	bool unbind_digital_axis(const std::string& axis_name, Key negative, Key positive, float scale) {
		auto it = s_digital_axis.find(axis_name);
		if (it == s_digital_axis.end()) return false;
		auto& v = it->second;
		auto old = v.size();
		v.erase(std::remove_if(v.begin(), v.end(), [&](const DigitalBinding& b) {
			return b.negative == negative && b.positive == positive && b.scale == scale;
			}), v.end());
		return v.size() != old;
	}

	void clear_digital_axis(const std::string& axis_name) {
		s_digital_axis.erase(axis_name);
	}

	void set_axis_deadzone(const std::string& axis_name, float deadzone) {
		s_axis_deadzone[axis_name] = std::max(0.0f, deadzone);
	}

	void set_axis_clamp(const std::string& axis_name, float min_val, float max_val) {
		if (min_val > max_val) std::swap(min_val, max_val);
		s_axis_clamp[axis_name] = { min_val, max_val };
	}

	void clear_axis_config(const std::string& axis_name) {
		s_axis_deadzone.erase(axis_name);
		s_axis_clamp.erase(axis_name);
	}

	float axis_value(const std::string& axis_name) {
		float sum = 0.0f;

		if (auto it = s_axis_bindings.find(axis_name); it != s_axis_bindings.end()) {
			for (const auto& b : it->second) sum += sample_axis_raw(b.axis) * b.scale;
		}

		if (auto it = s_digital_axis.find(axis_name); it != s_digital_axis.end()) {
			for (const auto& b : it->second) sum += sample_digital(b.negative, b.positive) * b.scale;
		}

		float dz = s_axis_deadzone.count(axis_name) ? s_axis_deadzone[axis_name] : kDefaultDeadzone;
		sum = apply_deadzone(sum, dz);

		float minV = kDefaultClampMin, maxV = kDefaultClampMax;
		if (s_axis_clamp.count(axis_name)) {
			minV = s_axis_clamp[axis_name].first;
			maxV = s_axis_clamp[axis_name].second;
		}

		return apply_clamp(sum, minV, maxV);
	}

	me::math::Vec2 mouse_position() {
		return { GetMousePosition().x, GetMousePosition().y };
	}

	me::math::Vec2 mouse_delta() {
		return { GetMouseDelta().x, GetMouseDelta().y };
	}

	float mouse_wheel_delta() {
		return GetMouseWheelMove();
	}
}