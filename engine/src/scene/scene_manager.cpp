#include "mini-engine-raylib/scene/scene_manager.hpp"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/core/events.hpp"
#include "mini-engine-raylib/ecs/components.hpp"
#include "mini-engine-raylib/ecs/physics_components.hpp" // NEW: We need to see the physics components
#include "mini-engine-raylib/ecs/script_component.hpp"
#include "mini-engine-raylib/core/file_system.hpp"
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

				je["components"] = std::move(comps);
				root["entities"].push_back(std::move(je));
			}

			std::filesystem::path physical_path = me::fs::resolve_if_virtual(filepath);

			std::ofstream ofs(physical_path, std::ios::binary);
			if (!ofs) {
				std::cerr << "Failed to save scene to: " << physical_path << "\n";
				return false;
			}
			ofs << root.dump(4);
			return true;
		}

		bool load(const std::string& filepath) {

			std::filesystem::path physical_path = me::fs::resolve_if_virtual(filepath);

			std::ifstream ifs(physical_path, std::ios::binary);
			if (!ifs) {
				std::cerr << "Failed to load scene from: " << physical_path << "\n";
				return false;
			}

			json root;
			try { ifs >> root; } catch (...) { return false; }

			clear();

			auto& reg = me::get_registry();
			if (!root.contains("entities") || !root["entities"].is_array()) return true;

			for (const auto& je : root["entities"]) {
				me::Entity e = reg.create_entity();
				if (!je.contains("components")) continue;

				const auto& comps = je["components"];

				if (comps.contains("Tag")) {
					e.add_component(me::components::TagComponent{ comps["Tag"].value("name", "Entity") });
				}

				if (comps.contains("Transform")) {
					auto& j = comps["Transform"];
					e.add_component(me::components::TransformComponent{
						{ j.value("x", 0.f), j.value("y", 0.f), j.value("z", 0.f) },
						{ j.value("rot_x", 0.f), j.value("rot_y", 0.f), j.value("rot_z", 0.f) },
						{ j.value("sx", 1.f), j.value("sy", 1.f), j.value("sz", 1.f) }
						});
				}

				if (comps.contains("MeshRenderer")) {
					auto& j = comps["MeshRenderer"];
					me::components::Shape3DComponent sc;
					sc.type = static_cast<me::components::Shape3DComponent::Type>(j.value("type", 0));
					sc.color = me::Color{
						static_cast<unsigned char>(j.value("color_r", 255)),
						static_cast<unsigned char>(j.value("color_g", 255)),
						static_cast<unsigned char>(j.value("color_b", 255)),
						255
					};
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

					mc.tint = me::Color{
						static_cast<unsigned char>(j.value("color_r", 255)),
						static_cast<unsigned char>(j.value("color_g", 255)),
						static_cast<unsigned char>(j.value("color_b", 255)),
						255
					};
					e.add_component(mc);
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
					lc.color = me::Color{
						static_cast<unsigned char>(j.value("color_r", 255)),
						static_cast<unsigned char>(j.value("color_g", 255)),
						static_cast<unsigned char>(j.value("color_b", 255)),
						255
					};
					lc.intensity = j.value("intensity", 1.0f);
					e.add_component(lc);
				}

				if (comps.contains("DirectionalLight")) {
					auto& j = comps["DirectionalLight"];
					me::components::DirectionalLightComponent dlc;
					dlc.color = me::Color{
						static_cast<unsigned char>(j.value("color_r", 255)),
						static_cast<unsigned char>(j.value("color_g", 255)),
						static_cast<unsigned char>(j.value("color_b", 255)),
						255
					};
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
					s2d.color = me::Color{
						static_cast<unsigned char>(j.value("color_r", 255)),
						static_cast<unsigned char>(j.value("color_g", 255)),
						static_cast<unsigned char>(j.value("color_b", 255)),
						255
					};
					s2d.wireframe = j.value("wireframe", false);
					e.add_component(s2d);
				}

				if (comps.contains("RigidBody")) {
					auto& j = comps["RigidBody"];
					me::components::RigidBodyComponent rb;
					rb.type = static_cast<me::components::RigidBodyType>(j.value("type", 1)); // 1 == Dynamic
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
			}

			me::get_event_bus().publish<me::events::SceneLoadedEvent>(filepath);

			return true;
		}

	} // namespace scene_manager
} // namespace me
