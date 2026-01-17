#pragma once

#include <string>
#include <vector>

#include "Math.hpp"  // me::math::Vec2

namespace me::input {

	// ---------- keyboard ----------
	enum class Key {
		Escape, Enter, Space, Tab, Backspace,
		Left, Right, Up, Down,
		F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
		D0, D1, D2, D3, D4, D5, D6, D7, D8, D9,
		A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
	};

	// ---------- mouse buttons ----------
	enum class MouseButton {
		Left, Right, Middle, Button4, Button5
	};

	// ---------- analog axes sources ----------
	enum class Axis {
		MouseX,      // horizontal mouse delta (per frame)
		MouseY,      // vertical mouse delta (per frame)
		MouseWheel   // wheel delta (usually vertical) per frame
	};

	// Called once per frame by the engine (inside me::Update()).
	void Poll();

	// -------- actions (support multiple bindings) --------
	void BindAction(const std::string& action, Key key);
	void BindAction(const std::string& action, MouseButton button);

	bool UnbindAction(const std::string& action, Key key);
	bool UnbindAction(const std::string& action, MouseButton button);

	void ClearAction(const std::string& action);

	bool ActionDown(const std::string& action);      // any bound input is down
	bool ActionPressed(const std::string& action);   // any bound pressed this frame
	bool ActionReleased(const std::string& action);  // any bound released this frame

	std::vector<Key>         GetKeyBindings(const std::string& action);
	std::vector<MouseButton> GetMouseBindings(const std::string& action);

	// -------- analog axes (sum of sources * scale) --------
	void  BindAxis(const std::string& axisName, Axis axis, float scale = 1.0f);
	bool  UnbindAxis(const std::string& axisName, Axis axis, float scale = 1.0f);
	void  ClearAxis(const std::string& axisName);

	// Digital axis = two keys (negative/positive) mapped to a single axis in [-1, 1] * scale.
	void  BindDigitalAxis(const std::string& axisName, Key negative, Key positive, float scale = 1.0f);
	bool  UnbindDigitalAxis(const std::string& axisName, Key negative, Key positive, float scale = 1.0f);
	void  ClearDigitalAxis(const std::string& axisName);

	// -------- axis shaping (deadzone + clamp) --------
	// Set per-axis deadzone: if |value| < deadzone => returns 0. Default = 0.0 (no deadzone).
	void  SetAxisDeadzone(const std::string& axisName, float deadzone);

	// Set per-axis clamp range (min/max). Default = [-1, +1].
	void  SetAxisClamp(const std::string& axisName, float minVal, float maxVal);

	// Remove custom shaping for an axis (restores defaults).
	void  ClearAxisConfig(const std::string& axisName);

	// Get shaped axis value (sum -> deadzone -> clamp)
	float AxisValue(const std::string& axisName);

	// -------- raw mouse helpers (no bindings required) --------
	me::math::Vec2  MousePosition();     // window coordinates
	me::math::Vec2  MouseDelta();        // delta since last frame
	float MouseWheelDelta();   // wheel delta this frame
}
