#pragma once

#include "raytracing_math.hpp"
#include <mini-ecs/registry.hpp>
#include <mini-engine-raylib/ecs/components.hpp>

#include <vector>
#include <optional>
#include <algorithm>
#include <numeric>
#include <cstdint>
#include <cmath>
#include <cfloat>

namespace me::raytracing {

	// ============================================================
	// AABB CONSTRUCTION — one helper per primitive type
	// ============================================================
	namespace detail {

		// Transforms all 8 unit-cube corners through model_matrix → exact OBB→AABB
		inline AABB aabb_from_cube(const Matrix& m) {
			constexpr Vector3 corners[8] = {
				{-1,-1,-1},{1,-1,-1},{-1,1,-1},{1,1,-1},
				{-1,-1, 1},{1,-1, 1},{-1,1, 1},{1,1, 1}
			};
			AABB box;
			for (auto& c : corners) box.grow(Vector3Transform(c, m));
			return box;
		}

		// Sphere: centre ± radius on every axis
		inline AABB aabb_from_sphere(const Vector3& centre, float radius) {
			return AABB{
				{ centre.x - radius, centre.y - radius, centre.z - radius },
				{ centre.x + radius, centre.y + radius, centre.z + radius }
			};
		}

		// Plane: a large finite slab so SAH cost doesn't blow up.
		// The slab is thin in the normal direction and huge in the tangent directions.
		inline AABB aabb_from_plane(const Vector3& centre, const Quaternion& rot) {
			constexpr float HALF_EXTENT = 1000.0f;
			constexpr float THICKNESS = 0.01f;

			constexpr Vector3 corners[4] = {
				{-HALF_EXTENT, 0.0f, -HALF_EXTENT},
				{ HALF_EXTENT, 0.0f, -HALF_EXTENT},
				{-HALF_EXTENT, 0.0f,  HALF_EXTENT},
				{ HALF_EXTENT, 0.0f,  HALF_EXTENT}
			};
			AABB box;
			for (auto& c : corners) {
				Vector3 w = Vector3RotateByQuaternion(c, rot);
				box.grow(Vector3{ w.x + centre.x, w.y + centre.y, w.z + centre.z });
			}
			Vector3 n = Vector3RotateByQuaternion({ 0,1,0 }, rot);
			box.min.x -= std::abs(n.x) * THICKNESS;  box.max.x += std::abs(n.x) * THICKNESS;
			box.min.y -= std::abs(n.y) * THICKNESS;  box.max.y += std::abs(n.y) * THICKNESS;
			box.min.z -= std::abs(n.z) * THICKNESS;  box.max.z += std::abs(n.z) * THICKNESS;
			return box;
		}

		inline AABB compute_aabb(const me::components::TransformComponent& t,
			const me::components::Shape3DComponent& s) {
			switch (s.type) {
			case me::components::Shape3DComponent::Cube:   return aabb_from_cube(t.model_matrix);
			case me::components::Shape3DComponent::Sphere: return aabb_from_sphere(t.position, t.scale.x);
			case me::components::Shape3DComponent::Plane:  return aabb_from_plane(t.position, t.rotation_quat);
			}
			return {};
		}

	} // namespace detail

	// ============================================================
	// BVHNode
	// ============================================================
	struct BVHNode {
		AABB     bounds;

		// 0 = leaf, > 0 = index of the left child (right child is always left+1)
		uint32_t left_child = 0;

		// Leaf data — "triangle" == primitive entity for now.
		uint32_t first_triangle_index = 0;
		uint32_t triangle_count = 0; // 0 means internal node

		bool is_leaf()    const { return triangle_count > 0; }
		uint32_t right_child() const { return left_child + 1; }
	};

	// ============================================================
	// BVH
	// ============================================================
	class BVH {
	public:
		// ----------------------------------------------------------
		// Build
		// Scans the registry for every entity with Shape3D+Transform,
		// then partitions them with the Surface Area Heuristic.
		// ----------------------------------------------------------
		void build(me::Registry& registry) {
			m_nodes.clear();
			m_prim_indices.clear();
			m_prims.clear();

			// Gather primitives
			auto& transforms = registry.view<me::components::TransformComponent>();
			for (size_t i = 0; i < transforms.size(); ++i) {
				me::entity::entity_id e = transforms.entity_map[i];
				auto* shape = registry.try_get_component<me::components::Shape3DComponent>(e);
				if (!shape) continue;

				m_prims.push_back({ e, detail::compute_aabb(transforms.components[i], *shape) });
			}

			if (m_prims.empty()) return;

			// Build an index array so we can sort without touching the prim data
			m_prim_indices.resize(m_prims.size());
			std::iota(m_prim_indices.begin(), m_prim_indices.end(), 0u);

			m_nodes.reserve(m_prims.size() * 2);

			// THE FIX: Push the Root Node at Index 0 before starting recursion!
			m_nodes.push_back({});
			subdivide(0, 0, static_cast<uint32_t>(m_prim_indices.size()));
		}

		// ----------------------------------------------------------
		// Traverse
		// Returns the closest HitPayload within max_dist, or nullopt.
		// ----------------------------------------------------------
		std::optional<HitPayload> traverse(
			const Vector3& origin,
			const Vector3& dir,
			const Vector3& inv_dir,
			float           max_dist,
			me::Registry& registry) const {
			if (m_nodes.empty()) return std::nullopt;

			HitPayload best{};
			best.hit_distance = max_dist;
			bool found = false;

			// Explicit stack — avoids recursion overhead, keeps hot data in registers
			uint32_t stack[64];
			int      top = 0;
			stack[top++] = 0; // push root

			while (top > 0) {
				const BVHNode& node = m_nodes[stack[--top]];

				if (node.bounds.intersect(origin, inv_dir, best.hit_distance) == FLT_MAX)
					continue; // whole subtree is behind us or farther than best hit

				if (node.is_leaf()) {
					for (uint32_t i = node.first_triangle_index;
						i < node.first_triangle_index + node.triangle_count; ++i) {
						if (auto h = test_primitive(m_prim_indices[i], origin, dir,
							best.hit_distance, registry)) {
							best = *h;
							found = true;
						}
					}
				} else {
					// Visit the nearer child first so we find a close hit quickly
					// and prune more of the tree on subsequent nodes.
					float tL = m_nodes[node.left_child].bounds.intersect(origin, inv_dir, best.hit_distance);
					float tR = m_nodes[node.right_child()].bounds.intersect(origin, inv_dir, best.hit_distance);

					if (tL != FLT_MAX && tR != FLT_MAX) {
						// Both children are candidates — push far one first
						if (tL <= tR) {
							stack[top++] = node.right_child();
							stack[top++] = node.left_child;
						} else {
							stack[top++] = node.left_child;
							stack[top++] = node.right_child();
						}
					} else if (tL != FLT_MAX) stack[top++] = node.left_child;
					else if (tR != FLT_MAX) stack[top++] = node.right_child();
				}
			}

			return found ? std::optional<HitPayload>(best) : std::nullopt;
		}

		bool empty() const { return m_nodes.empty(); }

	private:
		// ----------------------------------------------------------
		// Internal primitive record
		// ----------------------------------------------------------
		struct Prim {
			me::entity::entity_id entity;
			AABB                  aabb;
		};

		std::vector<BVHNode>  m_nodes;
		std::vector<uint32_t> m_prim_indices; // sorted during build, stable after
		std::vector<Prim>     m_prims;

		// ----------------------------------------------------------
		// Build helpers
		// ----------------------------------------------------------

		void subdivide(uint32_t node_idx, uint32_t first, uint32_t count) {

			// Compute the bounding box for this set
			for (uint32_t i = first; i < first + count; ++i)
				m_nodes[node_idx].bounds.grow(m_prims[m_prim_indices[i]].aabb);

			// Leaf: 4 or fewer primitives
			if (count <= 4) {
				m_nodes[node_idx].first_triangle_index = first;
				m_nodes[node_idx].triangle_count = count;
				return;
			}

			// ---- SAH: find best axis + split ----
			int      best_axis = -1;
			uint32_t best_split = 0;
			float    best_cost = FLT_MAX;

			for (int axis = 0; axis < 3; ++axis) {
				// Sort indices along this axis
				std::sort(m_prim_indices.begin() + first,
					m_prim_indices.begin() + first + count,
					[&](uint32_t a, uint32_t b) {
						auto ca = m_prims[a].aabb.centroid();
						auto cb = m_prims[b].aabb.centroid();
						return (&ca.x)[axis] < (&cb.x)[axis];
					});

				// Left-to-right and right-to-left prefix AABBs
				std::vector<AABB> left_box(count), right_box(count);
				AABB run{};
				for (uint32_t i = 0; i < count; ++i) {
					run.grow(m_prims[m_prim_indices[first + i]].aabb);
					left_box[i] = run;
				}
				run = {};
				for (int i = (int)count - 1; i >= 0; --i) {
					run.grow(m_prims[m_prim_indices[first + i]].aabb);
					right_box[i] = run;
				}

				// Evaluate each candidate split position
				float parent_sa = m_nodes[node_idx].bounds.surface_area();
				if (parent_sa <= 0.0f) parent_sa = 1.0f; // guard for degenerate boxes

				for (uint32_t split = 1; split < count; ++split) {
					float cost = (left_box[split - 1].surface_area() * split
						+ right_box[split].surface_area() * (count - split))
						/ parent_sa;

					if (cost < best_cost) {
						best_cost = cost;
						best_axis = axis;
						best_split = split;
					}
				}
			}

			// If a leaf is cheaper than any split, make one
			constexpr float LEAF_PRIM_COST = 1.0f;
			if (best_axis < 0 || best_cost >= count * LEAF_PRIM_COST) {
				m_nodes[node_idx].first_triangle_index = first;
				m_nodes[node_idx].triangle_count = count;
				return;
			}

			// Re-sort on the winning axis (last sort was on the last axis tried)
			std::sort(m_prim_indices.begin() + first,
				m_prim_indices.begin() + first + count,
				[&](uint32_t a, uint32_t b) {
					auto ca = m_prims[a].aabb.centroid();
					auto cb = m_prims[b].aabb.centroid();
					return (&ca.x)[best_axis] < (&cb.x)[best_axis];
				});

			// Allocate both children simultaneously so they are contiguous in memory
			uint32_t left_child_idx = static_cast<uint32_t>(m_nodes.size());
			m_nodes.push_back({}); // Allocate Left Child
			m_nodes.push_back({}); // Allocate Right Child

			// Assign the left child index to the current node
			m_nodes[node_idx].left_child = left_child_idx;

			// Recurse using the newly allocated node indices
			subdivide(left_child_idx, first, best_split);
			subdivide(left_child_idx + 1, first + best_split, count - best_split);
		}

		// ----------------------------------------------------------
		// Primitive intersection
		// ----------------------------------------------------------
		std::optional<HitPayload> test_primitive(
			uint32_t       prim_idx,
			const Vector3& origin,
			const Vector3& dir,
			float          max_dist,
			me::Registry& registry) const {
			const Prim& prim = m_prims[prim_idx];
			auto* t = registry.try_get_component<me::components::TransformComponent>(prim.entity);
			auto* shape = registry.try_get_component<me::components::Shape3DComponent>(prim.entity);
			if (!t || !shape) return std::nullopt;

			float   t_hit = FLT_MAX;
			Vector3 normal = {};

			switch (shape->type) {
			case me::components::Shape3DComponent::Sphere: {
				if (intersect_sphere(origin, dir, t->position, t->scale.x, t_hit)) {
					if (t_hit >= max_dist) return std::nullopt;
					Vector3 hp = Vector3Add(origin, Vector3Scale(dir, t_hit));
					normal = Vector3Normalize(Vector3Subtract(hp, t->position));
				} else return std::nullopt;
				break;
			}
			case me::components::Shape3DComponent::Plane: {
				Vector3 pn = Vector3RotateByQuaternion({ 0,1,0 }, t->rotation_quat);
				if (intersect_plane(origin, dir, t->position, pn, t_hit)) {
					if (t_hit >= max_dist) return std::nullopt;
					normal = Vector3DotProduct(pn, dir) < 0.0f ? pn : Vector3Scale(pn, -1.0f);
				} else return std::nullopt;
				break;
			}
			case me::components::Shape3DComponent::Cube: {
				if (intersect_obb(origin, dir, t->model_matrix, t_hit, normal)) {
					if (t_hit >= max_dist) return std::nullopt;
				} else return std::nullopt;
				break;
			}
			}

			HitPayload p{};
			p.hit_distance = t_hit;
			p.hit_point = Vector3Add(origin, Vector3Scale(dir, t_hit));
			p.normal = normal;
			p.emission = { 0.0f, 0.0f, 0.0f };

			auto* mat = registry.try_get_component<me::components::MaterialComponent>(prim.entity);
			if (mat) {
				p.base_color = { mat->albedo.r / 255.0f, mat->albedo.g / 255.0f, mat->albedo.b / 255.0f };
				p.roughness = mat->roughness;
				p.emission = {
					p.base_color.x * mat->emission_power,
					p.base_color.y * mat->emission_power,
					p.base_color.z * mat->emission_power
				};
			} else {
				p.base_color = { shape->color.r / 255.0f, shape->color.g / 255.0f, shape->color.b / 255.0f };
				p.roughness = 1.0f;
			}

			return p;
		}
	};

} // namespace me::raytracing