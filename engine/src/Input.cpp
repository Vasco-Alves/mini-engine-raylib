#include "Input.hpp"

#include <raylib.h>

#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>
#include <utility>
#include <limits>

namespace me::input {

	namespace {
		// Actions
		std::unordered_map<std::string, std::vector<Key>>         s_ActionKeyBindings;
		std::unordered_map<std::string, std::vector<MouseButton>> s_ActionMouseBindings;

		// Analog axes (mouse X/Y/Wheel) with scales
		struct AxisBinding { Axis axis; float scale; };
		std::unordered_map<std::string, std::vector<AxisBinding>> s_AxisBindings;

		// Digital axes (negative/positive keys) with scale
		struct DigitalBinding { Key negative; Key positive; float scale; };
		std::unordered_map<std::string, std::vector<DigitalBinding>> s_DigitalAxis;

		// Shaping: per-axis deadzone and clamp
		std::unordered_map<std::string, float> s_AxisDeadzone; // default 0.0
		std::unordered_map<std::string, std::pair<float, float>> s_AxisClamp; // default [-1,+1]

		// ----- translation helpers -----
		static int ToRaylibKey(Key k) {
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

		static int ToRaylibMouseButton(MouseButton b) {
			switch (b) {
			case MouseButton::Left:   return MOUSE_BUTTON_LEFT;
			case MouseButton::Right:  return MOUSE_BUTTON_RIGHT;
			case MouseButton::Middle: return MOUSE_BUTTON_MIDDLE;
			case MouseButton::Button4:return MOUSE_BUTTON_SIDE;
			case MouseButton::Button5:return MOUSE_BUTTON_EXTRA;
			}
			return MOUSE_BUTTON_LEFT;
		}

		static float SampleAxisRaw(Axis a) {
			switch (a) {
			case Axis::MouseX:    return GetMouseDelta().x;
			case Axis::MouseY:    return GetMouseDelta().y;
			case Axis::MouseWheel:return GetMouseWheelMove();
			}
			return 0.0f;
		}

		static float SampleDigital(Key neg, Key pos) {
			const bool n = IsKeyDown(ToRaylibKey(neg));
			const bool p = IsKeyDown(ToRaylibKey(pos));
			// (-1, 0, +1)
			return (p ? 1.0f : 0.0f) - (n ? 1.0f : 0.0f);
		}

		template<typename Pred>
		static bool AnyKeyBound(const std::string& action, Pred pred) {
			auto it = s_ActionKeyBindings.find(action);
			if (it == s_ActionKeyBindings.end()) return false;
			for (Key k : it->second) {
				if (pred(ToRaylibKey(k))) return true;
			}
			return false;
		}

		template<typename Pred>
		static bool AnyMouseBound(const std::string& action, Pred pred) {
			auto it = s_ActionMouseBindings.find(action);
			if (it == s_ActionMouseBindings.end()) return false;
			for (MouseButton b : it->second) {
				if (pred(ToRaylibMouseButton(b))) return true;
			}
			return false;
		}

		// Defaults if no per-axis config is set
		constexpr float kDefaultDeadzone = 0.0f;
		constexpr float kDefaultClampMin = -1.0f;
		constexpr float kDefaultClampMax = 1.0f;

		static float ApplyDeadzone(float v, float deadzone) {
			const float av = std::fabs(v);
			if (av < deadzone) return 0.0f; // hard deadzone
			return v;
		}

		static float ApplyClamp(float v, float minV, float maxV) {
			if (v < minV) return minV;
			if (v > maxV) return maxV;
			return v;
		}
	} // namespace

	// ---------------- per-frame hook ----------------
	void Poll() {
		// raylib tracks per-frame transitions internally.
		// Keep this for symmetry & future expansion (text input, gamepad, etc).
		(void)0;
	}

	// ---------------- actions ----------------
	void BindAction(const std::string& action, Key key) {
		auto& v = s_ActionKeyBindings[action];
		if (std::find(v.begin(), v.end(), key) == v.end()) v.push_back(key);
	}

	void BindAction(const std::string& action, MouseButton button) {
		auto& v = s_ActionMouseBindings[action];
		if (std::find(v.begin(), v.end(), button) == v.end()) v.push_back(button);
	}

	bool UnbindAction(const std::string& action, Key key) {
		auto it = s_ActionKeyBindings.find(action);
		if (it == s_ActionKeyBindings.end()) return false;
		auto& v = it->second;
		auto old = v.size();
		v.erase(std::remove(v.begin(), v.end(), key), v.end());
		return v.size() != old;
	}

	bool UnbindAction(const std::string& action, MouseButton button) {
		auto it = s_ActionMouseBindings.find(action);
		if (it == s_ActionMouseBindings.end()) return false;
		auto& v = it->second;
		auto old = v.size();
		v.erase(std::remove(v.begin(), v.end(), button), v.end());
		return v.size() != old;
	}

	void ClearAction(const std::string& action) {
		s_ActionKeyBindings.erase(action);
		s_ActionMouseBindings.erase(action);
	}

	bool ActionDown(const std::string& action) {
		return AnyKeyBound(action, &IsKeyDown) || AnyMouseBound(action, &IsMouseButtonDown);
	}

	bool ActionPressed(const std::string& action) {
		return AnyKeyBound(action, &IsKeyPressed) || AnyMouseBound(action, &IsMouseButtonPressed);
	}

	bool ActionReleased(const std::string& action) {
		return AnyKeyBound(action, &IsKeyReleased) || AnyMouseBound(action, &IsMouseButtonReleased);
	}

	std::vector<Key> GetKeyBindings(const std::string& action) {
		auto it = s_ActionKeyBindings.find(action);
		if (it == s_ActionKeyBindings.end()) return {};
		return it->second;
	}

	std::vector<MouseButton> GetMouseBindings(const std::string& action) {
		auto it = s_ActionMouseBindings.find(action);
		if (it == s_ActionMouseBindings.end()) return {};
		return it->second;
	}

	// ---------------- analog axes ----------------
	void BindAxis(const std::string& axisName, Axis axis, float scale) {
		auto& v = s_AxisBindings[axisName];
		v.push_back(AxisBinding{ axis, scale });
	}

	bool UnbindAxis(const std::string& axisName, Axis axis, float scale) {
		auto it = s_AxisBindings.find(axisName);
		if (it == s_AxisBindings.end()) return false;
		auto& v = it->second;
		auto old = v.size();
		v.erase(std::remove_if(v.begin(), v.end(),
			[&](const AxisBinding& b) { return b.axis == axis && b.scale == scale; }),
			v.end());
		return v.size() != old;
	}

	void ClearAxis(const std::string& axisName) {
		s_AxisBindings.erase(axisName);
		s_DigitalAxis.erase(axisName);
		s_AxisDeadzone.erase(axisName);
		s_AxisClamp.erase(axisName);
	}

	// ---------------- digital axes ----------------
	void BindDigitalAxis(const std::string& axisName, Key negative, Key positive, float scale) {
		auto& v = s_DigitalAxis[axisName];
		auto it = std::find_if(v.begin(), v.end(),
			[&](const DigitalBinding& b) {
				return b.negative == negative && b.positive == positive && b.scale == scale;
			});
		if (it == v.end()) v.push_back(DigitalBinding{ negative, positive, scale });
	}

	bool UnbindDigitalAxis(const std::string& axisName, Key negative, Key positive, float scale) {
		auto it = s_DigitalAxis.find(axisName);
		if (it == s_DigitalAxis.end()) return false;
		auto& v = it->second;
		auto old = v.size();
		v.erase(std::remove_if(v.begin(), v.end(),
			[&](const DigitalBinding& b) {
				return b.negative == negative && b.positive == positive && b.scale == scale;
			}), v.end());
		return v.size() != old;
	}

	void ClearDigitalAxis(const std::string& axisName) {
		s_DigitalAxis.erase(axisName);
	}

	// ---------------- shaping ----------------
	void SetAxisDeadzone(const std::string& axisName, float deadzone) {
		if (deadzone < 0.0f) deadzone = 0.0f;
		s_AxisDeadzone[axisName] = deadzone;
	}

	void SetAxisClamp(const std::string& axisName, float minVal, float maxVal) {
		if (minVal > maxVal) std::swap(minVal, maxVal);
		s_AxisClamp[axisName] = { minVal, maxVal };
	}

	void ClearAxisConfig(const std::string& axisName) {
		s_AxisDeadzone.erase(axisName);
		s_AxisClamp.erase(axisName);
	}

	// Sum analog + digital, then apply deadzone and clamp.
	float AxisValue(const std::string& axisName) {
		float sum = 0.0f;

		// 1) analog (mouse)
		if (auto it = s_AxisBindings.find(axisName); it != s_AxisBindings.end()) {
			for (const auto& b : it->second) {
				sum += SampleAxisRaw(b.axis) * b.scale;
			}
		}

		// 2) digital (keys)
		if (auto it = s_DigitalAxis.find(axisName); it != s_DigitalAxis.end()) {
			for (const auto& b : it->second) {
				sum += SampleDigital(b.negative, b.positive) * b.scale;
			}
		}

		// 3) deadzone (per-axis or default)
		float dz = kDefaultDeadzone;
		if (auto it = s_AxisDeadzone.find(axisName); it != s_AxisDeadzone.end()) {
			dz = it->second;
		}
		sum = ApplyDeadzone(sum, dz);

		// 4) clamp (per-axis or default)
		float minV = kDefaultClampMin, maxV = kDefaultClampMax;
		if (auto it = s_AxisClamp.find(axisName); it != s_AxisClamp.end()) {
			minV = it->second.first; maxV = it->second.second;
		}
		sum = ApplyClamp(sum, minV, maxV);

		return sum;
	}

	// ---------------- raw mouse helpers ----------------
	me::math::Vec2 MousePosition() {
		const auto p = GetMousePosition();
		return me::math::Vec2{ p.x, p.y };
	}

	me::math::Vec2 MouseDelta() {
		const auto d = GetMouseDelta();
		return me::math::Vec2{ d.x, d.y };
	}

	float MouseWheelDelta() {
		return GetMouseWheelMove();
	}

} // namespace me::input
