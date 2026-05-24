#pragma once

#include <mini-ecs/registry.hpp>

#include <raylib.h>

namespace me::systems {

	// Sweeps through all AudioSourceComponents and plays triggered sounds 
	// with 3D spatial math applied relative to the AudioListenerComponent
	void audio_update(me::Registry& registry, Vector3 fallback_pos = { 0,0,0 }, Vector3 fallback_forward = { 0,0,-1 }, Vector3 fallback_up = { 0,1,0 });

} // me::systems