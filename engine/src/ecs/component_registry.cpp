#include "mini-engine-raylib/ecs/component_registry.hpp"

#include <mini-ecs/registry.hpp>

#include "mini-engine-raylib/ecs/components.hpp"
#include "mini-engine-raylib/ecs/physics_components.hpp"
#include "mini-engine-raylib/ecs/audio_components.hpp"
#include "mini-engine-raylib/ecs/script_component.hpp"
#include "mini-engine-raylib/audio/audio.hpp"
#include "mini-engine-raylib/assets/assets.hpp"
#include "../assets/assets_internal.hpp"

#include <cstdint>

namespace me::ecs {

	namespace {

		// --- me::Color <-> flat color_r/g/b[/a] keys (the on-disk scene format) ---
		void write_color(json& j, const me::Color& c, bool with_alpha = false) {
			j["color_r"] = c.r;
			j["color_g"] = c.g;
			j["color_b"] = c.b;
			if (with_alpha) j["color_a"] = c.a;
		}

		me::Color read_color(const json& j, bool with_alpha = false) {
			return me::Color{
				static_cast<unsigned char>(j.value("color_r", 255)),
				static_cast<unsigned char>(j.value("color_g", 255)),
				static_cast<unsigned char>(j.value("color_b", 255)),
				with_alpha ? static_cast<unsigned char>(j.value("color_a", 255)) : static_cast<unsigned char>(255)
			};
		}

		// Builds a ComponentMeta for the common case (generic has/clone) from a
		// save function `json(const T&)` and a load function `void(T&, const json&)`.
		template <typename T, typename SaveFn, typename LoadFn>
		ComponentMeta meta(const char* name, SaveFn save_fn, LoadFn load_fn, std::function<void(Registry&, entity::entity_id)> on_destroy = nullptr) {
			ComponentMeta m;
			m.name = name;
			m.has = [](Registry& r, entity::entity_id e) { return r.has_component<T>(e); };
			m.save = [save_fn](Registry& r, entity::entity_id e) -> json {
				return save_fn(*r.try_get_component<T>(e));
				};
			m.load = [load_fn](Registry& r, entity::entity_id e, const json& j) {
				T c{};
				load_fn(c, j);
				r.add_component<T>(e, c);
				};
			m.clone = [](Registry& r, entity::entity_id s, entity::entity_id d) {
				if (auto* c = r.try_get_component<T>(s)) r.add_component<T>(d, *c);
				};
			m.on_destroy = std::move(on_destroy);
			return m;
		}

		std::vector<ComponentMeta> build_registry() {
			using namespace me::components;
			std::vector<ComponentMeta> r;

			// Transform — parent/children are relinked in a second pass by the scene
			// loader (they reference other entities), so load() only reads the local TRS.
			r.push_back(meta<TransformComponent>("Transform",
				[](const TransformComponent& t) {
					return json{
						{"x", t.position.x}, {"y", t.position.y}, {"z", t.position.z},
						{"rot_x", t.rotation.x}, {"rot_y", t.rotation.y}, {"rot_z", t.rotation.z},
						{"sx", t.scale.x}, {"sy", t.scale.y}, {"sz", t.scale.z},
						{"parent", static_cast<uint32_t>(t.parent)},
						{"children", t.children}
					};
				},
				[](TransformComponent& t, const json& j) {
					t.position = { j.value("x", 0.f), j.value("y", 0.f), j.value("z", 0.f) };
					t.rotation = { j.value("rot_x", 0.f), j.value("rot_y", 0.f), j.value("rot_z", 0.f) };
					t.scale = { j.value("sx", 1.f), j.value("sy", 1.f), j.value("sz", 1.f) };
				}));

			// Tag
			r.push_back(meta<TagComponent>("Tag",
				[](const TagComponent& c) { return json{ {"name", c.name} }; },
				[](TagComponent& c, const json& j) { c.name = j.value("name", "Entity"); }));

			// Shape3D (a.k.a. MeshRenderer in the scene format)
			r.push_back(meta<Shape3DComponent>("MeshRenderer",
				[](const Shape3DComponent& c) {
					json j; j["type"] = static_cast<int>(c.type);
					write_color(j, c.color);
					j["wireframe"] = c.wireframe;
					return j;
				},
				[](Shape3DComponent& c, const json& j) {
					c.type = static_cast<Shape3DComponent::Type>(j.value("type", 0));
					c.color = read_color(j);
					c.wireframe = j.value("wireframe", false);
				}));

			// Model3D — owns a cached model handle.
			r.push_back(meta<Model3DComponent>("Model",
				[](const Model3DComponent& c) {
					const char* path = me::assets::internal_get_model_path(c.model);
					json j; j["path"] = path ? path : "";
					write_color(j, c.tint);
					return j;
				},
				[](Model3DComponent& c, const json& j) {
					std::string path = j.value("path", "");
					if (!path.empty()) c.model = me::assets::load_model(path.c_str());
					c.tint = read_color(j);
				},
				[](Registry& reg, entity::entity_id e) {
					if (auto* c = reg.try_get_component<Model3DComponent>(e)) me::assets::release(c->model);
				}));

			// Material
			r.push_back(meta<MaterialComponent>("Material",
				[](const MaterialComponent& c) {
					json j;
					write_color(j, c.albedo, true);
					j["roughness"] = c.roughness;
					j["metallic"] = c.metallic;
					j["emission_power"] = c.emission_power;
					j["transmission"] = c.transmission;
					j["ior"] = c.ior;
					return j;
				},
				[](MaterialComponent& c, const json& j) {
					c.albedo = read_color(j, true);
					c.roughness = j.value("roughness", 1.0f);
					c.metallic = j.value("metallic", 0.0f);
					c.emission_power = j.value("emission_power", 0.0f);
					c.transmission = j.value("transmission", 0.0f);
					c.ior = j.value("ior", 1.5f);
				}));

			// Script — custom: dual-format load, only added when non-empty, and the
			// clone keeps paths only so the copy re-initializes its own Lua state.
			{
				ComponentMeta m;
				m.name = "Script";
				m.has = [](Registry& reg, entity::entity_id e) { return reg.has_component<ScriptComponent>(e); };
				m.save = [](Registry& reg, entity::entity_id e) -> json {
					auto* sc = reg.try_get_component<ScriptComponent>(e);
					json arr = json::array();
					for (const auto& inst : sc->scripts) arr.push_back({ {"path", inst.path} });
					return json{ {"scripts", arr} };
					};
				m.load = [](Registry& reg, entity::entity_id e, const json& j) {
					ScriptComponent sc;
					if (j.contains("path")) sc.scripts.push_back({ j.value("path", "") });
					else if (j.contains("scripts") && j["scripts"].is_array())
						for (const auto& s : j["scripts"]) sc.scripts.push_back({ s.value("path", "") });
					if (!sc.scripts.empty()) reg.add_component<ScriptComponent>(e, sc);
					};
				m.clone = [](Registry& reg, entity::entity_id s, entity::entity_id d) {
					if (auto* sc = reg.try_get_component<ScriptComponent>(s)) {
						ScriptComponent fresh;
						for (const auto& inst : sc->scripts) fresh.scripts.push_back({ inst.path });
						reg.add_component<ScriptComponent>(d, fresh);
					}
					};
				r.push_back(std::move(m));
			}

			// Point Light
			r.push_back(meta<LightComponent>("Light",
				[](const LightComponent& c) {
					json j; write_color(j, c.color); j["intensity"] = c.intensity; return j;
				},
				[](LightComponent& c, const json& j) {
					c.color = read_color(j);
					c.intensity = j.value("intensity", 1.0f);
				}));

			// Directional Light
			r.push_back(meta<DirectionalLightComponent>("DirectionalLight",
				[](const DirectionalLightComponent& c) {
					json j; write_color(j, c.color); j["intensity"] = c.intensity; return j;
				},
				[](DirectionalLightComponent& c, const json& j) {
					c.color = read_color(j);
					c.intensity = j.value("intensity", 1.0f);
				}));

			// Camera3D
			r.push_back(meta<CameraComponent>("Camera",
				[](const CameraComponent& c) {
					return json{
						{"active", c.active},
						{"target_x", c.target.x}, {"target_y", c.target.y}, {"target_z", c.target.z},
						{"up_x", c.up.x}, {"up_y", c.up.y}, {"up_z", c.up.z},
						{"fov", c.fov},
						{"projection", c.projection}
					};
				},
				[](CameraComponent& c, const json& j) {
					c.active = j.value("active", true);
					c.target = { j.value("target_x", 0.f), j.value("target_y", 0.f), j.value("target_z", 0.f) };
					c.up = { j.value("up_x", 0.f), j.value("up_y", 1.f), j.value("up_z", 0.f) };
					c.fov = j.value("fov", 45.0f);
					c.projection = j.value("projection", 0);
				}));

			// Camera2D
			r.push_back(meta<Camera2DComponent>("Camera2D",
				[](const Camera2DComponent& c) {
					return json{
						{"active", c.active},
						{"offset_x", c.offset.x}, {"offset_y", c.offset.y},
						{"rotation", c.rotation},
						{"zoom", c.zoom}
					};
				},
				[](Camera2DComponent& c, const json& j) {
					c.active = j.value("active", true);
					c.offset = { j.value("offset_x", 0.f), j.value("offset_y", 0.f) };
					c.rotation = j.value("rotation", 0.f);
					c.zoom = j.value("zoom", 1.0f);
				}));

			// Shape2D
			r.push_back(meta<Shape2DComponent>("Shape2D",
				[](const Shape2DComponent& c) {
					json j; j["type"] = static_cast<int>(c.type);
					write_color(j, c.color);
					j["wireframe"] = c.wireframe;
					return j;
				},
				[](Shape2DComponent& c, const json& j) {
					c.type = static_cast<Shape2DComponent::Type>(j.value("type", 0));
					c.color = read_color(j);
					c.wireframe = j.value("wireframe", false);
				}));

			// RigidBody
			r.push_back(meta<RigidBodyComponent>("RigidBody",
				[](const RigidBodyComponent& c) {
					return json{
						{"type", static_cast<int>(c.type)},
						{"mass", c.mass},
						{"bounciness", c.bounciness},
						{"friction", c.friction}
					};
				},
				[](RigidBodyComponent& c, const json& j) {
					c.type = static_cast<RigidBodyType>(j.value("type", 1));
					c.mass = j.value("mass", 1.0f);
					c.bounciness = j.value("bounciness", 0.2f);
					c.friction = j.value("friction", 0.5f);
				}));

			// BoxCollider
			r.push_back(meta<BoxColliderComponent>("BoxCollider",
				[](const BoxColliderComponent& c) {
					return json{
						{"hx", c.half_extents.x}, {"hy", c.half_extents.y}, {"hz", c.half_extents.z},
						{"show_debug", c.show_debug}
					};
				},
				[](BoxColliderComponent& c, const json& j) {
					c.half_extents = { j.value("hx", 0.5f), j.value("hy", 0.5f), j.value("hz", 0.5f) };
					c.show_debug = j.value("show_debug", true);
				}));

			// SphereCollider
			r.push_back(meta<SphereColliderComponent>("SphereCollider",
				[](const SphereColliderComponent& c) {
					return json{ {"radius", c.radius}, {"show_debug", c.show_debug} };
				},
				[](SphereColliderComponent& c, const json& j) {
					c.radius = j.value("radius", 1.0f);
					c.show_debug = j.value("show_debug", true);
				}));

			// AudioListener
			r.push_back(meta<AudioListenerComponent>("AudioListener",
				[](const AudioListenerComponent& c) { return json{ {"active", c.active} }; },
				[](AudioListenerComponent& c, const json& j) { c.active = j.value("active", true); }));

			// AudioSource — owns a cached sound handle.
			r.push_back(meta<AudioSourceComponent>("AudioSource",
				[](const AudioSourceComponent& c) {
					return json{
						{"filepath", c.filepath},
						{"volume", c.volume},
						{"pitch", c.pitch},
						{"play_on_awake", c.play_on_awake},
						{"spatial", c.spatial},
						{"max_distance", c.max_distance}
					};
				},
				[](AudioSourceComponent& c, const json& j) {
					c.filepath = j.value("filepath", "");
					c.volume = j.value("volume", 1.0f);
					c.pitch = j.value("pitch", 1.0f);
					c.play_on_awake = j.value("play_on_awake", false);
					c.spatial = j.value("spatial", true);
					c.max_distance = j.value("max_distance", 50.0f);
					if (!c.filepath.empty()) c.clip = me::audio::load(c.filepath.c_str());
				},
				[](Registry& reg, entity::entity_id e) {
					if (auto* c = reg.try_get_component<AudioSourceComponent>(e)) me::audio::release(c->clip);
				}));

			// BackgroundMusic — owns a cached music-stream handle.
			r.push_back(meta<BackgroundMusicComponent>("BackgroundMusic",
				[](const BackgroundMusicComponent& c) {
					return json{
						{"filepath", c.filepath},
						{"volume", c.volume},
						{"loop", c.loop},
						{"play_on_awake", c.play_on_awake}
					};
				},
				[](BackgroundMusicComponent& c, const json& j) {
					c.filepath = j.value("filepath", "");
					c.volume = j.value("volume", 1.0f);
					c.loop = j.value("loop", true);
					c.play_on_awake = j.value("play_on_awake", false);
					if (!c.filepath.empty()) c.stream = me::audio::load_music(c.filepath.c_str());
				},
				[](Registry& reg, entity::entity_id e) {
					if (auto* c = reg.try_get_component<BackgroundMusicComponent>(e)) me::audio::release(c->stream);
				}));

			// Sprite — previously never persisted. Registered here so 2D sprites
			// round-trip and free their texture on delete. (Whether the engine draws
			// them is a separate, still-open question; this only handles the data.)
			r.push_back(meta<SpriteComponent>("Sprite",
				[](const SpriteComponent& c) {
					const char* path = me::assets::internal_get_texture_path(c.texture);
					json j; j["path"] = path ? path : "";
					write_color(j, c.tint, true);
					return j;
				},
				[](SpriteComponent& c, const json& j) {
					std::string path = j.value("path", "");
					if (!path.empty()) c.texture = me::assets::load_texture(path.c_str());
					c.tint = read_color(j, true);
				},
				[](Registry& reg, entity::entity_id e) {
					if (auto* c = reg.try_get_component<SpriteComponent>(e)) me::assets::release(c->texture);
				}));

			return r;
		}

	} // namespace

	const std::vector<ComponentMeta>& components() {
		static const std::vector<ComponentMeta> s_components = build_registry();
		return s_components;
	}

	void clone_entity(Registry& reg, entity::entity_id src, entity::entity_id dst) {
		for (const auto& m : components()) {
			if (m.clone && m.has(reg, src)) m.clone(reg, src, dst);
		}
	}

	void release_native_handles(Registry& reg, entity::entity_id e) {
		for (const auto& m : components()) {
			if (m.on_destroy && m.has(reg, e)) m.on_destroy(reg, e);
		}
	}

} // namespace me::ecs
