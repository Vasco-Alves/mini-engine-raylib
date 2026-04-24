#include "mini-engine-raylib/assets/assets.hpp"
#include "mini-engine-raylib/core/math.hpp"
#include "mini-engine-raylib/core/engine.hpp" 
#include "mini-engine-raylib/ecs/components.hpp"

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

		std::string s_base = "assets/";

		std::unordered_map<std::string, TexRecord> s_by_path;
		std::unordered_map<std::uint32_t, std::string> s_handle_to_path;
		std::uint32_t s_next_handle = 1;

		static std::string full_path(const char* uri) {
			if (!uri || !*uri) return {};
			if (!s_base.empty())
				return s_base + uri;
			return std::string(uri);
		}
	} // namespace

	// ---- internal access for Render2D/3D ----
	const ::Texture2D* internal_get_texture(TextureId id) {
		if (id.handle == 0) return nullptr;
		auto itPath = s_handle_to_path.find(id.handle);
		if (itPath == s_handle_to_path.end()) return nullptr;
		auto itRec = s_by_path.find(itPath->second);
		if (itRec == s_by_path.end()) return nullptr;
		return &itRec->second.tex;
	}

	const char* internal_get_texture_path(TextureId id) {
		if (id.handle == 0) return nullptr;
		auto itPath = s_handle_to_path.find(id.handle);
		if (itPath == s_handle_to_path.end()) return nullptr;
		return itPath->second.c_str();
	}

	// ---------------- public API ----------------
	void set_asset_root(const char* folder) {
		s_base = (folder && *folder) ? folder : "";
		if (!s_base.empty() && (s_base.back() != '/' && s_base.back() != '\\')) {
			s_base.push_back('/');
		}

		fs::path full = fs::current_path() / s_base;
		fs::create_directories(full);
	}

	TextureId load_texture(const char* uri) {
		TextureId out{};
		if (!uri || !*uri) return out;

		const std::string key = uri;
		auto it = s_by_path.find(key);
		if (it == s_by_path.end()) {
			const std::string path = full_path(uri);
			::Image img = LoadImage(path.c_str());
			if (img.data == nullptr) {
				return out;
			}
			::Texture2D tex = LoadTextureFromImage(img);
			UnloadImage(img);

			TexRecord rec{};
			rec.tex = tex;
			rec.refs = 1;
			rec.handle = s_next_handle++;

			s_by_path.emplace(key, rec);

			out.handle = rec.handle;
			s_handle_to_path[out.handle] = key;
		} else {
			it->second.refs += 1;
			out.handle = it->second.handle;

			if (out.handle == 0) {
				out.handle = s_next_handle++;
				it->second.handle = out.handle;
				s_handle_to_path[out.handle] = key;
			}
		}
		return out;
	}

	void release(TextureId id) {
		if (id.handle == 0) return;
		auto itPath = s_handle_to_path.find(id.handle);
		if (itPath == s_handle_to_path.end()) return;

		const std::string& key = itPath->second;
		auto itRec = s_by_path.find(key);
		if (itRec == s_by_path.end()) {
			s_handle_to_path.erase(itPath);
			return;
		}

		itRec->second.refs -= 1;
		if (itRec->second.refs <= 0) {
			UnloadTexture(itRec->second.tex);
			s_by_path.erase(itRec);
		}

		s_handle_to_path.erase(itPath);
	}

	void release_all() {
		for (auto& kv : s_by_path)
			UnloadTexture(kv.second.tex);

		s_by_path.clear();
		s_handle_to_path.clear();
		s_next_handle = 1;
	}

	void release_unused() {}

	bool is_texture_valid(TextureId id) {
		if (id.handle == 0) return false;
		auto itPath = s_handle_to_path.find(id.handle);
		if (itPath == s_handle_to_path.end()) return false;
		auto itRec = s_by_path.find(itPath->second);
		return itRec != s_by_path.end();
	}

	me::math::Vec2 texture_size(TextureId id) {
		me::math::Vec2 sz{};
		auto* tex = internal_get_texture(id);
		if (!tex) return sz;
		sz.x = static_cast<float>(tex->width);
		sz.y = static_cast<float>(tex->height);
		return sz;
	}

} // namespace me::assets