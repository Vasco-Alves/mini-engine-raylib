#include "mini-engine-raylib/render/renderer.hpp"
#include "mini-engine-raylib/ecs/components.hpp"
#include "mini-engine-raylib/core/engine.hpp" // For me::get_registry()

#include <raylib.h>
#include <rlgl.h>

namespace me::render {

	inline ::Color to_ray(me::Color c) { return ::Color{ c.r, c.g, c.b, c.a }; }

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
			me::EntityId e = camPool.entity_map[i];
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
			me::EntityId e = meshPool.entity_map[i];
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
}