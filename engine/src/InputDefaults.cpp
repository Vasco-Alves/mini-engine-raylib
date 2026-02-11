#include "InputDefaults.hpp"
#include "Input.hpp"

namespace me::input {

	void SetupDefaultBindings() {
		// --- Movement: WASD and Arrows ---

		// Strafe (X)
		BindDigitalAxis("MoveX", Key::A, Key::D, 1.0f);
		BindDigitalAxis("MoveX", Key::Left, Key::Right, 1.0f);

		// Forward/Back (Z)
		BindDigitalAxis("MoveZ", Key::W, Key::S, 1.0f);
		BindDigitalAxis("MoveZ", Key::Up, Key::Down, 1.0f);

		// --- Look: Mouse ---
		BindAxis("LookX", Axis::MouseX, 0.1f);
		BindAxis("LookY", Axis::MouseY, 0.1f);

		// --- Configuration (Deadzone & Clamping) ---

		SetAxisDeadzone("MoveX", 0.25f);
		SetAxisClamp("MoveX", -1.0f, 1.0f);

		// Added MoveZ config so W/S feels the same as A/D
		SetAxisDeadzone("MoveZ", 0.25f);
		SetAxisClamp("MoveZ", -1.0f, 1.0f);

		// MoveY (Vertical/Fly) - Not bound by default, but configured just in case
		SetAxisDeadzone("MoveY", 0.25f);
		SetAxisClamp("MoveY", -1.0f, 1.0f);

		SetAxisDeadzone("LookX", 0.05f);
		SetAxisClamp("LookX", -5.0f, 5.0f);

		SetAxisDeadzone("LookY", 0.05f);
		SetAxisClamp("LookY", -5.0f, 5.0f);
	}

} // namespace me::input