#pragma once

#include <string>
#include <vector>

#include <raylib.h>

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

	// Called once per frame by the engine
	void poll();

	void lock_cursor();
	void unlock_cursor();

	// Editor playtest gate. When CLOSED, every input query returns neutral
	// (actions false, axes 0, mouse delta 0) and lock_cursor() is deferred so the
	// cursor stays free — the editor closes it so a game running in play mode only
	// receives input once the viewport is focused, and Escape can hand the mouse
	// back for tweaking panels. Re-opening restores the game's own lock/unlock
	// intent. The game runtime never closes it, so a shipped game is unaffected.
	void set_input_gate(bool open);
	bool is_input_gate_open();

	// -------- actions (support multiple bindings) --------
	void bind_action(const std::string& action, Key key);
	void bind_action(const std::string& action, MouseButton button);

	bool unbind_action(const std::string& action, Key key);
	bool unbind_action(const std::string& action, MouseButton button);

	void clear_action(const std::string& action);

	bool action_down(const std::string& action);      // any bound input is down
	bool action_pressed(const std::string& action);   // any bound pressed this frame
	bool action_released(const std::string& action);  // any bound released this frame

	std::vector<Key>         get_key_bindings(const std::string& action);
	std::vector<MouseButton> get_mouse_bindings(const std::string& action);

	// -------- analog axes (sum of sources * scale) --------
	void bind_axis(const std::string& axis_name, Axis axis, float scale = 1.0f);
	bool unbind_axis(const std::string& axis_name, Axis axis, float scale = 1.0f);
	void clear_axis(const std::string& axis_name);

	// Digital axis = two keys (negative/positive) mapped to a single axis in [-1, 1] * scale.
	void bind_digital_axis(const std::string& axis_name, Key negative, Key positive, float scale = 1.0f);
	bool unbind_digital_axis(const std::string& axis_name, Key negative, Key positive, float scale = 1.0f);
	void clear_digital_axis(const std::string& axis_name);

	// -------- axis shaping (deadzone + clamp) --------
	void set_axis_deadzone(const std::string& axis_name, float deadzone);
	void set_axis_clamp(const std::string& axis_name, float min_val, float max_val);
	void clear_axis_config(const std::string& axis_name);

	// Get shaped axis value (sum -> deadzone -> clamp)
	float axis_value(const std::string& axis_name);

	// -------- raw mouse helpers (no bindings required) --------
	Vector2	mouse_position();
	Vector2	mouse_delta();
	float	mouse_wheel_delta();
}