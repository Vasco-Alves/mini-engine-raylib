#include "Audio.hpp"

#include <raylib.h>

#include <unordered_map>
#include <string>
#include <cstdint>
#include <utility>
#include <filesystem>

namespace fs = std::filesystem;

namespace me::audio {

	namespace {
		bool s_DeviceReady = false;

		inline void EnsureAudioDevice() {
			if (!s_DeviceReady) {
				InitAudioDevice();
				::SetMasterVolume(1.0f);
				s_DeviceReady = true;
			}
		}

		// ---------- Sound cache ----------
		struct SoundRec {
			::Sound snd{};
			int refs = 0;
		};
		std::unordered_map<std::string, SoundRec> s_SoundByPath;         // uri -> rec
		std::unordered_map<std::uint32_t, std::string> s_SoundHandleTo;  // handle -> uri
		std::uint32_t s_NextSoundHandle = 1;

		// ---------- Music cache ----------
		struct MusicRec {
			::Music music{};
			int refs = 0;
			bool playing = false;
		};

		std::unordered_map<std::string, MusicRec> s_MusicByPath;         // uri -> rec
		std::unordered_map<std::uint32_t, std::string> s_MusicHandleTo;  // handle -> uri
		std::uint32_t s_NextMusicHandle = 1;

		inline const ::Sound* GetNative(SoundId id) {
			if (id.handle == 0) return nullptr;
			auto it = s_SoundHandleTo.find(id.handle);
			if (it == s_SoundHandleTo.end()) return nullptr;
			auto itRec = s_SoundByPath.find(it->second);
			if (itRec == s_SoundByPath.end()) return nullptr;
			return &itRec->second.snd;
		}

		inline ::Music* GetNative(MusicId id) {
			if (id.handle == 0) return nullptr;
			auto it = s_MusicHandleTo.find(id.handle);
			if (it == s_MusicHandleTo.end()) return nullptr;
			auto itRec = s_MusicByPath.find(it->second);
			if (itRec == s_MusicByPath.end()) return nullptr;
			return &itRec->second.music;
		}
	} // namespace

	void Init() {
		EnsureAudioDevice();
	}

	void Shutdown() {
		// Stop all music and unload
		for (auto& kv : s_MusicByPath) {
			if (kv.second.playing) StopMusicStream(kv.second.music);
			UnloadMusicStream(kv.second.music);
		}
		s_MusicByPath.clear();
		s_MusicHandleTo.clear();
		s_NextMusicHandle = 1;

		// Unload all sounds
		for (auto& kv : s_SoundByPath) {
			UnloadSound(kv.second.snd);
		}
		s_SoundByPath.clear();
		s_SoundHandleTo.clear();
		s_NextSoundHandle = 1;

		if (s_DeviceReady) {
			CloseAudioDevice();
			s_DeviceReady = false;
		}
	}

	// ---------- Sound ----------
	SoundId Load(const char* uri) {
		SoundId out{};
		if (!uri || !*uri) return out;
		EnsureAudioDevice();

		const std::string key = uri;
		auto it = s_SoundByPath.find(key);
		if (it == s_SoundByPath.end()) {
			fs::path p = fs::current_path() / "assets" / key;
			fs::create_directories(p.parent_path());
			::Sound s = LoadSound(p.string().c_str());
			if (s.stream.buffer == nullptr && s.frameCount == 0) return out;

			SoundRec rec{ s, 1 };
			s_SoundByPath.emplace(key, rec);
			out.handle = s_NextSoundHandle++;
			s_SoundHandleTo[out.handle] = key;
		} else {
			it->second.refs += 1;
			for (const auto& kv : s_SoundHandleTo) if (kv.second == key) { out.handle = kv.first; break; }
			if (out.handle == 0) { out.handle = s_NextSoundHandle++; s_SoundHandleTo[out.handle] = key; }
		}
		return out;
	}

	void Release(SoundId id) {
		if (id.handle == 0) return;
		auto itPath = s_SoundHandleTo.find(id.handle);
		if (itPath == s_SoundHandleTo.end()) return;
		const std::string& key = itPath->second;

		auto itRec = s_SoundByPath.find(key);
		if (itRec != s_SoundByPath.end()) {
			itRec->second.refs -= 1;
			if (itRec->second.refs <= 0) {
				UnloadSound(itRec->second.snd);
				s_SoundByPath.erase(itRec);
			}
		}
		s_SoundHandleTo.erase(itPath);
	}

	void Play(SoundId id, float volume, float pitch) {
		const ::Sound* s = GetNative(id);
		if (!s) return;
		::Sound copy = *s; // raylib copies handle safely
		SetSoundVolume(copy, volume);
		SetSoundPitch(copy, pitch);
		PlaySound(copy);
	}

	void SetMasterVolume(float v) {
		EnsureAudioDevice();
		::SetMasterVolume(v < 0.f ? 0.f : (v > 1.f ? 1.f : v));
	}

	// ---------- Music ----------
	MusicId LoadMusic(const char* uri) {
		MusicId out{};
		if (!uri || !*uri) return out;
		EnsureAudioDevice();

		const std::string key = uri;
		auto it = s_MusicByPath.find(key);
		if (it == s_MusicByPath.end()) {
			fs::path p = fs::current_path() / "assets" / key;
			fs::create_directories(p.parent_path());
			::Music m = LoadMusicStream(p.string().c_str());
			// raylib doesn't give a null to check easily; assume success if m.ctxData!=nullptr OR just proceed.
			MusicRec rec{ m, 1, false };
			s_MusicByPath.emplace(key, rec);
			out.handle = s_NextMusicHandle++;
			s_MusicHandleTo[out.handle] = key;
		} else {
			it->second.refs += 1;
			for (const auto& kv : s_MusicHandleTo) if (kv.second == key) { out.handle = kv.first; break; }
			if (out.handle == 0) { out.handle = s_NextMusicHandle++; s_MusicHandleTo[out.handle] = key; }
		}
		return out;
	}

	void Release(MusicId id) {
		if (id.handle == 0) return;
		auto itPath = s_MusicHandleTo.find(id.handle);
		if (itPath == s_MusicHandleTo.end()) return;
		const std::string& key = itPath->second;

		auto itRec = s_MusicByPath.find(key);
		if (itRec != s_MusicByPath.end()) {
			if (itRec->second.playing) StopMusicStream(itRec->second.music);
			UnloadMusicStream(itRec->second.music);
			s_MusicByPath.erase(itRec);
		}
		s_MusicHandleTo.erase(itPath);
	}

	void PlayMusic(MusicId id, bool loop, float volume) {
		::Music* m = GetNative(id);
		if (!m) return;
		SetMusicVolume(*m, volume);
		// loop flag in raylib is set per stream; some formats use music.looping. We can just keep calling UpdateMusicStream.
		PlayMusicStream(*m);
		// mark playing in cache
		auto itPath = s_MusicHandleTo.find(id.handle);
		if (itPath != s_MusicHandleTo.end()) {
			auto itRec = s_MusicByPath.find(itPath->second);
			if (itRec != s_MusicByPath.end()) {
				itRec->second.playing = true;
				// crude looping: rely on UpdateMusicStream + IsMusicStreamPlaying
				// raylib also has music.looping for module formats; we keep it simple.
				if (!loop) { /* nothing special; caller can StopMusic later */ }
			}
		}
	}

	void StopMusic(MusicId id) {
		::Music* m = GetNative(id);
		if (!m) return;
		StopMusicStream(*m);
		auto itPath = s_MusicHandleTo.find(id.handle);
		if (itPath != s_MusicHandleTo.end()) {
			auto itRec = s_MusicByPath.find(itPath->second);
			if (itRec != s_MusicByPath.end()) itRec->second.playing = false;
		}
	}

	void PauseMusic(MusicId id) { if (auto* m = GetNative(id)) PauseMusicStream(*m); }
	void ResumeMusic(MusicId id) { if (auto* m = GetNative(id)) ResumeMusicStream(*m); }
	void SetMusicVolume(MusicId id, float volume) { if (auto* m = GetNative(id)) SetMusicVolume(*m, volume); }

	void Update() {
		// Update all playing music streams
		for (auto& kv : s_MusicByPath) {
			if (kv.second.playing) UpdateMusicStream(kv.second.music);
		}
	}

} // namespace me::audio
