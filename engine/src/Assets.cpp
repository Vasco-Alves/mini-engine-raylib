#include "Assets.hpp"
#include "Math.hpp"
#include "Registry.hpp"
#include "Components.hpp"

#include <raylib.h>

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstdint>
#include <utility>
#include <filesystem>


namespace fs = std::filesystem;

namespace me::assets {

	namespace {
		struct TexRecord {
			::Texture2D tex{};
			int refs = 0;
			std::uint32_t handle = 0;
		};

		// base folder (resolved like base + uri)
		std::string s_Base = "assets/";

		// URI -> record
		std::unordered_map<std::string, TexRecord> s_ByPath;

		// handle -> URI (for Release / lookup)
		std::unordered_map<std::uint32_t, std::string> s_HandleToPath;

		// monotonically increasing handles (0 = invalid)
		std::uint32_t s_NextHandle = 1;

		// helper: build full path
		static std::string FullPath(const char* uri) {
			if (!uri || !*uri) return {};
			if (!s_Base.empty())
				return s_Base + uri;
			return std::string(uri);
		}
	} // namespace

	// ---- internal access for Render2D/3D (not exposed in public headers) ----
	const ::Texture2D* ME_InternalGetTexture(TextureId id) {
		if (id.handle == 0) return nullptr;
		auto itPath = s_HandleToPath.find(id.handle);
		if (itPath == s_HandleToPath.end()) return nullptr;
		auto itRec = s_ByPath.find(itPath->second);
		if (itRec == s_ByPath.end()) return nullptr;
		return &itRec->second.tex;
	}

	const char* ME_InternalGetTexturePath(TextureId id) {
		if (id.handle == 0) return nullptr;
		auto itPath = s_HandleToPath.find(id.handle);
		if (itPath == s_HandleToPath.end()) return nullptr;
		return itPath->second.c_str();
	}

	// ---------------- public API ----------------
	void SetAssetRoot(const char* folder) {
		s_Base = (folder && *folder) ? folder : "";
		if (!s_Base.empty() && (s_Base.back() != '/' && s_Base.back() != '\\')) {
			s_Base.push_back('/');
		}

		// ensure the folder exists
		fs::path full = fs::current_path() / s_Base;
		fs::create_directories(full);
	}

	TextureId LoadTexture(const char* uri) {
		TextureId out{};
		if (!uri || !*uri) return out;

		const std::string key = uri;
		auto it = s_ByPath.find(key);
		if (it == s_ByPath.end()) {
			// first time: load from disk
			const std::string path = FullPath(uri);
			::Image img = LoadImage(path.c_str());
			if (img.data == nullptr) {
				// load failed
				return out;
			}
			::Texture2D tex = LoadTextureFromImage(img);
			UnloadImage(img);

			TexRecord rec{};
			rec.tex = tex;
			rec.refs = 1;
			rec.handle = s_NextHandle++;

			s_ByPath.emplace(key, rec);

			out.handle = rec.handle;
			s_HandleToPath[out.handle] = key;
		} else {
			// already loaded: bump refcount and return existing handle
			it->second.refs += 1;
			out.handle = it->second.handle;

			if (out.handle == 0) {
				// should not happen, but be safe
				out.handle = s_NextHandle++;
				it->second.handle = out.handle;
				s_HandleToPath[out.handle] = key;
			}
		}
		return out;
	}

	void Release(TextureId id) {
		if (id.handle == 0) return;
		auto itPath = s_HandleToPath.find(id.handle);
		if (itPath == s_HandleToPath.end()) return;

		const std::string& key = itPath->second;
		auto itRec = s_ByPath.find(key);
		if (itRec == s_ByPath.end()) {
			s_HandleToPath.erase(itPath);
			return;
		}

		itRec->second.refs -= 1;
		if (itRec->second.refs <= 0) {
			UnloadTexture(itRec->second.tex);
			s_ByPath.erase(itRec);
		}

		s_HandleToPath.erase(itPath);
	}

	// Unload every texture we still hold
	void ReleaseAll() {
		for (auto& kv : s_ByPath)
			UnloadTexture(kv.second.tex);

		s_ByPath.clear();
		s_HandleToPath.clear();
		s_NextHandle = 1;
	}

	void ReleaseUnused() {
		// [FIX] Disabled for 3D refactor because SpriteRenderer/SpriteSheet no longer exist.
		// If you implement a MeshRenderer that uses textures later, you can add that check here.

		/*
		std::unordered_set<std::string> inUse;
		auto& reg = me::detail::Reg();

		// Sweep anything not inUse
		for (auto it = s_ByPath.begin(); it != s_ByPath.end(); ) {
			if (inUse.find(it->first) == inUse.end()) {
				UnloadTexture(it->second.tex);
				it = s_ByPath.erase(it);
			} else {
				++it;
			}
		}

		// Rebuild handle map
		for (auto it = s_HandleToPath.begin(); it != s_HandleToPath.end(); ) {
			if (s_ByPath.find(it->second) == s_ByPath.end()) it = s_HandleToPath.erase(it);
			else ++it;
		}
		*/
	}

	bool IsTextureValid(TextureId id) {
		if (id.handle == 0) return false;
		auto itPath = s_HandleToPath.find(id.handle);
		if (itPath == s_HandleToPath.end()) return false;
		auto itRec = s_ByPath.find(itPath->second);
		return itRec != s_ByPath.end();
	}

	me::math::Vec2 TextureSize(TextureId id) {
		me::math::Vec2 sz{};
		auto* tex = ME_InternalGetTexture(id);
		if (!tex) return sz;
		sz.x = static_cast<float>(tex->width);
		sz.y = static_cast<float>(tex->height);
		return sz;
	}

} // namespace me::assets