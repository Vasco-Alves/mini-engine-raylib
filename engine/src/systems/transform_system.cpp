#include "mini-engine-raylib/systems/transform_system.hpp"
#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/ecs/components.hpp"
#include <mini-ecs/registry.hpp>
#include <raymath.h>

namespace me::systems {

	void update_transform_node(me::Registry& reg, me::entity::entity_id e, const Matrix& parent_matrix, bool parent_dirty) {
		auto* t = reg.try_get_component<me::components::TransformComponent>(e);
		if (!t) return;

		bool moved = !Vector3Equals(t->position, t->last_position);
		bool rotated = !Vector3Equals(t->rotation, t->last_rotation);
		bool scaled = !Vector3Equals(t->scale, t->last_scale);

		// If Euler angles were changed externally (e.g. via the inspector),
		// rebuild the quaternion from them so it stays in sync.
		if (rotated) {
			t->rotation_quat = QuaternionNormalize(QuaternionFromEuler(
				t->rotation.x * DEG2RAD,
				t->rotation.y * DEG2RAD,
				t->rotation.z * DEG2RAD
			));
		}

		// If the parent moved, children must recalculate their world position
		bool is_dirty = t->is_dirty || moved || rotated || scaled || parent_dirty;

		if (is_dirty) {
			Matrix matScale = MatrixScale(t->scale.x, t->scale.y, t->scale.z);

			// Use quaternion directly — avoids Euler round-trip drift during gizmo rotation
			Matrix matRot = QuaternionToMatrix(QuaternionNormalize(t->rotation_quat));

			Matrix matTrans = MatrixTranslate(t->position.x, t->position.y, t->position.z);

			// Local Space Matrix
			Matrix local_matrix = MatrixMultiply(MatrixMultiply(matScale, matRot), matTrans);

			// World Space Matrix (Local Space applied on top of Parent Space)
			t->model_matrix = MatrixMultiply(local_matrix, parent_matrix);

			t->last_position = t->position;
			t->last_rotation = t->rotation;
			t->last_scale = t->scale;
			t->is_dirty = false;
		}

		// Recursively crawl down the tree to the children
		for (auto child_id : t->children) {
			update_transform_node(reg, child_id, t->model_matrix, is_dirty);
		}
	}

	void transform_update() {
		auto& reg = me::get_registry();
		auto& transforms = reg.view<me::components::TransformComponent>();

		Matrix identity = MatrixIdentity();

		// Sweep 1: Find all Root Entities (Objects with no parent)
		for (size_t i = 0; i < transforms.size(); ++i) {
			auto e = transforms.entity_map[i];
			auto& t = transforms.components[i];

			if (t.parent == me::entity::null) {
				update_transform_node(reg, e, identity, false);
			}
		}
	}
}
