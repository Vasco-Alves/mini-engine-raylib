#include "mini-engine-raylib/scene/scene_manager.hpp"

#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/ecs/components.hpp"
#include "mini-engine-raylib/ecs/script_component.hpp"
#include "../assets/assets_internal.hpp"

#include <mini-ecs/registry.hpp>

using json = nlohmann::ordered_json;

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
					{"sx", t.scale.x}, {"sy", t.scale.y}, {"sz", t.scale.z}
				};

				// 2. Tag
				if (auto* tag = reg.try_get_component<me::components::TagComponent>(e)) {
					comps["Tag"] = json{ {"name", tag->name} };
				}

				// 3. Shape3D (Formerly MeshRenderer)
				if (auto* mesh = reg.try_get_component<me::components::Shape3DComponent>(e)) {
					// We keep the JSON key as "MeshRenderer" so old save files don't break!
					comps["MeshRenderer"] = json{
						{"type", static_cast<int>(mesh->type)},
						{"color_r", mesh->color.r},
						{"color_g", mesh->color.g},
						{"color_b", mesh->color.b}
					};
				}

				// 4. Script
				if (auto* script = reg.try_get_component<me::components::ScriptComponent>(e)) {
					comps["Script"] = json{ {"path", script->path} };
				}

				// 5. Model3D
				if (auto* mod = reg.try_get_component<me::components::Model3DComponent>(e)) {
					const char* path = me::assets::internal_get_model_path(mod->model);
					comps["Model"] = json{
						{"path", path ? path : ""},
						{"color_r", mod->tint.r},
						{"color_g", mod->tint.g},
						{"color_b", mod->tint.b}
					};
				}

				je["components"] = std::move(comps);
				root["entities"].push_back(std::move(je));
			}

			std::ofstream ofs(filepath, std::ios::binary);
			if (!ofs) {
				std::cerr << "Failed to save scene to: " << filepath << "\n";
				return false;
			}
			ofs << root.dump(2);
			return true;
		}

		bool load(const std::string& filepath) {
			std::ifstream ifs(filepath, std::ios::binary);
			if (!ifs) {
				std::cerr << "Failed to load scene from: " << filepath << "\n";
				return false;
			}

			json root;
			try { ifs >> root; } catch (...) { return false; }

			// Clear current scene before loading new one
			clear();

			auto& reg = me::get_registry();
			if (!root.contains("entities") || !root["entities"].is_array()) return true;

			for (const auto& je : root["entities"]) {
				me::Entity e = reg.create_entity("Entity");
				if (!je.contains("components")) continue;

				const auto& comps = je["components"];

				if (comps.contains("Transform")) {
					auto& j = comps["Transform"];
					e.add_component(me::components::TransformComponent{
						{ j.value("x", 0.f), j.value("y", 0.f), j.value("z", 0.f) },
						{ j.value("rot_x", 0.f), j.value("rot_y", 0.f), j.value("rot_z", 0.f) },
						{ j.value("sx", 1.f), j.value("sy", 1.f), j.value("sz", 1.f) }
						});
				}

				if (comps.contains("Tag")) {
					e.add_component(me::components::TagComponent{ comps["Tag"].value("name", "Entity") });
				}

				if (comps.contains("MeshRenderer")) {
					auto& j = comps["MeshRenderer"];
					e.add_component(me::components::Shape3DComponent{
						static_cast<me::components::Shape3DComponent::Type>(j.value("type", 0)),
						me::Color{
							static_cast<unsigned char>(j.value("color_r", 255)),
							static_cast<unsigned char>(j.value("color_g", 255)),
							static_cast<unsigned char>(j.value("color_b", 255)),
							255
						}
						});
				}

				if (comps.contains("Model")) {
					auto& j = comps["Model"];
					me::components::Model3DComponent mc;

					std::string path = j.value("path", "");
					if (!path.empty()) {
						mc.model = me::assets::load_model(path.c_str());
					}

					mc.tint = me::Color{
						static_cast<unsigned char>(j.value("color_r", 255)),
						static_cast<unsigned char>(j.value("color_g", 255)),
						static_cast<unsigned char>(j.value("color_b", 255)),
						255
					};
					e.add_component(mc);
				}

				if (comps.contains("Script")) {
					e.add_component(me::components::ScriptComponent{ comps["Script"].value("path", "") });
				}
			}
			return true;
		}

	} // namespace scene_manager
} // namespace me
