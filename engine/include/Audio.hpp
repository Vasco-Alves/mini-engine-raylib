#pragma once

#include <cstdint>

namespace me::audio {

	// --------- Sound (short SFX) ----------
	struct SoundId { std::uint32_t handle = 0; };

	void     Init();    // optional (lazy-inits if you forget)
	void     Shutdown();// optional (tidy logs)

	SoundId  Load(const char* uri);       // from assets/
	void     Release(SoundId id);

	void     Play(SoundId id, float volume = 1.0f, float pitch = 1.0f);
	void     SetMasterVolume(float v);    // 0..1

	// --------- Music (streamed, long tracks) ----------
	struct MusicId { std::uint32_t handle = 0; };

	MusicId  LoadMusic(const char* uri);  // from assets/
	void     Release(MusicId id);

	void     PlayMusic(MusicId id, bool loop = true, float volume = 1.0f);
	void     StopMusic(MusicId id);
	void     PauseMusic(MusicId id);
	void     ResumeMusic(MusicId id);
	void     SetMusicVolume(MusicId id, float volume); // 0..1

	// Must be called once per frame if any music is playing.
	void     Update();
}
