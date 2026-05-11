#include "mini-engine-raylib/input/input_defaults.hpp"
#include "mini-engine-raylib/input/input.hpp"

namespace me::input {

	void setup_default_bindings() {
		// --- Movement: WASD and Arrows ---

		// Strafe (X)
		bind_digital_axis("MoveX", Key::A, Key::D, 1.0f);
		//bind_digital_axis("MoveX", Key::Left, Key::Right, 1.0f);

		// Forward/Back (Z)
		bind_digital_axis("MoveZ", Key::W, Key::S, 1.0f);
		//bind_digital_axis("MoveZ", Key::Up, Key::Down, 1.0f);

		// --- Look: Mouse ---
		bind_axis("LookX", Axis::MouseX, 0.1f);
		bind_axis("LookY", Axis::MouseY, 0.1f);

		// --- Buttons: Mouse ---
		bind_action("MouseRight", MouseButton::Right);
		bind_action("MouseMiddle", MouseButton::Middle);
		bind_action("MouseLeft", MouseButton::Left);

		// --- Configuration (Deadzone & Clamping) ---

		set_axis_deadzone("MoveX", 0.25f);
		set_axis_clamp("MoveX", -1.0f, 1.0f);

		// Added MoveZ config so W/S feels the same as A/D
		set_axis_deadzone("MoveZ", 0.25f);
		set_axis_clamp("MoveZ", -1.0f, 1.0f);

		// MoveY (Vertical/Fly) - Not bound by default, but configured just in case
		set_axis_deadzone("MoveY", 0.25f);
		set_axis_clamp("MoveY", -1.0f, 1.0f);

		set_axis_deadzone("LookX", 0.05f);
		set_axis_clamp("LookX", -5.0f, 5.0f);

		set_axis_deadzone("LookY", 0.05f);
		set_axis_clamp("LookY", -5.0f, 5.0f);
	}

} // namespace me::input