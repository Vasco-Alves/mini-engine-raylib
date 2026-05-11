#include "mini-engine-raylib/systems/transform_system.hpp"
#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/ecs/components.hpp"

#include <mini-ecs/registry.hpp>

#include <raymath.h>

namespace me::systems {

	void transform_update() {
		auto& reg = me::get_registry();
		auto& transforms = reg.view<me::components::TransformComponent>();

		for (size_t i = 0; i < transforms.size(); ++i) {
			auto& t = transforms.components[i];

			// USE RAYMATH EQUALITY CHECKS!
			bool moved = !Vector3Equals(t.position, t.last_position);
			bool rotated = !Vector3Equals(t.rotation, t.last_rotation);
			bool scaled = !Vector3Equals(t.scale, t.last_scale);

			if (t.is_dirty || moved || rotated || scaled) {

				Matrix matScale = MatrixScale(t.scale.x, t.scale.y, t.scale.z);
				Vector3 radRot = { t.rotation.x * DEG2RAD, t.rotation.y * DEG2RAD, t.rotation.z * DEG2RAD };
				Matrix matRot = MatrixRotateXYZ(radRot);
				Matrix matTrans = MatrixTranslate(t.position.x, t.position.y, t.position.z);

				Matrix rotScale = MatrixMultiply(matRot, matScale);
				t.model_matrix = MatrixMultiply(matTrans, rotScale);

				t.last_position = t.position;
				t.last_rotation = t.rotation;
				t.last_scale = t.scale;
				t.is_dirty = false;
			}
		}
	}
}