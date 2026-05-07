#include "mini-engine-raylib/audio/audio.hpp"

#include <raylib.h>

#include <unordered_map>
#include <string>
#include <cstdint>
#include <utility>
#include <filesystem>

namespace fs = std::filesystem;

namespace me::audio {

	namespace {
		bool s_device_ready = false;

		inline void ensure_audio_device() {
			if (!s_device_ready) {
				InitAudioDevice();
				::SetMasterVolume(1.0f);
				s_device_ready = true;
			}
		}

		// ---------- Sound cache ----------
		struct SoundRec {
			::Sound snd{};
			int refs = 0;
		};

		std::unordered_map<std::string, SoundRec> s_sound_by_path;
		std::unordered_map<std::uint32_t, std::string> s_sound_handle_to;
		std::uint32_t s_next_sound_handle = 1;

		// ---------- Music cache ----------
		struct MusicRec {
			::Music music{};
			int refs = 0;
			bool playing = false;
		};

		std::unordered_map<std::string, MusicRec> s_music_by_path;
		std::unordered_map<std::uint32_t, std::string> s_music_handle_to;
		std::uint32_t s_next_music_handle = 1;

		inline const ::Sound* get_native(SoundId id) {
			if (id.handle == 0) return nullptr;
			auto it = s_sound_handle_to.find(id.handle);
			if (it == s_sound_handle_to.end()) return nullptr;
			auto itRec = s_sound_by_path.find(it->second);
			if (itRec == s_sound_by_path.end()) return nullptr;
			return &itRec->second.snd;
		}

		inline ::Music* get_native(MusicId id) {
			if (id.handle == 0) return nullptr;
			auto it = s_music_handle_to.find(id.handle);
			if (it == s_music_handle_to.end()) return nullptr;
			auto itRec = s_music_by_path.find(it->second);
			if (itRec == s_music_by_path.end()) return nullptr;
			return &itRec->second.music;
		}
	} // namespace

	void init() {
		ensure_audio_device();
	}

	void shutdown() {
		// Stop all music and unload
		for (auto& kv : s_music_by_path) {
			if (kv.second.playing) StopMusicStream(kv.second.music);
			UnloadMusicStream(kv.second.music);
		}

		s_music_by_path.clear();
		s_music_handle_to.clear();
		s_next_music_handle = 1;

		// Unload all sounds
		for (auto& kv : s_sound_by_path) {
			UnloadSound(kv.second.snd);
		}
		s_sound_by_path.clear();
		s_sound_handle_to.clear();
		s_next_sound_handle = 1;

		if (s_device_ready) {
			CloseAudioDevice();
			s_device_ready = false;
		}
	}

	// ---------- Sound ----------
	SoundId load(const char* uri) {
		SoundId out{};
		if (!uri || !*uri) return out;
		ensure_audio_device();

		const std::string key = uri;
		auto it = s_sound_by_path.find(key);
		if (it == s_sound_by_path.end()) {
			fs::path p = fs::current_path() / "assets" / key;
			fs::create_directories(p.parent_path());
			::Sound s = LoadSound(p.string().c_str());
			if (s.stream.buffer == nullptr && s.frameCount == 0) return out;

			SoundRec rec{ s, 1 };
			s_sound_by_path.emplace(key, rec);
			out.handle = s_next_sound_handle++;
			s_sound_handle_to[out.handle] = key;
		} else {
			it->second.refs += 1;
			for (const auto& kv : s_sound_handle_to) if (kv.second == key) { out.handle = kv.first; break; }
			if (out.handle == 0) { out.handle = s_next_sound_handle++; s_sound_handle_to[out.handle] = key; }
		}
		return out;
	}

	void release(SoundId id) {
		if (id.handle == 0) return;
		auto itPath = s_sound_handle_to.find(id.handle);
		if (itPath == s_sound_handle_to.end()) return;
		const std::string& key = itPath->second;

		auto itRec = s_sound_by_path.find(key);
		if (itRec != s_sound_by_path.end()) {
			itRec->second.refs -= 1;
			if (itRec->second.refs <= 0) {
				UnloadSound(itRec->second.snd);
				s_sound_by_path.erase(itRec);
			}
		}
		s_sound_handle_to.erase(itPath);
	}

	void play(SoundId id, float volume, float pitch) {
		const ::Sound* s = get_native(id);
		if (!s) return;
		::Sound copy = *s;
		SetSoundVolume(copy, volume);
		SetSoundPitch(copy, pitch);
		PlaySound(copy);
	}

	void stop(SoundId id) {
		const ::Sound* s = get_native(id);
		if (!s) return;
		StopSound(*s);
	}

	void set_master_volume(float v) {
		ensure_audio_device();
		::SetMasterVolume(v < 0.f ? 0.f : (v > 1.f ? 1.f : v));
	}

	// ---------- Music ----------
	MusicId load_music(const char* uri) {
		MusicId out{};
		if (!uri || !*uri) return out;
		ensure_audio_device();

		const std::string key = uri;
		auto it = s_music_by_path.find(key);
		if (it == s_music_by_path.end()) {
			fs::path p = fs::current_path() / "assets" / key;
			fs::create_directories(p.parent_path());
			::Music m = LoadMusicStream(p.string().c_str());
			MusicRec rec{ m, 1, false };
			s_music_by_path.emplace(key, rec);
			out.handle = s_next_music_handle++;
			s_music_handle_to[out.handle] = key;
		} else {
			it->second.refs += 1;
			for (const auto& kv : s_music_handle_to) if (kv.second == key) { out.handle = kv.first; break; }
			if (out.handle == 0) { out.handle = s_next_music_handle++; s_music_handle_to[out.handle] = key; }
		}
		return out;
	}

	void release(MusicId id) {
		if (id.handle == 0) return;
		auto itPath = s_music_handle_to.find(id.handle);
		if (itPath == s_music_handle_to.end()) return;
		const std::string& key = itPath->second;

		auto itRec = s_music_by_path.find(key);
		if (itRec != s_music_by_path.end()) {
			if (itRec->second.playing) StopMusicStream(itRec->second.music);
			UnloadMusicStream(itRec->second.music);
			s_music_by_path.erase(itRec);
		}
		s_music_handle_to.erase(itPath);
	}

	void play_music(MusicId id, bool loop, float volume) {
		::Music* m = get_native(id);
		if (!m) return;
		SetMusicVolume(*m, volume);
		PlayMusicStream(*m);

		auto itPath = s_music_handle_to.find(id.handle);
		if (itPath != s_music_handle_to.end()) {
			auto itRec = s_music_by_path.find(itPath->second);
			if (itRec != s_music_by_path.end()) {
				itRec->second.playing = true;
			}
		}
	}

	void stop_music(MusicId id) {
		::Music* m = get_native(id);
		if (!m) return;
		StopMusicStream(*m);
		auto itPath = s_music_handle_to.find(id.handle);
		if (itPath != s_music_handle_to.end()) {
			auto itRec = s_music_by_path.find(itPath->second);
			if (itRec != s_music_by_path.end()) itRec->second.playing = false;
		}
	}

	void pause_music(MusicId id) { if (auto* m = get_native(id)) PauseMusicStream(*m); }
	void resume_music(MusicId id) { if (auto* m = get_native(id)) ResumeMusicStream(*m); }
	void set_music_volume(MusicId id, float volume) { if (auto* m = get_native(id)) SetMusicVolume(*m, volume); }

	void update() {
		for (auto& kv : s_music_by_path) {
			if (kv.second.playing) UpdateMusicStream(kv.second.music);
		}
	}

} // namespace me::audio