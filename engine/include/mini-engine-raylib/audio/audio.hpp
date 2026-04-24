#pragma once

#include <cstdint>

namespace me::audio {

	// --------- Sound (short SFX) ----------
	struct SoundId { std::uint32_t handle = 0; };

	void     init();
	void     shutdown();

	SoundId  load(const char* uri);
	void     release(SoundId id);

	void     play(SoundId id, float volume = 1.0f, float pitch = 1.0f);
	void     set_master_volume(float v);    // 0..1

	// --------- Music (streamed, long tracks) ----------
	struct MusicId { std::uint32_t handle = 0; };

	MusicId  load_music(const char* uri);
	void     release(MusicId id);

	void     play_music(MusicId id, bool loop = true, float volume = 1.0f);
	void     stop_music(MusicId id);
	void     pause_music(MusicId id);
	void     resume_music(MusicId id);
	void     set_music_volume(MusicId id, float volume); // 0..1

	// Must be called once per frame if any music is playing.
	void     update();
}