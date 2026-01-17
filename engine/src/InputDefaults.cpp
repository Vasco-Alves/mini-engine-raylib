#include "InputDefaults.hpp"

namespace me::input {

	void SetupDefaultBindings() {
		// Movement: WASD and arrows
		BindDigitalAxis("MoveX", Key::A, Key::D, 1.0f);
		BindDigitalAxis("MoveX", Key::Left, Key::Right, 1.0f);

		BindDigitalAxis("MoveY", Key::W, Key::S, 1.0f);
		BindDigitalAxis("MoveY", Key::Up, Key::Down, 1.0f);

		// Look with mouse deltas
		BindAxis("LookX", Axis::MouseX, 0.1f);
		BindAxis("LookY", Axis::MouseY, 0.1f);

		// Shape input: clamp & deadzone
		SetAxisDeadzone("MoveX", 0.25f);
		SetAxisClamp("MoveX", -1.0f, 1.0f);

		SetAxisDeadzone("MoveY", 0.25f);
		SetAxisClamp("MoveY", -1.0f, 1.0f);

		SetAxisDeadzone("LookX", 0.05f);
		SetAxisClamp("LookX", -5.0f, 5.0f);

		SetAxisDeadzone("LookY", 0.05f);
		SetAxisClamp("LookY", -5.0f, 5.0f);
	}

} // namespace me::input
