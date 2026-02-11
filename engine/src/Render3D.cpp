#include "Render3D.hpp"
#include "Components.hpp"
#include "Registry.hpp"

#include <raylib.h>
#include <rlgl.h> // specific OpenGL/Raylib low-level calls if needed

namespace me::render {

	inline ::Color ToRay(me::Color c) { return ::Color{ c.r, c.g, c.b, c.a }; }

	void ClearWorld(me::Color color) {
		ClearBackground(ToRay(color));
	}

	void RenderWorld() {
		auto& reg = me::detail::Reg();

		// 1. Find Active Camera
		Camera3D rayCam = { 0 };
		rayCam.position = { 10.0f, 10.0f, 10.0f }; // Fallback
		rayCam.target = { 0.0f, 0.0f, 0.0f };
		rayCam.up = { 0.0f, 1.0f, 0.0f };
		rayCam.fovy = 45.0f;
		rayCam.projection = CAMERA_PERSPECTIVE;

		// Search for an entity with Transform + Camera components
		auto* camPool = reg.TryGetPool<me::components::Camera>();
		if (camPool) {
			for (auto& kv : camPool->data) {
				if (kv.second.active) {
					auto* t = reg.TryGetComponent<me::components::Transform>(kv.first);
					if (t) {
						rayCam.position = { t->x, t->y, t->z };
						rayCam.target = { kv.second.targetX, kv.second.targetY, kv.second.targetZ };
						rayCam.up = { kv.second.upX, kv.second.upY, kv.second.upZ };
						rayCam.fovy = kv.second.fov;
						rayCam.projection = kv.second.projection;
						break; // Use first active camera found
					}
				}
			}
		}

		// 2. Render Scene
		BeginMode3D(rayCam);

		DrawGrid(20, 1.0f); // Helpful guide

		auto* meshPool = reg.TryGetPool<me::components::MeshRenderer>();
		if (meshPool) {
			for (auto& kv : meshPool->data) {
				auto* t = reg.TryGetComponent<me::components::Transform>(kv.first);
				if (!t) continue;

				Vector3 pos = { t->x, t->y, t->z };
				Vector3 size = { t->sx, t->sy, t->sz };
				::Color col = ToRay(kv.second.color);

				// Basic rotation handling (around Y axis mostly for now, or X/Z)
				// Note: Raylib's simple shapes don't support full Euler rotation easily without matrices.
				// For now, we apply basic transforms.
				rlPushMatrix();
				rlTranslatef(pos.x, pos.y, pos.z);
				rlRotatef(t->rotZ, 0, 0, 1);
				rlRotatef(t->rotY, 0, 1, 0);
				rlRotatef(t->rotX, 1, 0, 0);
				rlScalef(size.x, size.y, size.z);

				if (kv.second.type == me::components::MeshRenderer::Cube) {
					if (kv.second.wireframe) DrawCubeWires({ 0,0,0 }, 2.0f, 2.0f, 2.0f, col); // Raylib Cube is size 2 centered
					else DrawCube({ 0,0,0 }, 2.0f, 2.0f, 2.0f, col);

				} else if (kv.second.type == me::components::MeshRenderer::Sphere) {
					if (kv.second.wireframe) DrawSphereWires({ 0,0,0 }, 1.0f, 16, 16, col);

					else DrawSphere({ 0,0,0 }, 1.0f, col);

				} else if (kv.second.type == me::components::MeshRenderer::Plane) {
					DrawPlane({ 0,0,0 }, { 2.0f, 2.0f }, col);
				}

				rlPopMatrix();
			}
		}

		EndMode3D();
	}
}