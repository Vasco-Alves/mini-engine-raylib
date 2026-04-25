#include "../assets/assets_internal.hpp"

#include <mini-ecs/registry.hpp>

#include "mini-engine-raylib/render/renderer.hpp"
#include "mini-engine-raylib/ecs/components.hpp"
#include "mini-engine-raylib/core/engine.hpp"

#include <raylib.h>
#include <rlgl.h>

namespace me::render {

	inline ::Color to_ray(me::Color c) {
		return ::Color{ c.r, c.g, c.b, c.a };
	}

	void clear_world(me::Color color) {
		ClearBackground(to_ray(color));
	}

	void render_world() {
		auto& reg = me::get_registry();

		Camera3D rayCam = { 0 };
		rayCam.position = { 10.0f, 10.0f, 10.0f };
		rayCam.target = { 0.0f, 0.0f, 0.0f };
		rayCam.up = { 0.0f, 1.0f, 0.0f };
		rayCam.fovy = 45.0f;
		rayCam.projection = CAMERA_PERSPECTIVE;

		auto& camPool = reg.view<me::components::CameraComponent>();

		for (size_t i = 0; i < camPool.size(); ++i) {
			me::entity::entity_id e = camPool.entity_map[i];
			auto& cam = camPool.components[i];

			if (cam.active) {
				auto* t = reg.try_get_component<me::components::TransformComponent>(e);
				if (t) {
					rayCam.position = { t->x, t->y, t->z };
					rayCam.target = { cam.target_x, cam.target_y, cam.target_z };
					rayCam.up = { cam.up_x, cam.up_y, cam.up_z };
					rayCam.fovy = cam.fov;
					rayCam.projection = cam.projection;
					break;
				}
			}
		}

		BeginMode3D(rayCam);
		DrawGrid(20, 1.0f);

		auto& meshPool = reg.view<me::components::MeshRendererComponent>();

		for (size_t i = 0; i < meshPool.size(); ++i) {
			me::entity::entity_id e = meshPool.entity_map[i];
			auto& mesh = meshPool.components[i];

			auto* t = reg.try_get_component<me::components::TransformComponent>(e);
			if (!t) continue;

			Vector3 pos = { t->x, t->y, t->z };
			Vector3 size = { t->sx, t->sy, t->sz };
			::Color col = to_ray(mesh.color);

			rlPushMatrix();
			rlTranslatef(pos.x, pos.y, pos.z);
			rlRotatef(t->rot_z, 0, 0, 1);
			rlRotatef(t->rot_y, 0, 1, 0);
			rlRotatef(t->rot_x, 1, 0, 0);
			rlScalef(size.x, size.y, size.z);

			if (mesh.type == me::components::MeshRendererComponent::Cube) {
				if (mesh.wireframe) DrawCubeWires({ 0,0,0 }, 2.0f, 2.0f, 2.0f, col);
				else DrawCube({ 0,0,0 }, 2.0f, 2.0f, 2.0f, col);

			} else if (mesh.type == me::components::MeshRendererComponent::Sphere) {
				if (mesh.wireframe) DrawSphereWires({ 0,0,0 }, 1.0f, 16, 16, col);
				else DrawSphere({ 0,0,0 }, 1.0f, col);

			} else if (mesh.type == me::components::MeshRendererComponent::Plane) {
				DrawPlane({ 0,0,0 }, { 2.0f, 2.0f }, col);
			}

			rlPopMatrix();
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
					ray_cam2d.target = { t->x, t->y }; // Camera follows Transform's X/Y
					ray_cam2d.offset = { cam.offset_x, cam.offset_y };
					ray_cam2d.rotation = cam.rotation;
					ray_cam2d.zoom = cam.zoom;
					break;
				}
			}
		}

		// 2. Start Drawing 2D World
		BeginMode2D(ray_cam2d);

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
				::Rectangle dest = { t->x, t->y, tex->width * t->sx, tex->height * t->sy };

				// Origin is the center of the sprite so it rotates correctly
				::Vector2 origin = { dest.width / 2.0f, dest.height / 2.0f };

				// Draw it using Transform's rot_z for 2D rotation
				DrawTexturePro(*tex, source, dest, origin, t->rot_z, to_ray(sprite.tint));
			}
		}

		EndMode2D();
	}

}