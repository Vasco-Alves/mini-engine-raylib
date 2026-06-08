#include "mini-engine-raylib/systems/transform_system.hpp"
#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/ecs/components.hpp"
#include <mini-ecs/registry.hpp>
#include <raymath.h>

namespace me::systems {

	void update_transform_node(me::Registry& reg, me::entity::entity_id e, const Matrix& parent_matrix, bool parent_dirty) {
		auto* t = reg.try_get_component<me::components::TransformComponent>(e);
		if (!t) return;

		bool pos_changed = !Vector3Equals(t->position, t->last_position);
		bool scl_changed = !Vector3Equals(t->scale, t->last_scale);
		bool eul_changed = !Vector3Equals(t->rotation, t->last_rotation);
		bool quat_changed = !QuaternionEquals(t->rotation_quat, t->last_rotation_quat);

		// ==========================================
		// 1. BI-DIRECTIONAL SYNC LOGIC
		// ==========================================
		if (eul_changed) {
			// Inspector update: User typed Euler angles -> Update Quaternion
			t->rotation_quat = QuaternionNormalize(QuaternionFromEuler(
				t->rotation.x * DEG2RAD,
				t->rotation.y * DEG2RAD,
				t->rotation.z * DEG2RAD
			));
		} else if (quat_changed) {
			// Gizmo update: User rotated via Quaternion -> Update Euler for the Inspector
			Vector3 euler = QuaternionToEuler(t->rotation_quat);
			t->rotation = { euler.x * RAD2DEG, euler.y * RAD2DEG, euler.z * RAD2DEG };
		}

		// ==========================================
		// 2. DIRTY CHECK & MATRIX REBUILD
		// ==========================================
		bool is_dirty = t->is_dirty || pos_changed || eul_changed || quat_changed || scl_changed || parent_dirty;

		if (is_dirty) {
			Matrix matScale = MatrixScale(t->scale.x, t->scale.y, t->scale.z);

			// Use quaternion directly for matrix — avoids Euler round-trip drift
			Matrix matRot = QuaternionToMatrix(QuaternionNormalize(t->rotation_quat));

			Matrix matTrans = MatrixTranslate(t->position.x, t->position.y, t->position.z);

			// Local Space Matrix (Order: Scale * Rotation * Translation)
			Matrix local_matrix = MatrixMultiply(MatrixMultiply(matScale, matRot), matTrans);

			// World Space Matrix (Local Space applied on top of Parent Space)
			t->model_matrix = MatrixMultiply(local_matrix, parent_matrix);

			// Cache state so we don't recalculate next frame unless something moves
			t->last_position = t->position;
			t->last_rotation = t->rotation;
			t->last_scale = t->scale;
			t->last_rotation_quat = t->rotation_quat;
			t->is_dirty = false;
		}

		// ==========================================
		// 3. RECURSIVE HIERARCHY CRAWL
		// ==========================================
		// Pass down the is_dirty flag so children know they need to update if the parent moved
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
				// Updating the root node will automatically recurse and update all its children
				update_transform_node(reg, e, identity, false);
			}
		}
	}
}