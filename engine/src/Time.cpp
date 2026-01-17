#include "Time.hpp"

#include <raylib.h>

namespace me::time {

	float Delta() {
		return GetFrameTime();
	}

	double Elapsed() {
		return GetTime();
	}

	int GetFPS() {
		return ::GetFPS();
	}

} // namespace me::time
