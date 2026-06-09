#include "mini-engine-raylib/scene/scene_manager.hpp"

#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/core/events.hpp"
#include "mini-engine-raylib/ecs/components.hpp"
#include "mini-engine-raylib/ecs/physics_components.hpp"
#include "mini-engine-raylib/ecs/audio_components.hpp"
#include "mini-engine-raylib/ecs/script_component.hpp"
#include "mini-engine-raylib/core/file_system.hpp"
#include "../assets/assets_internal.hpp"
#include <mini-ecs/registry.hpp>

using json = nlohmann::ordered_json;

namespace {
	// Reads a color stored as flat color_r/g/b[/a] keys (the scene file format).
	me::Color read_color(const json& j, bool with_alpha = false) {
		return me::Color{
			static_cast<unsigned char>(j.value("color_r", 255)),
			static_cast<unsigned char>(j.value("color_g", 255)),
			static_cast<unsigned char>(j.value("color_b", 255)),
			with_alpha ? static_cast<unsigned char>(j.value("color_a", 255)) : static_cast<unsigned char>(255)
		};
	}
}

namespace me {
	namespace scene_manager {

		void clear() {
			me::get_registry().clear();
		}

		bool save(const std::string& filepath) {
			json root;
			root["entities"] = json::array();

			auto& reg = me::get_registry();
			auto& transforms = reg.view<me::components::TransformComponent>();

			for (size_t i = 0; i < transforms.size(); ++i) {
				me::entity::entity_id e = transforms.entity_map[i];
				auto& t = transforms.components[i];
				if (!reg.is_alive(e)) continue;

				json je;
				je["id"] = static_cast<uint32_t>(e);

				json comps = json::object();

				// 1. Transform 
				comps["Transform"] = json{
					{"x", t.position.x}, {"y", t.position.y}, {"z", t.position.z},
					{"rot_x", t.rotation.x}, {"rot_y", t.rotation.y}, {"rot_z", t.rotation.z},
					{"sx", t.scale.x}, {"sy", t.scale.y}, {"sz", t.scale.z},
					// Hierarchy Data
					{"parent", static_cast<uint32_t>(t.parent)},
					{"children", t.children}
				};

				// 2. Tag
				if (auto* tag = reg.try_get_component<me::components::TagComponent>(e)) {
					comps["Tag"] = json{ {"name", tag->name} };
				}

				// 3. Shape3D (MeshRenderer)
				if (auto* mesh = reg.try_get_component<me::components::Shape3DComponent>(e)) {
					comps["MeshRenderer"] = json{
						{"type", static_cast<int>(mesh->type)},
						{"color_r", mesh->color.r},
						{"color_g", mesh->color.g},
						{"color_b", mesh->color.b},
						{"wireframe", mesh->wireframe}
					};
				}

				// 4. Model3D
				if (auto* mod = reg.try_get_component<me::components::Model3DComponent>(e)) {
					const char* path = me::assets::internal_get_model_path(mod->model);
					comps["Model"] = json{
						{"path", path ? path : ""},
						{"color_r", mod->tint.r},
						{"color_g", mod->tint.g},
						{"color_b", mod->tint.b}
					};
				}

				// --- Material ---
				if (auto* mat = reg.try_get_component<me::components::MaterialComponent>(e)) {
					comps["Material"] = json{
						{"color_r", mat->albedo.r},
						{"color_g", mat->albedo.g},
						{"color_b", mat->albedo.b},
						{"color_a", mat->albedo.a},
						{"roughness", mat->roughness},
						{"metallic", mat->metallic},
						{"emission_power", mat->emission_power},
						{"transmission", mat->transmission},
						{"ior", mat->ior}
					};
				}

				// 5. Script
				if (auto* script_comp = reg.try_get_component<me::components::ScriptComponent>(e)) {
					json scripts_array = json::array();
					for (const auto& instance : script_comp->scripts) {
						scripts_array.push_back({ {"path", instance.path} });
					}
					comps["Script"] = json{ {"scripts", scripts_array} };
				}

				// 6. Point Light
				if (auto* light = reg.try_get_component<me::components::LightComponent>(e)) {
					comps["Light"] = json{
						{"color_r", light->color.r},
						{"color_g", light->color.g},
						{"color_b", light->color.b},
						{"intensity", light->intensity}
					};
				}

				// 7. Directional Light (The Sun)
				if (auto* dirLight = reg.try_get_component<me::components::DirectionalLightComponent>(e)) {
					comps["DirectionalLight"] = json{
						{"color_r", dirLight->color.r},
						{"color_g", dirLight->color.g},
						{"color_b", dirLight->color.b},
						{"intensity", dirLight->intensity}
					};
				}

				// 8. Camera3D
				if (auto* cam = reg.try_get_component<me::components::CameraComponent>(e)) {
					comps["Camera"] = json{
						{"active", cam->active},
						{"target_x", cam->target.x}, {"target_y", cam->target.y}, {"target_z", cam->target.z},
						{"up_x", cam->up.x}, {"up_y", cam->up.y}, {"up_z", cam->up.z},
						{"fov", cam->fov},
						{"projection", cam->projection}
					};
				}

				// 9. Camera2D
				if (auto* cam2d = reg.try_get_component<me::components::Camera2DComponent>(e)) {
					comps["Camera2D"] = json{
						{"active", cam2d->active},
						{"offset_x", cam2d->offset.x},
						{"offset_y", cam2d->offset.y},
						{"rotation", cam2d->rotation},
						{"zoom", cam2d->zoom}
					};
				}

				// 10. Shape2D
				if (auto* s2d = reg.try_get_component<me::components::Shape2DComponent>(e)) {
					comps["Shape2D"] = json{
						{"type", static_cast<int>(s2d->type)},
						{"color_r", s2d->color.r},
						{"color_g", s2d->color.g},
						{"color_b", s2d->color.b},
						{"wireframe", s2d->wireframe}
					};
				}

				// 11. RigidBody
				if (auto* rb = reg.try_get_component<me::components::RigidBodyComponent>(e)) {
					comps["RigidBody"] = json{
						{"type", static_cast<int>(rb->type)},
						{"mass", rb->mass},
						{"bounciness", rb->bounciness},
						{"friction", rb->friction}
					};
				}

				// 12. BoxCollider
				if (auto* col = reg.try_get_component<me::components::BoxColliderComponent>(e)) {
					comps["BoxCollider"] = json{
						{"hx", col->half_extents.x},
						{"hy", col->half_extents.y},
						{"hz", col->half_extents.z},
						{"show_debug", col->show_debug}
					};
				}

				// 13. SphereCollider
				if (auto* col = reg.try_get_component<me::components::SphereColliderComponent>(e)) {
					comps["SphereCollider"] = json{
						{"radius", col->radius},
						{"show_debug", col->show_debug}
					};
				}

				// 14. AudioListener
				if (auto* listener = reg.try_get_component<me::components::AudioListenerComponent>(e)) {
					comps["AudioListener"] = json{
						{"active", listener->active}
					};
				}

				// 15. AudioSource
				if (auto* audio = reg.try_get_component<me::components::AudioSourceComponent>(e)) {
					comps["AudioSource"] = json{
						{"filepath", audio->filepath},
						{"volume", audio->volume},
						{"pitch", audio->pitch},
						{"play_on_awake", audio->play_on_awake},
						{"spatial", audio->spatial},
						{"max_distance", audio->max_distance}
					};
				}

				// 16. Background Music
				if (auto* bgm = reg.try_get_component<me::components::BackgroundMusicComponent>(e)) {
					comps["BackgroundMusic"] = json{
						{"filepath", bgm->filepath},
						{"volume", bgm->volume},
						{"loop", bgm->loop},
						{"play_on_awake", bgm->play_on_awake}
					};
				}

				je["components"] = std::move(comps);
				root["entities"].push_back(std::move(je));
			}

			std::filesystem::path physical_path = me::fs::resolve_if_virtual(filepath);

			std::ofstream ofs(physical_path, std::ios::binary);
			if (!ofs) {
				me::logger::error("Failed to open scene file for writing: " + physical_path.string());
				return false;
			}
			try {
				ofs.exceptions(std::ios::failbit | std::ios::badbit);
				ofs << root.dump(4);
			} catch (const std::exception& ex) {
				me::logger::error("Error writing scene file: " + std::string(ex.what()));
				return false;
			}
			return true;
		}

		bool load(const std::string& filepath) {
			std::filesystem::path physical_path = me::fs::resolve_if_virtual(filepath);

			std::ifstream ifs(physical_path, std::ios::binary);
			if (!ifs) {
				me::logger::error("Failed to open scene file: " + physical_path.string());
				return false;
			}

			json root;
			try {
				ifs >> root;
			} catch (const std::exception& ex) {
				me::logger::error("Failed to parse scene JSON: " + std::string(ex.what()));
				return false;
			}

			clear();

			auto& reg = me::get_registry();
			if (!root.contains("entities") || !root["entities"].is_array()) return true;

			// =================================================================
			// PASS 1: Entity Creation & ID Mapping
			// =================================================================
			std::unordered_map<uint32_t, me::entity::entity_id> old_to_new;
			std::vector<me::Entity> loaded_entities;

			for (const auto& je : root["entities"]) {
				me::Entity e = reg.create_entity();
				loaded_entities.push_back(e);

				uint32_t old_id = je.value("id", 0);
				if (old_id != 0) {
					old_to_new[old_id] = e.get_id();
				}
			}

			// =================================================================
			// PASS 2: Component Parsing & Hierarchy Linking
			// =================================================================
			for (size_t i = 0; i < root["entities"].size(); ++i) {
				const auto& je = root["entities"][i];
				me::Entity e = loaded_entities[i];

				if (!je.contains("components")) continue;
				const auto& comps = je["components"];

				if (comps.contains("Tag")) {
					e.add_component(me::components::TagComponent{ comps["Tag"].value("name", "Entity") });
				}

				if (comps.contains("Transform")) {
					auto& j = comps["Transform"];
					me::components::TransformComponent tc;
					tc.position = { j.value("x", 0.f), j.value("y", 0.f), j.value("z", 0.f) };
					tc.rotation = { j.value("rot_x", 0.f), j.value("rot_y", 0.f), j.value("rot_z", 0.f) };
					tc.scale = { j.value("sx", 1.f), j.value("sy", 1.f), j.value("sz", 1.f) };

					// Re-link Parent
					uint32_t old_parent = j.value("parent", 0);
					if (old_parent != 0) {
						if (old_to_new.count(old_parent)) {
							tc.parent = old_to_new[old_parent];
						} else {
							me::logger::warn("Scene load: entity references unknown parent ID " +
								std::to_string(old_parent) + " — hierarchy link dropped");
							tc.parent = me::entity::null;
						}
					} else {
						tc.parent = me::entity::null;
					}

					// Re-link Children
					if (j.contains("children") && j["children"].is_array()) {
						for (uint32_t old_child : j["children"]) {
							if (old_to_new.count(old_child)) {
								tc.children.push_back(old_to_new[old_child]);
							} else {
								me::logger::warn("Scene load: entity references unknown child ID " +
									std::to_string(old_child) + " — link dropped");
							}
						}
					}

					e.add_component(tc);
				}

				if (comps.contains("MeshRenderer")) {
					auto& j = comps["MeshRenderer"];
					me::components::Shape3DComponent sc;
					sc.type = static_cast<me::components::Shape3DComponent::Type>(j.value("type", 0));
					sc.color = read_color(j);
					sc.wireframe = j.value("wireframe", false);
					e.add_component(sc);
				}

				if (comps.contains("Model")) {
					auto& j = comps["Model"];
					me::components::Model3DComponent mc;

					std::string path = j.value("path", "");
					if (!path.empty()) {
						mc.model = me::assets::load_model(path.c_str());
					}

					mc.tint = read_color(j);
					e.add_component(mc);
				}

				// --- Material ---
				if (comps.contains("Material")) {
					auto& j = comps["Material"];
					me::components::MaterialComponent mat;
					mat.albedo = read_color(j, true);
					mat.roughness = j.value("roughness", 1.0f);
					mat.metallic = j.value("metallic", 0.0f);
					mat.emission_power = j.value("emission_power", 0.0f);
					mat.transmission = j.value("transmission", 0.0f);
					mat.ior = j.value("ior", 1.5f);

					e.add_component(mat);
				}

				if (comps.contains("Script")) {
					auto& j = comps["Script"];
					me::components::ScriptComponent sc;

					if (j.contains("path")) sc.scripts.push_back({ j.value("path", "") });
					else if (j.contains("scripts") && j["scripts"].is_array()) {
						for (const auto& s : j["scripts"]) sc.scripts.push_back({ s.value("path", "") });
					}
					if (!sc.scripts.empty()) e.add_component(sc);
				}

				if (comps.contains("Light")) {
					auto& j = comps["Light"];
					me::components::LightComponent lc;
					lc.color = read_color(j);
					lc.intensity = j.value("intensity", 1.0f);
					e.add_component(lc);
				}

				if (comps.contains("DirectionalLight")) {
					auto& j = comps["DirectionalLight"];
					me::components::DirectionalLightComponent dlc;
					dlc.color = read_color(j);
					dlc.intensity = j.value("intensity", 1.0f);
					e.add_component(dlc);
				}

				if (comps.contains("Camera")) {
					auto& j = comps["Camera"];
					me::components::CameraComponent cc;
					cc.active = j.value("active", true);
					cc.target = { j.value("target_x", 0.f), j.value("target_y", 0.f), j.value("target_z", 0.f) };
					cc.up = { j.value("up_x", 0.f), j.value("up_y", 1.f), j.value("up_z", 0.f) };
					cc.fov = j.value("fov", 45.0f);
					cc.projection = j.value("projection", 0);
					e.add_component(cc);
				}

				if (comps.contains("Camera2D")) {
					auto& j = comps["Camera2D"];
					me::components::Camera2DComponent c2d;
					c2d.active = j.value("active", true);
					c2d.offset = { j.value("offset_x", 0.f), j.value("offset_y", 0.f) };
					c2d.rotation = j.value("rotation", 0.f);
					c2d.zoom = j.value("zoom", 1.0f);
					e.add_component(c2d);
				}

				if (comps.contains("Shape2D")) {
					auto& j = comps["Shape2D"];
					me::components::Shape2DComponent s2d;
					s2d.type = static_cast<me::components::Shape2DComponent::Type>(j.value("type", 0));
					s2d.color = read_color(j);
					s2d.wireframe = j.value("wireframe", false);
					e.add_component(s2d);
				}

				if (comps.contains("RigidBody")) {
					auto& j = comps["RigidBody"];
					me::components::RigidBodyComponent rb;
					rb.type = static_cast<me::components::RigidBodyType>(j.value("type", 1));
					rb.mass = j.value("mass", 1.0f);
					rb.bounciness = j.value("bounciness", 0.2f);
					rb.friction = j.value("friction", 0.5f);
					e.add_component(rb);
				}

				if (comps.contains("BoxCollider")) {
					auto& j = comps["BoxCollider"];
					me::components::BoxColliderComponent col;
					col.half_extents = { j.value("hx", 0.5f), j.value("hy", 0.5f), j.value("hz", 0.5f) };
					col.show_debug = j.value("show_debug", true);
					e.add_component(col);
				}

				if (comps.contains("SphereCollider")) {
					auto& j = comps["SphereCollider"];
					me::components::SphereColliderComponent col;
					col.radius = j.value("radius", 1.0f);
					col.show_debug = j.value("show_debug", true);
					e.add_component(col);
				}

				if (comps.contains("AudioListener")) {
					auto& j = comps["AudioListener"];
					me::components::AudioListenerComponent listener;
					listener.active = j.value("active", true);
					e.add_component(listener);
				}

				if (comps.contains("AudioSource")) {
					auto& j = comps["AudioSource"];
					me::components::AudioSourceComponent audio;
					audio.filepath = j.value("filepath", "");
					audio.volume = j.value("volume", 1.0f);
					audio.pitch = j.value("pitch", 1.0f);
					audio.play_on_awake = j.value("play_on_awake", false);
					audio.spatial = j.value("spatial", true);
					audio.max_distance = j.value("max_distance", 50.0f);

					if (!audio.filepath.empty()) {
						audio.clip = me::audio::load(audio.filepath.c_str());
					}

					e.add_component(audio);
				}

				if (comps.contains("BackgroundMusic")) {
					auto& j = comps["BackgroundMusic"];
					me::components::BackgroundMusicComponent bgm;
					bgm.filepath = j.value("filepath", "");
					bgm.volume = j.value("volume", 1.0f);
					bgm.loop = j.value("loop", true);
					bgm.play_on_awake = j.value("play_on_awake", false);

					if (!bgm.filepath.empty()) {
						bgm.stream = me::audio::load_music(bgm.filepath.c_str());
					}
					e.add_component(bgm);
				}
			}

			me::get_event_bus().publish<me::events::SceneLoadedEvent>(filepath);

			return true;
		}

	} // namespace scene_manager
} // namespace me