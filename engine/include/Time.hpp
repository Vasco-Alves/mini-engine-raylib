#pragma once

namespace me::time {
	// Seconds since last frame (smoothed by raylib).
	float Delta();

	// Seconds since engine started.
	double Elapsed();

	// Current frames per second.
	int GetFPS();
}
