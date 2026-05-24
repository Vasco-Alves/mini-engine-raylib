#include "mini-engine-raylib/assets/assets.hpp"

#include "mini-engine-raylib/core/vfs.hpp"
#include "mini-engine-raylib/core/engine.hpp" 
#include "mini-engine-raylib/ecs/components.hpp"

#include <unordered_map>
#include <string>
#include <cstdint>
#include <filesystem>
#include <raylib.h>

namespace fs = std::filesystem;

namespace me::assets {

	namespace {
		// ====================================================================
		// FNV-1a HASH ALGORITHM
		// Crushes a file path string into a unique 32-bit integer.
		// ====================================================================
		constexpr std::uint32_t hash_path(const char* str) {
			std::uint32_t hash = 2166136261u;
			while (*str) {
				hash ^= static_cast<std::uint32_t>(*str++);
				hash *= 16777619u;
			}
			return hash;
		}

		// ====================================================================
		// CACHE DATA STRUCTURES
		// ====================================================================

		// --- Texture Cache ---
		struct TexRecord {
			::Texture2D tex{};
			int refs = 0;
		};

		// Notice: The key is now a blazing fast uint32_t integer!
		std::unordered_map<std::uint32_t, TexRecord> s_textures;
		std::unordered_map<std::uint32_t, std::string> s_texture_paths; // Kept only so we can save paths to JSON

		// --- Model Cache ---
		struct ModelRec {
			::Model model{};
			int refs = 0;
		};

		std::unordered_map<std::uint32_t, ModelRec> s_models;
		std::unordered_map<std::uint32_t, std::string> s_model_paths;
	}

	// ====================================================================
	// GLOBAL CONFIGURATION
	// ====================================================================

	void release_all() {
		// Clean up Textures
		for (auto& kv : s_textures) {
			UnloadTexture(kv.second.tex);
		}
		s_textures.clear();
		s_texture_paths.clear();

		// Clean up Models
		for (auto& kv : s_models) {
			UnloadModel(kv.second.model);
		}
		s_models.clear();
		s_model_paths.clear();
	}

	// ====================================================================
	// TEXTURES
	// ====================================================================

	const ::Texture2D* internal_get_texture(TextureId id) {
		if (id.handle == 0) return nullptr;
		auto it = s_textures.find(id.handle);
		if (it == s_textures.end()) return nullptr;
		return &it->second.tex;
	}

	const char* internal_get_texture_path(TextureId id) {
		if (id.handle == 0) return nullptr;
		auto it = s_texture_paths.find(id.handle);
		if (it == s_texture_paths.end()) return nullptr;
		return it->second.c_str();
	}

	TextureId load_texture(const char* uri) {
		if (!uri || !*uri) return TextureId{ 0 };

		// 1. Hash the string instantly
		std::uint32_t handle = hash_path(uri);
		auto it = s_textures.find(handle);

		// 2. Load from disk if it doesn't exist
		if (it == s_textures.end()) {
			const std::string path = me::vfs::resolve(uri);
			::Image img = LoadImage(path.c_str());
			if (img.data == nullptr) {
				return TextureId{ 0 };
			}
			::Texture2D tex = LoadTextureFromImage(img);
			UnloadImage(img);

			s_textures[handle] = TexRecord{ tex, 1 };
			s_texture_paths[handle] = uri; // Save the string for JSON serialization
		}
		// 3. Just increment the ref count!
		else {
			it->second.refs += 1;
		}

		return TextureId{ handle };
	}

	void release(TextureId id) {
		if (id.handle == 0) return;
		auto it = s_textures.find(id.handle);
		if (it == s_textures.end()) return;

		it->second.refs -= 1;
		if (it->second.refs <= 0) {
			UnloadTexture(it->second.tex);
			s_textures.erase(it);
			s_texture_paths.erase(id.handle);
		}
	}

	bool is_texture_valid(TextureId id) {
		return s_textures.find(id.handle) != s_textures.end();
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
		auto it = s_models.find(id.handle);
		if (it == s_models.end()) return nullptr;
		return &it->second.model;
	}

	const char* internal_get_model_path(ModelId id) {
		if (id.handle == 0) return nullptr;
		auto it = s_model_paths.find(id.handle);
		if (it == s_model_paths.end()) return nullptr;
		return it->second.c_str();
	}

	ModelId load_model(const char* uri) {
		if (!uri || !*uri) return ModelId{ 0 };

		// 1. Hash the string instantly
		std::uint32_t handle = hash_path(uri);
		auto it = s_models.find(handle);

		// 2. Load from disk if it doesn't exist
		if (it == s_models.end()) {
			const std::string path = me::vfs::resolve(uri);
			::Model mod = LoadModel(path.c_str());

			// Raylib leaves mesh count at 0 if the load fails
			if (mod.meshCount == 0) return ModelId{ 0 };

			s_models[handle] = ModelRec{ mod, 1 };
			s_model_paths[handle] = uri;
		}
		// 3. Just increase the reference count!
		else {
			it->second.refs += 1;
		}

		return ModelId{ handle };
	}

	void release(ModelId id) {
		if (id.handle == 0) return;
		auto it = s_models.find(id.handle);
		if (it == s_models.end()) return;

		it->second.refs -= 1;
		if (it->second.refs <= 0) {
			UnloadModel(it->second.model);
			s_models.erase(it);
			s_model_paths.erase(id.handle);
		}
	}

	bool is_model_valid(ModelId id) {
		return s_models.find(id.handle) != s_models.end();
	}

} // namespace me::assets
