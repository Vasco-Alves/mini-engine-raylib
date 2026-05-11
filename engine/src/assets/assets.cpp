#include "mini-engine-raylib/assets/assets.hpp"

#include "mini-engine-raylib/core/vfs.hpp"
#include "mini-engine-raylib/core/engine.hpp" 
#include "mini-engine-raylib/ecs/components.hpp"

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstdint>
#include <utility>
#include <filesystem>

#include <raylib.h>

namespace fs = std::filesystem;

namespace me::assets {

	namespace {
		// ====================================================================
		// CACHE DATA STRUCTURES
		// ====================================================================

		// --- Texture Cache ---
		struct TexRecord {
			::Texture2D tex{};
			int refs = 0;
			std::uint32_t handle = 0;
		};

		std::unordered_map<std::string, TexRecord> s_by_path;
		std::unordered_map<std::uint32_t, std::string> s_handle_to_path;
		std::uint32_t s_next_handle = 1;

		// --- Model Cache ---
		struct ModelRec {
			::Model model{};
			int refs = 0;
			std::uint32_t handle = 0;
		};

		std::unordered_map<std::string, ModelRec> s_model_by_path;
		std::unordered_map<std::uint32_t, std::string> s_model_handle_to;
		std::uint32_t s_next_model_handle = 1;
	}

	// ====================================================================
	// GLOBAL CONFIGURATION
	// ====================================================================

	void release_all() {
		// Clean up Textures
		for (auto& kv : s_by_path) {
			UnloadTexture(kv.second.tex);
		}
		s_by_path.clear();
		s_handle_to_path.clear();
		s_next_handle = 1;

		// Clean up Models
		for (auto& kv : s_model_by_path) {
			UnloadModel(kv.second.model);
		}
		s_model_by_path.clear();
		s_model_handle_to.clear();
		s_next_model_handle = 1;
	}

	// ====================================================================
	// TEXTURES
	// ====================================================================

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

	TextureId load_texture(const char* uri) {
		TextureId out{};
		if (!uri || !*uri) return out;

		const std::string key = uri;
		auto it = s_by_path.find(key);
		if (it == s_by_path.end()) {
			const std::string path = me::vfs::resolve(uri);
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

	bool is_texture_valid(TextureId id) {
		if (id.handle == 0) return false;
		auto itPath = s_handle_to_path.find(id.handle);
		if (itPath == s_handle_to_path.end()) return false;
		auto itRec = s_by_path.find(itPath->second);
		return itRec != s_by_path.end();
	}

	Vector2 texture_size(TextureId id) {
		Vector2 sz{};
		auto* tex = internal_get_texture(id);
		if (!tex) return sz;
		sz.x = static_cast<float>(tex->width);
		sz.y = static_cast<float>(tex->height);
		return sz;
	}

	// ====================================================================
	// 3D MODELS
	// ====================================================================

	const ::Model* internal_get_model(ModelId id) {
		if (id.handle == 0) return nullptr;
		auto itPath = s_model_handle_to.find(id.handle);
		if (itPath == s_model_handle_to.end()) return nullptr;
		auto itRec = s_model_by_path.find(itPath->second);
		if (itRec == s_model_by_path.end()) return nullptr;
		return &itRec->second.model;
	}

	const char* internal_get_model_path(ModelId id) {
		if (id.handle == 0) return nullptr;
		auto itPath = s_model_handle_to.find(id.handle);
		if (itPath == s_model_handle_to.end()) return nullptr;
		return itPath->second.c_str();
	}

	ModelId load_model(const char* uri) {
		ModelId out{};
		if (!uri || !*uri) return out;

		const std::string key = uri;
		auto it = s_model_by_path.find(key);

		// 1. If it's not loaded, load it from disk!
		if (it == s_model_by_path.end()) {
			const std::string path = me::vfs::resolve(uri);
			::Model mod = LoadModel(path.c_str());

			// Raylib leaves mesh count at 0 if the load fails
			if (mod.meshCount == 0) return out;

			ModelRec rec{};
			rec.model = mod;
			rec.refs = 1;
			rec.handle = s_next_model_handle++;

			s_model_by_path.emplace(key, rec);
			out.handle = rec.handle;
			s_model_handle_to[out.handle] = key;
		}
		// 2. If it is already loaded, just increase the reference count!
		else {
			it->second.refs += 1;
			out.handle = it->second.handle;
		}
		return out;
	}

	void release(ModelId id) {
		if (id.handle == 0) return;
		auto itPath = s_model_handle_to.find(id.handle);
		if (itPath == s_model_handle_to.end()) return;

		const std::string& key = itPath->second;
		auto itRec = s_model_by_path.find(key);
		if (itRec == s_model_by_path.end()) return;

		itRec->second.refs -= 1;
		if (itRec->second.refs <= 0) {
			UnloadModel(itRec->second.model);
			s_model_by_path.erase(itRec);
		}
		s_model_handle_to.erase(itPath);
	}

	bool is_model_valid(ModelId id) {
		return internal_get_model(id) != nullptr;
	}

} // namespace me::assets