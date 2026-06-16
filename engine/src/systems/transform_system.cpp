#include "mini-engine-raylib/systems/transform_system.hpp"
#include "mini-engine-raylib/core/engine.hpp"
#include "mini-engine-raylib/core/logger.hpp"
#include "mini-engine-raylib/ecs/components.hpp"
#include <mini-ecs/registry.hpp>
#include <raymath.h>

#include <unordered_set>

namespace me::systems {

	namespace {
		// Guards the hierarchy crawl against cycles in corrupt or hand-edited
		// scene files (A parenting B parenting A would recurse forever and
		// overflow the stack). Static so the set's capacity is reused each frame.
		std::unordered_set<me::entity::entity_id> s_visited;
		bool s_cycle_warned = false;
	}

	void update_transform_node(me::Registry& reg, me::entity::entity_id e, const Matrix& parent_matrix, bool parent_dirty) {
		if (!s_visited.insert(e).second) {
			if (!s_cycle_warned) {
				me::logger::warn("Transform hierarchy contains a cycle - breaking the loop (check the scene file).");
				s_cycle_warned = true;
			}
			return;
		}

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

		Matrix identity = MatrixIdentity();
		s_visited.clear();

		// Sweep 1: roots — no parent, or a parent that doesn't resolve (a
		// corrupt file or a stale id). Treating broken parents as roots keeps
		// those entities updating and visible instead of frozen forever.
		for (auto [e, t] : reg.view<me::components::TransformComponent>()) {
			bool parent_resolves = t.parent != me::entity::null
				&& reg.try_get_component<me::components::TransformComponent>(t.parent) != nullptr;
			if (!parent_resolves) {
				// Updating the root node will automatically recurse and update all its children
				update_transform_node(reg, e, identity, false);
			}
		}

		// Sweep 2: anything still unvisited sits in a parent cycle with no
		// root (e.g. A and B parenting each other). Update each standalone so
		// the entities stay visible and editable rather than disappearing.
		for (auto [e, t] : reg.view<me::components::TransformComponent>()) {
			(void)t;
			if (!s_visited.contains(e)) {
				if (!s_cycle_warned) {
					me::logger::warn("Transform hierarchy contains a cycle - breaking the loop (check the scene file).");
					s_cycle_warned = true;
				}
				update_transform_node(reg, e, identity, false);
			}
		}
	}
}