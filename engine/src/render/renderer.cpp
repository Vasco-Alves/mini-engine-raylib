#include "mini-engine-raylib/render/renderer.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <string>

#include "../assets/assets_internal.hpp"
#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/ecs/components.hpp"
#include "mini-engine-raylib/core/file_system.hpp"
#include "mini-engine-raylib/core/logger.hpp"
#include <mini-ecs/registry.hpp>

namespace me::render {

	static Shader s_LightingShader;
	static Shader s_DefaultShader;
	static bool s_LightingEnabled = true;
	static int s_ViewPosLoc;

	// Point Lights
#define MAX_LIGHTS 8
	static int s_LightCountLoc;
	static int s_LightPosLoc[MAX_LIGHTS];
	static int s_LightColorLoc[MAX_LIGHTS];
	static int s_LightIntensityLoc[MAX_LIGHTS];

	// Directional Light (The Sun)
	static int s_DirLightDirLoc;
	static int s_DirLightColorLoc;
	static int s_DirLightIntensityLoc;
	static int s_HasDirLightLoc;

	inline static ::Color to_ray(me::Color c) { return ::Color{ c.r, c.g, c.b, c.a }; }

	void init() {
		// 1. Load Custom Shader
		std::string vsCode = me::fs::read_text("engine://shaders/lighting.vs");
		std::string fsCode = me::fs::read_text("engine://shaders/lighting.fs");
		s_LightingShader = LoadShaderFromMemory(vsCode.c_str(), fsCode.c_str());

		// 2. Cache Raylib's Default Shader
		s_DefaultShader.id = rlGetShaderIdDefault();
		s_DefaultShader.locs = rlGetShaderLocsDefault();

		// 3. Cache Uniforms
		s_ViewPosLoc = GetShaderLocation(s_LightingShader, "viewPos");
		s_LightCountLoc = GetShaderLocation(s_LightingShader, "lightCount");

		for (int i = 0; i < MAX_LIGHTS; i++) {
			s_LightPosLoc[i] = GetShaderLocation(s_LightingShader, TextFormat("lights[%i].position", i));
			s_LightColorLoc[i] = GetShaderLocation(s_LightingShader, TextFormat("lights[%i].color", i));
			s_LightIntensityLoc[i] = GetShaderLocation(s_LightingShader, TextFormat("lights[%i].intensity", i));
		}

		s_DirLightDirLoc = GetShaderLocation(s_LightingShader, "dirLightDir");
		s_DirLightColorLoc = GetShaderLocation(s_LightingShader, "dirLightColor");
		s_DirLightIntensityLoc = GetShaderLocation(s_LightingShader, "dirLightIntensity");
		s_HasDirLightLoc = GetShaderLocation(s_LightingShader, "hasDirLight");
	}

	void shutdown() {
		UnloadShader(s_LightingShader);
	}

	void clear_world(me::Color color) {
		ClearBackground(to_ray(color));
	}

	void render_world(const me::components::TransformComponent* override_transform, const me::components::CameraComponent* override_cam) {
		auto& reg = me::get_registry();

		Camera3D rayCam = { 0 };
		rayCam.position = { 10.0f, 10.0f, 10.0f };
		rayCam.target = { 0.0f, 0.0f, 0.0f };
		rayCam.up = { 0.0f, 1.0f, 0.0f };
		rayCam.fovy = 45.0f;
		rayCam.projection = CAMERA_PERSPECTIVE;

		// ==========================================
		// 1. WHICH CAMERA ARE WE USING?
		// ==========================================
		if (override_transform && override_cam) {
			rayCam.position = { override_transform->position.x, override_transform->position.y, override_transform->position.z };
			rayCam.target = { override_cam->target.x, override_cam->target.y, override_cam->target.z };
			rayCam.up = { override_cam->up.x, override_cam->up.y, override_cam->up.z };
			rayCam.fovy = override_cam->fov;
			rayCam.projection = override_cam->projection;
		} else {
			auto& camPool = reg.view<me::components::CameraComponent>();
			for (size_t i = 0; i < camPool.size(); ++i) {
				me::entity::entity_id e = camPool.entity_map[i];
				auto& cam = camPool.components[i];
				if (cam.active) {
					auto* t = reg.try_get_component<me::components::TransformComponent>(e);
					if (t) {
						rayCam.position = { t->position.x, t->position.y, t->position.z };
						rayCam.target = { cam.target.x, cam.target.y, cam.target.z };
						rayCam.up = { cam.up.x, cam.up.y, cam.up.z };
						rayCam.fovy = cam.fov;
						rayCam.projection = cam.projection;
						break;
					}
				}
			}
		}

		// ==========================================
		// 2. EXTRACT LIGHTS & INJECT UNIFORMS
		// ==========================================
		if (s_LightingEnabled) {
			Vector3 viewPos = rayCam.position;
			SetShaderValue(s_LightingShader, s_ViewPosLoc, &viewPos, SHADER_UNIFORM_VEC3);

			// --- A. Point Lights ---
			int active_lights = 0;
			auto& light_pool = reg.view<me::components::LightComponent>();
			for (size_t i = 0; i < light_pool.size() && active_lights < MAX_LIGHTS; ++i) {
				me::entity::entity_id e = light_pool.entity_map[i];
				if (auto* t = reg.try_get_component<me::components::TransformComponent>(e)) {
					auto& l = light_pool.components[i];
					Vector3 pos = { t->position.x, t->position.y, t->position.z };
					Vector3 col = { l.color.r / 255.0f, l.color.g / 255.0f, l.color.b / 255.0f };
					SetShaderValue(s_LightingShader, s_LightPosLoc[active_lights], &pos, SHADER_UNIFORM_VEC3);
					SetShaderValue(s_LightingShader, s_LightColorLoc[active_lights], &col, SHADER_UNIFORM_VEC3);
					SetShaderValue(s_LightingShader, s_LightIntensityLoc[active_lights], &l.intensity, SHADER_UNIFORM_FLOAT);
					active_lights++;
				}
			}
			SetShaderValue(s_LightingShader, s_LightCountLoc, &active_lights, SHADER_UNIFORM_INT);

			// --- B. Directional Light (The Sun) ---
			int has_dir_light = 0;
			auto& dir_pool = reg.view<me::components::DirectionalLightComponent>();
			if (dir_pool.size() > 0) {
				me::entity::entity_id e = dir_pool.entity_map[0];
				if (auto* t = reg.try_get_component<me::components::TransformComponent>(e)) {
					auto& dl = dir_pool.components[0];
					has_dir_light = 1;

					float pitch = t->rotation.x * DEG2RAD;
					float yaw = t->rotation.y * DEG2RAD;
					Vector3 dir = {
						std::cos(pitch) * std::sin(yaw),
						-std::sin(pitch),
						std::cos(pitch) * std::cos(yaw)
					};
					dir = Vector3Normalize(dir);

					Vector3 col = { dl.color.r / 255.0f, dl.color.g / 255.0f, dl.color.b / 255.0f };
					SetShaderValue(s_LightingShader, s_DirLightDirLoc, &dir, SHADER_UNIFORM_VEC3);
					SetShaderValue(s_LightingShader, s_DirLightColorLoc, &col, SHADER_UNIFORM_VEC3);
					SetShaderValue(s_LightingShader, s_DirLightIntensityLoc, &dl.intensity, SHADER_UNIFORM_FLOAT);
				}
			}
			SetShaderValue(s_LightingShader, s_HasDirLightLoc, &has_dir_light, SHADER_UNIFORM_INT);
		}

		// ==========================================
		// 3. RENDER SCENE
		// ==========================================
		BeginMode3D(rayCam);
		DrawGrid(100, 1.0f);

		if (s_LightingEnabled) BeginShaderMode(s_LightingShader);

		// --- DRAW 3D PRIMITIVES ---
		auto& meshPool = reg.view<me::components::Shape3DComponent>();
		for (size_t i = 0; i < meshPool.size(); ++i) {
			me::entity::entity_id e = meshPool.entity_map[i];
			auto& mesh = meshPool.components[i];
			auto* t = reg.try_get_component<me::components::TransformComponent>(e);
			if (!t) continue;

			::Color col = to_ray(mesh.color);
			rlPushMatrix();
			Matrix glMatrix = MatrixTranspose(t->model_matrix);
			rlMultMatrixf((float*)&glMatrix);

			if (mesh.type == me::components::Shape3DComponent::Cube) {
				if (mesh.wireframe) DrawCubeWires({ 0,0,0 }, 2.0f, 2.0f, 2.0f, col);
				else DrawCube({ 0,0,0 }, 2.0f, 2.0f, 2.0f, col);
			} else if (mesh.type == me::components::Shape3DComponent::Sphere) {
				if (mesh.wireframe) DrawSphereWires({ 0,0,0 }, 1.0f, 16, 16, col);
				else DrawSphere({ 0,0,0 }, 1.0f, col);
			} else if (mesh.type == me::components::Shape3DComponent::Plane) {
				DrawPlane({ 0,0,0 }, { 2.0f, 2.0f }, col);
			}
			rlPopMatrix();
		}

		// --- DRAW 3D MODELS ---
		auto& modelPool = reg.view<me::components::Model3DComponent>();
		Shader active_shader = s_LightingEnabled ? s_LightingShader : s_DefaultShader;

		for (size_t i = 0; i < modelPool.size(); ++i) {
			me::entity::entity_id e = modelPool.entity_map[i];
			auto& modComp = modelPool.components[i];
			auto* t = reg.try_get_component<me::components::TransformComponent>(e);
			if (!t) continue;

			const ::Model* const_model = me::assets::internal_get_model(modComp.model);
			if (const_model) {
				::Model* raw_model = const_cast<::Model*>(const_model);
				for (int j = 0; j < raw_model->materialCount; j++) {
					raw_model->materials[j].shader = active_shader;
				}

				::Color col = to_ray(modComp.tint);
				rlPushMatrix();
				Matrix glMatrix = MatrixTranspose(t->model_matrix);
				rlMultMatrixf((float*)&glMatrix);
				DrawModel(*raw_model, { 0.0f, 0.0f, 0.0f }, 1.0f, col);
				rlPopMatrix();
			}
		}

		if (s_LightingEnabled) EndShaderMode();
		EndMode3D();
	}

	void render_2d() {
		auto& reg = me::get_registry();

		// 1. Setup the 2D Camera
		::Camera2D ray_cam2d = { 0 };
		ray_cam2d.zoom = 1.0f;

		auto& cam2d_pool = reg.view<me::components::Camera2DComponent>();
		for (size_t i = 0; i < cam2d_pool.size(); ++i) {
			me::entity::entity_id e = cam2d_pool.entity_map[i];
			auto& cam = cam2d_pool.components[i];

			if (cam.active) {
				auto* t = reg.try_get_component<me::components::TransformComponent>(e);
				if (t) {
					ray_cam2d.target = { t->position.x, t->position.y };
					ray_cam2d.offset = { cam.offset.x, cam.offset.y };
					ray_cam2d.rotation = cam.rotation;
					ray_cam2d.zoom = cam.zoom;
					break;
				}
			}
		}

		// 2. Start Drawing 2D World
		BeginMode2D(ray_cam2d);

		// --- 2D Primitives ---
		auto& shape2d_pool = reg.view<me::components::Shape2DComponent>();
		for (size_t i = 0; i < shape2d_pool.size(); ++i) {
			me::entity::entity_id e = shape2d_pool.entity_map[i];
			auto& shape = shape2d_pool.components[i];

			auto* t = reg.try_get_component<me::components::TransformComponent>(e);
			if (!t) continue;

			::Color col = to_ray(shape.color);

			if (shape.type == me::components::Shape2DComponent::Rectangle) {
				::Rectangle rect = { t->position.x, t->position.y, t->scale.x, t->scale.y };
				::Vector2 origin = { rect.width / 2.0f, rect.height / 2.0f };

				if (shape.wireframe) DrawRectangleLinesEx(rect, 1.0f, col);
				else DrawRectanglePro(rect, origin, t->rotation.z, col);

			} else if (shape.type == me::components::Shape2DComponent::Circle) {
				if (shape.wireframe) DrawCircleLines(t->position.x, t->position.y, t->scale.x, col);
				else DrawCircle(t->position.x, t->position.y, t->scale.x, col);
			}
		}

		// --- Sprites ---
		auto& sprite_pool = reg.view<me::components::SpriteComponent>();
		for (size_t i = 0; i < sprite_pool.size(); ++i) {
			me::entity::entity_id e = sprite_pool.entity_map[i];
			auto& sprite = sprite_pool.components[i];

			auto* t = reg.try_get_component<me::components::TransformComponent>(e);
			if (!t) continue;

			const ::Texture2D* tex = me::assets::internal_get_texture(sprite.texture);
			if (tex) {
				::Rectangle source = { 0.0f, 0.0f, (float)tex->width, (float)tex->height };
				::Rectangle dest = { t->position.x, t->position.y, tex->width * t->scale.x, tex->height * t->scale.y };
				::Vector2 origin = { dest.width / 2.0f, dest.height / 2.0f };

				DrawTexturePro(*tex, source, dest, origin, t->rotation.z, to_ray(sprite.tint));
			}
		}

		EndMode2D();
	}

	bool is_lighting_enabled() { return s_LightingEnabled; }
	void set_lighting_enabled(bool enabled) { s_LightingEnabled = enabled; }

}
