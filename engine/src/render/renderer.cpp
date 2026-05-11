#include "mini-engine-raylib/render/renderer.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include "../assets/assets_internal.hpp"
#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/ecs/components.hpp"

#include <mini-ecs/registry.hpp>

namespace me::render {

	inline static ::Color to_ray(me::Color c) {
		return ::Color{ c.r, c.g, c.b, c.a };
	}

	void clear_world(me::Color color) {
		ClearBackground(to_ray(color));
	}

	void render_world(const me::components::TransformComponent* override_transform,
		const me::components::CameraComponent* override_cam) {
		auto& reg = me::get_registry();

		Camera3D rayCam = { 0 };
		rayCam.position = { 10.0f, 10.0f, 10.0f };
		rayCam.target = { 0.0f, 0.0f, 0.0f };
		rayCam.up = { 0.0f, 1.0f, 0.0f };
		rayCam.fovy = 45.0f;
		rayCam.projection = CAMERA_PERSPECTIVE;

		// 1. WHICH CAMERA ARE WE USING?
		if (override_transform && override_cam) {
			// A. Use the Editor's private camera
			rayCam.position = { override_transform->position.x, override_transform->position.y, override_transform->position.z };
			rayCam.target = { override_cam->target.x, override_cam->target.y, override_cam->target.z };
			rayCam.up = { override_cam->up.x, override_cam->up.y, override_cam->up.z };
			rayCam.fovy = override_cam->fov;
			rayCam.projection = override_cam->projection;
		} else {
			// B. Fallback to searching the ECS (for Shipped Game / Play Mode)
			auto& camPool = reg.view<me::components::CameraComponent>();
			for (size_t i = 0; i < camPool.size(); ++i) {
				me::entity::entity_id e = camPool.entity_map[i];
				auto& cam = camPool.components[i];

				if (cam.active) {
					auto* t = reg.try_get_component<me::components::TransformComponent>(e);
					if (t) {
						rayCam.position = { t->position.x, t->position.y, t->position.z }; // Fixed typo here!
						rayCam.target = { cam.target.x, cam.target.y, cam.target.z };
						rayCam.up = { cam.up.x, cam.up.y, cam.up.z };
						rayCam.fovy = cam.fov;
						rayCam.projection = cam.projection;
						break;
					}
				}
			}
		}

		BeginMode3D(rayCam);
		DrawGrid(20, 1.0f);

		// --- DRAW 3D PRIMITIVES ---
		auto& meshPool = reg.view<me::components::Shape3DComponent>();

		for (size_t i = 0; i < meshPool.size(); ++i) {
			me::entity::entity_id e = meshPool.entity_map[i];
			auto& mesh = meshPool.components[i];

			auto* t = reg.try_get_component<me::components::TransformComponent>(e);
			if (!t) continue;

			::Color col = to_ray(mesh.color);

			rlPushMatrix();

			rlMultMatrixf((float*)&t->model_matrix);

			// Transpose the matrix to match OpenGL's memory layout!
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

		for (size_t i = 0; i < modelPool.size(); ++i) {
			me::entity::entity_id e = modelPool.entity_map[i];
			auto& modComp = modelPool.components[i];

			auto* t = reg.try_get_component<me::components::TransformComponent>(e);
			if (!t) continue;

			const ::Model* model = me::assets::internal_get_model(modComp.model);
			if (model) {
				::Color col = to_ray(modComp.tint);

				rlPushMatrix();

				// Inject our optimized ECS Matrix Cache directly into OpenGL!
				Matrix glMatrix = MatrixTranspose(t->model_matrix);
				rlMultMatrixf((float*)&glMatrix);

				// Draw the model at 0,0,0 because the Matrix handles all the movement!
				DrawModel(*model, { 0.0f, 0.0f, 0.0f }, 1.0f, col);

				rlPopMatrix();
			}
		}

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

			// Note: Raylib handles 2D drawing from the top-left for Rects, and center for Circles.
			// You use the Transform's position and scale to drive it!
			if (shape.type == me::components::Shape2DComponent::Rectangle) {
				::Rectangle rect = { t->position.x, t->position.y, t->scale.x, t->scale.y };

				// Optional: Support 2D rotation
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

			// Fetch the raw Raylib Texture
			const ::Texture2D* tex = me::assets::internal_get_texture(sprite.texture);
			if (tex) {
				// Source rect (entire image)
				::Rectangle source = { 0.0f, 0.0f, (float)tex->width, (float)tex->height };

				// Destination rect (Position and Scaled Size)
				::Rectangle dest = { t->position.x, t->position.y, tex->width * t->scale.x, tex->height * t->scale.y };

				// Origin is the center of the sprite so it rotates correctly
				::Vector2 origin = { dest.width / 2.0f, dest.height / 2.0f };

				// Draw it using Transform's rot_z for 2D rotation
				DrawTexturePro(*tex, source, dest, origin, t->rotation.z, to_ray(sprite.tint));
			}
		}

		EndMode2D();
	}

}
