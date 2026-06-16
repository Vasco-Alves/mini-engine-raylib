#include "mini-engine-raylib/systems/audio_system.hpp"
#include "mini-engine-raylib/ecs/components.hpp"
#include "mini-engine-raylib/ecs/audio_components.hpp"
#include "mini-engine-raylib/audio/audio.hpp"
#include "mini-engine-raylib/core/engine.hpp"
#include <raymath.h>
#include <algorithm>

namespace me::systems {

	void audio_update(me::Registry& registry, Vector3 fallback_pos, Vector3 fallback_forward, Vector3 fallback_up) {

		// 1. Setup default listener using the Fallback (Editor Camera)
		Vector3 listener_pos = fallback_pos;
		Vector3 listener_right = Vector3Normalize(Vector3CrossProduct(fallback_forward, fallback_up));

		// 2. Check the ECS: Is there an active Game Camera overriding it?
		me::entity::entity_id listener_id = me::entity::null;

		for (auto [e, listener] : registry.view<me::components::AudioListenerComponent>()) {
			if (listener.active) {
				listener_id = e;
				break;
			}
		}

		if (listener_id != me::entity::null) {
			auto* t = registry.try_get_component<me::components::TransformComponent>(listener_id);
			auto* c = registry.try_get_component<me::components::CameraComponent>(listener_id);

			if (t && c) {
				// Override with ECS Game Camera
				listener_pos = {
					t->model_matrix.m12,
					t->model_matrix.m13,
					t->model_matrix.m14
				};
				Vector3 forward = Vector3Normalize(Vector3Subtract(c->target, listener_pos));
				listener_right = Vector3Normalize(Vector3CrossProduct(forward, c->up));
			}
		}

		// 3. Loop through all Audio Sources and check for Triggers.
		// TransformComponent is only needed for spatial sources, so it stays an
		// optional try_get rather than a required component of the view.
		for (auto [e, source] : registry.view<me::components::AudioSourceComponent>()) {

			// Handle Play On Awake (ONLY if the game is actively playing)
			if (me::is_playing() && source.play_on_awake && !source.has_played_awake) {
				source.trigger_play = true;
				source.has_played_awake = true; // Ensure it only fires once
			}

			// Did a Lua script or C++ system request a sound to play this frame?
			if (source.trigger_play) {
				source.trigger_play = false; // Immediately reset the trigger

				if (source.clip.handle == 0) continue; // Safety check for empty audio

				float final_volume = source.volume;
				float final_pan = 0.5f; // Default center

				// 4. Apply Spatial 3D Math using whichever listener_pos/right won
				if (source.spatial) {
					auto* source_transform = registry.try_get_component<me::components::TransformComponent>(e);

					if (source_transform) {
						Vector3 source_pos = {
							source_transform->model_matrix.m12,
							source_transform->model_matrix.m13,
							source_transform->model_matrix.m14
						};

						// Calculate Distance Falloff (Linear fade out)
						float distance = Vector3Distance(listener_pos, source_pos);
						float attenuation = 1.0f - std::clamp(distance / source.max_distance, 0.0f, 1.0f);
						final_volume *= attenuation;

						// Calculate Panning (Left vs Right speaker)
						// Dot product between Camera's Right vector and the direction to the sound
						if (distance > 0.01f) {
							Vector3 dir_to_sound = Vector3Normalize(Vector3Subtract(source_pos, listener_pos));
							float dot = Vector3DotProduct(listener_right, dir_to_sound);

							final_pan = (-dot + 1.0f) * 0.5f;
						}
					}
				}

				// 5. Send the calculated math to the Sound Card
				if (final_volume > 0.001f) {
					me::audio::play(source.clip, final_volume, source.pitch, final_pan);
				}
			}
		}

		// 4. Loop through all Music Sources
		for (auto [e, music] : registry.view<me::components::BackgroundMusicComponent>()) {
			(void)e;

			// Handle Play On Awake (only if the game is actively playing)
			if (me::is_playing() && music.play_on_awake && !music.has_played_awake) {
				music.trigger_play = true;
				music.has_played_awake = true;
			}

			if (music.stream.handle == 0) continue; // Safety check

			// Handle state triggers
			if (music.trigger_play) {
				music.trigger_play = false;
				me::audio::play_music(music.stream, music.loop, music.volume);
			}
			if (music.trigger_stop) {
				music.trigger_stop = false;
				me::audio::stop_music(music.stream);
			}
			if (music.trigger_pause) {
				music.trigger_pause = false;
				me::audio::pause_music(music.stream);
			}
			if (music.trigger_resume) {
				music.trigger_resume = false;
				me::audio::resume_music(music.stream);
			}

			// Dynamically sync volume in case the Inspector slider is dragged
			me::audio::set_music_volume(music.stream, music.volume);
		}
	}

} // namespace me::systems
