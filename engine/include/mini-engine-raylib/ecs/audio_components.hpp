#pragma once
#include "mini-engine-raylib/audio/audio.hpp"

namespace me::components {

	// Attach this to a Camera Entity
	struct AudioListenerComponent {
		bool active = true;
	};

	// Attach this to the Entity that generates the sound
	struct AudioSourceComponent {
		std::string filepath = "";      // The VFS path to reload the sound from JSON
		me::audio::SoundId clip{};
		float volume = 1.0f;
		float pitch = 1.0f;

		bool play_on_awake = false;     // Should it play automatically when the scene loads?
		bool has_played_awake = false;

		// 3D Spatial Settings
		bool spatial = true;
		float max_distance = 50.0f; // How far away before it becomes completely silent

		// The ECS System checks this every frame. If true, it plays the sound and resets it to false.
		bool trigger_play = false;
	};

	// Attach this to an Entity to stream long soundtracks
	struct BackgroundMusicComponent {
		std::string filepath = "";
		me::audio::MusicId stream{};
		float volume = 1.0f;

		bool loop = true;
		bool play_on_awake = false;
		bool has_played_awake = false;

		// State Triggers (Read by the Audio System)
		bool trigger_play = false;
		bool trigger_stop = false;
		bool trigger_pause = false;
		bool trigger_resume = false;
	};

}