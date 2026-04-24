#include "mini-engine-raylib/core/time.hpp"

#include <raylib.h>

namespace me::time {

	float delta() {
		return GetFrameTime();
	}

	double elapsed() {
		return GetTime();
	}

	int get_fps() {
		return ::GetFPS();
	}

} // namespace me::time