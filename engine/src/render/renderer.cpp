#include "mini-engine-raylib/render/renderer.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>
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

	// Material Uniforms
	static int s_UseMaterialLoc;
	static int s_MatAlbedoLoc;
	static int s_MatRoughnessLoc;
	static int s_MatMetallicLoc;
	static int s_MatEmissionLoc;

	// Point Lights
#define MAX_LIGHTS 8
	static int s_LightCountLoc;
	static int s_LightPosLoc[MAX_LIGHTS];
	static int s_LightColorLoc[MAX_LIGHTS];
	static int s_LightIntensityLoc[MAX_LIGHTS];

	// Directional Light
	static int s_DirLightDirLoc;
	static int s_DirLightColorLoc;
	static int s_DirLightIntensityLoc;
	static int s_HasDirLightLoc;

	// Directional-light shadow map. Extent and resolution come from the
	// DirectionalLightComponent (per-light, artist-tunable in the inspector);
	// the depth target is (re)created lazily whenever the resolution changes.
	static RenderTexture2D s_ShadowMap = { 0 };
	static int s_ShadowMapRes = 0; // current depth-target size (0 = not created)
	static Matrix s_LightVP = { 0 };
	static bool s_ShadowsReady = false; // render_shadows produced a map this frame
	static int s_ShadowMapLoc;
	static int s_LightVPLoc;
	static int s_ShadowsEnabledLoc;
	static int s_ShadowMapResLoc;

	inline static ::Color to_ray(me::Color c) { return ::Color{ c.r, c.g, c.b, c.a }; }

	// A depth-texture-only framebuffer (regular RenderTextures use a non-sampleable
	// depth renderbuffer; shadow mapping needs to *read* the depth back in a shader).
	static RenderTexture2D load_depth_target(int width, int height) {
		RenderTexture2D target = { 0 };
		target.id = rlLoadFramebuffer();
		target.texture.width = width;
		target.texture.height = height;

		if (target.id == 0) {
			me::logger::error("Shadow map: could not create a framebuffer - shadows disabled");
			return target;
		}

		rlEnableFramebuffer(target.id);
		target.depth.id = rlLoadTextureDepth(width, height, false);
		target.depth.width = width;
		target.depth.height = height;
		target.depth.mipmaps = 1;
		rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
		if (!rlFramebufferComplete(target.id))
			me::logger::error("Shadow map: framebuffer incomplete - shadows disabled");
		rlDisableFramebuffer();
		return target;
	}

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

		s_UseMaterialLoc = GetShaderLocation(s_LightingShader, "useMaterial");
		s_MatAlbedoLoc = GetShaderLocation(s_LightingShader, "matAlbedo");
		s_MatRoughnessLoc = GetShaderLocation(s_LightingShader, "matRoughness");
		s_MatMetallicLoc = GetShaderLocation(s_LightingShader, "matMetallic");
		s_MatEmissionLoc = GetShaderLocation(s_LightingShader, "matEmission");

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

		// 4. Shadow Map (uniform locations only — the depth target is created
		// lazily by render_shadows at the light's configured resolution)
		s_ShadowMapLoc = GetShaderLocation(s_LightingShader, "shadowMap");
		s_LightVPLoc = GetShaderLocation(s_LightingShader, "lightVP");
		s_ShadowsEnabledLoc = GetShaderLocation(s_LightingShader, "shadowsEnabled");
		s_ShadowMapResLoc = GetShaderLocation(s_LightingShader, "shadowMapResolution");
	}

	void shutdown() {
		UnloadShader(s_LightingShader);
		if (s_ShadowMap.id != 0) {
			rlUnloadTexture(s_ShadowMap.depth.id);
			rlUnloadFramebuffer(s_ShadowMap.id);
			s_ShadowMap = { 0 };
			s_ShadowMapRes = 0;
		}
	}

	void clear_world(me::Color color) {
		ClearBackground(to_ray(color));
	}

	// Uploads one entity's MaterialComponent to the lighting shader (or flags
	// "no material" so the shader falls back to the vertex/shape color).
	static void bind_material(me::Registry& reg, me::entity::entity_id e) {
		if (!s_LightingEnabled) return;

		if (auto* mat = reg.try_get_component<me::components::MaterialComponent>(e)) {
			int useMat = 1;
			Vector4 albedo = { mat->albedo.r / 255.0f, mat->albedo.g / 255.0f, mat->albedo.b / 255.0f, mat->albedo.a / 255.0f };
			SetShaderValue(s_LightingShader, s_UseMaterialLoc, &useMat, SHADER_UNIFORM_INT);
			SetShaderValue(s_LightingShader, s_MatAlbedoLoc, &albedo, SHADER_UNIFORM_VEC4);
			SetShaderValue(s_LightingShader, s_MatRoughnessLoc, &mat->roughness, SHADER_UNIFORM_FLOAT);
			SetShaderValue(s_LightingShader, s_MatMetallicLoc, &mat->metallic, SHADER_UNIFORM_FLOAT);
			SetShaderValue(s_LightingShader, s_MatEmissionLoc, &mat->emission_power, SHADER_UNIFORM_FLOAT);
		} else {
			int useMat = 0;
			SetShaderValue(s_LightingShader, s_UseMaterialLoc, &useMat, SHADER_UNIFORM_INT);
		}
	}

	// Draws every Shape3D primitive and Model3D mesh with its cached world matrix.
	// Shared by the main pass (bind_materials = true, lighting shader on models)
	// and the shadow depth pass (bind_materials = false, default shader — only
	// the depth output matters there).
	static void draw_geometry(me::Registry& reg, const Shader& model_shader, bool bind_materials) {
		// --- DRAW 3D PRIMITIVES ---
		auto& meshPool = reg.view<me::components::Shape3DComponent>();
		for (size_t i = 0; i < meshPool.size(); ++i) {
			me::entity::entity_id e = meshPool.entity_map[i];
			auto& mesh = meshPool.components[i];
			auto* t = reg.try_get_component<me::components::TransformComponent>(e);
			if (!t) continue;

			if (bind_materials) {
				// Force Raylib to draw waiting geometry before we change shader uniforms
				rlDrawRenderBatchActive();
				bind_material(reg, e);
			}

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

		rlDrawRenderBatchActive();

		// --- DRAW 3D MODELS ---
		auto& modelPool = reg.view<me::components::Model3DComponent>();
		for (size_t i = 0; i < modelPool.size(); ++i) {
			me::entity::entity_id e = modelPool.entity_map[i];
			auto& modComp = modelPool.components[i];
			auto* t = reg.try_get_component<me::components::TransformComponent>(e);
			if (!t) continue;

			if (bind_materials) bind_material(reg, e);

			const ::Model* const_model = me::assets::internal_get_model(modComp.model);
			if (const_model) {
				::Model* raw_model = const_cast<::Model*>(const_model);
				for (int j = 0; j < raw_model->materialCount; j++) {
					raw_model->materials[j].shader = model_shader;
				}

				::Color col = to_ray(modComp.tint);
				rlPushMatrix();
				Matrix glMatrix = MatrixTranspose(t->model_matrix);
				rlMultMatrixf((float*)&glMatrix);
				DrawModel(*raw_model, { 0.0f, 0.0f, 0.0f }, 1.0f, col);
				rlPopMatrix();
			}
		}
	}

	void render_shadows(Vector3 focus) {
		s_ShadowsReady = false;
		if (!s_LightingEnabled) return;

		auto& reg = me::get_registry();

		// Same convention as render_world: the first directional light is the sun.
		auto& dir_pool = reg.view<me::components::DirectionalLightComponent>();
		if (dir_pool.size() == 0) return;
		auto& dl = dir_pool.components[0];
		if (!dl.cast_shadows) return;
		auto* lt = reg.try_get_component<me::components::TransformComponent>(dir_pool.entity_map[0]);
		if (!lt) return;

		// (Re)create the depth target at the light's configured resolution.
		const int res = std::clamp(dl.shadow_resolution, 256, 8192);
		if (s_ShadowMap.id == 0 || s_ShadowMapRes != res) {
			if (s_ShadowMap.id != 0) {
				rlUnloadTexture(s_ShadowMap.depth.id);
				rlUnloadFramebuffer(s_ShadowMap.id);
			}
			s_ShadowMap = load_depth_target(res, res);
			s_ShadowMapRes = res;
		}
		if (s_ShadowMap.id == 0) return;

		const float extent = std::max(dl.shadow_extent, 1.0f);

		// Light direction from the entity's pitch/yaw (must match render_world).
		float pitch = lt->rotation.x * DEG2RAD;
		float yaw = lt->rotation.y * DEG2RAD;
		Vector3 dir = Vector3Normalize({ std::cos(pitch) * std::sin(yaw), -std::sin(pitch), std::cos(pitch) * std::cos(yaw) });
		Vector3 up = (std::fabs(dir.y) > 0.99f) ? Vector3{ 0.0f, 0.0f, 1.0f } : Vector3{ 0.0f, 1.0f, 0.0f };

		// Snap the volume's center to whole shadow-map texels in light space, so
		// shadow edges don't shimmer while the camera (the focus) moves smoothly.
		Matrix light_basis = MatrixLookAt({ 0.0f, 0.0f, 0.0f }, dir, up);
		Vector3 f_ls = Vector3Transform(focus, light_basis);
		const float texel = extent / static_cast<float>(res);
		f_ls.x = std::floor(f_ls.x / texel) * texel;
		f_ls.y = std::floor(f_ls.y / texel) * texel;
		Vector3 center = Vector3Transform(f_ls, MatrixInvert(light_basis));

		Camera3D light_cam = { 0 };
		light_cam.position = Vector3Subtract(center, Vector3Scale(dir, extent * 2.0f));
		light_cam.target = center;
		light_cam.up = up;
		light_cam.fovy = extent; // for an ortho camera this is the volume height
		light_cam.projection = CAMERA_ORTHOGRAPHIC;

		// Generous depth range so tall casters above the focus still register.
		double saved_near = rlGetCullDistanceNear();
		double saved_far = rlGetCullDistanceFar();
		rlSetClipPlanes(0.05, extent * 6.0);

		BeginTextureMode(s_ShadowMap);
		ClearBackground(::Color{ 255, 255, 255, 255 }); // clears the depth attachment to 1.0
		BeginMode3D(light_cam);
		Matrix light_view = rlGetMatrixModelview();
		Matrix light_proj = rlGetMatrixProjection();
		draw_geometry(reg, s_DefaultShader, false); // depth only — cheapest path
		EndMode3D();
		EndTextureMode();

		rlSetClipPlanes(saved_near, saved_far);

		s_LightVP = MatrixMultiply(light_view, light_proj);
		s_ShadowsReady = true;
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

			// --- B. Directional Light ---
			int has_dir_light = 0;
			auto& dir_pool = reg.view<me::components::DirectionalLightComponent>();
			if (dir_pool.size() > 0) {
				me::entity::entity_id e = dir_pool.entity_map[0];
				if (auto* t = reg.try_get_component<me::components::TransformComponent>(e)) {
					auto& dl = dir_pool.components[0];
					has_dir_light = 1;

					float pitch = t->rotation.x * DEG2RAD;
					float yaw = t->rotation.y * DEG2RAD;
					Vector3 dir = { std::cos(pitch) * std::sin(yaw), -std::sin(pitch), std::cos(pitch) * std::cos(yaw) };
					dir = Vector3Normalize(dir);

					Vector3 col = { dl.color.r / 255.0f, dl.color.g / 255.0f, dl.color.b / 255.0f };
					SetShaderValue(s_LightingShader, s_DirLightDirLoc, &dir, SHADER_UNIFORM_VEC3);
					SetShaderValue(s_LightingShader, s_DirLightColorLoc, &col, SHADER_UNIFORM_VEC3);
					SetShaderValue(s_LightingShader, s_DirLightIntensityLoc, &dl.intensity, SHADER_UNIFORM_FLOAT);
				}
			}
			SetShaderValue(s_LightingShader, s_HasDirLightLoc, &has_dir_light, SHADER_UNIFORM_INT);

			// --- C. Shadow map (rendered earlier this frame by render_shadows) ---
			int shadows_on = (s_ShadowsReady && has_dir_light == 1) ? 1 : 0;
			SetShaderValue(s_LightingShader, s_ShadowsEnabledLoc, &shadows_on, SHADER_UNIFORM_INT);
			if (shadows_on) {
				SetShaderValueMatrix(s_LightingShader, s_LightVPLoc, s_LightVP);
				int res = s_ShadowMapRes;
				SetShaderValue(s_LightingShader, s_ShadowMapResLoc, &res, SHADER_UNIFORM_INT);

				// Bind the depth texture on a high slot: rlgl's own batched draws
				// only ever touch slot 0, so the binding survives the whole pass.
				rlEnableShader(s_LightingShader.id);
				int slot = 10;
				rlActiveTextureSlot(slot);
				rlEnableTexture(s_ShadowMap.depth.id);
				rlSetUniform(s_ShadowMapLoc, &slot, SHADER_UNIFORM_INT, 1);
				rlActiveTextureSlot(0);
			}
		}

		// ==========================================
		// 3. RENDER SCENE
		// ==========================================
		// (No grid here: the editor draws its own in edit mode, and exported
		// games should never show editor furniture.)
		BeginMode3D(rayCam);

		if (s_LightingEnabled) BeginShaderMode(s_LightingShader);
		draw_geometry(reg, s_LightingEnabled ? s_LightingShader : s_DefaultShader, true);
		if (s_LightingEnabled) EndShaderMode();

		EndMode3D();
	}

	bool is_lighting_enabled() { return s_LightingEnabled; }
	void set_lighting_enabled(bool enabled) { s_LightingEnabled = enabled; }

}
