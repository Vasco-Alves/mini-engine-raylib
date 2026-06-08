#pragma once

#include "raytracing_math.hpp"
#include <mini-ecs/registry.hpp>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/assets/assets.hpp>

#include <vector>
#include <optional>
#include <algorithm>
#include <numeric>
#include <cstdint>
#include <cmath>
#include <cfloat>

// 1. FORWARD DECLARE THE BLAS CLASS
namespace me::raytracing {
	class TriangleBVH;
}

// 2. FORWARD DECLARE THE INTERNAL ASSET FUNCTION 
// (This tricks the compiler so we don't need to include the private src header!)
namespace me::assets {
	const me::raytracing::TriangleBVH* internal_get_model_bvh(ModelId id);
}

namespace me::raytracing {

	// ============================================================
	// 2. BLAS: Triangle BVH (Upgraded to an actual Tree!)
	// ============================================================
	struct TriangleNode {
		AABB     bounds;
		uint32_t left_child = 0;
		uint32_t first_triangle = 0;
		uint32_t triangle_count = 0;

		bool is_leaf() const { return triangle_count > 0; }
	};

	class TriangleBVH {
	public:
		AABB local_bounds;

		void build_from_model(const ::Model& model) {
			m_triangles.clear();
			m_nodes.clear();
			m_triangle_indices.clear();
			local_bounds = AABB();

			// 1. Extract Triangles
			for (int m = 0; m < model.meshCount; m++) {
				const ::Mesh& mesh = model.meshes[m];
				int triangle_count = mesh.triangleCount;
				float* vertices = mesh.vertices;
				unsigned short* indices = mesh.indices;

				for (int i = 0; i < triangle_count; i++) {
					Triangle t;
					if (indices != nullptr) {
						int i0 = indices[i * 3 + 0] * 3; int i1 = indices[i * 3 + 1] * 3; int i2 = indices[i * 3 + 2] * 3;
						t.v0 = { vertices[i0], vertices[i0 + 1], vertices[i0 + 2] };
						t.v1 = { vertices[i1], vertices[i1 + 1], vertices[i1 + 2] };
						t.v2 = { vertices[i2], vertices[i2 + 1], vertices[i2 + 2] };
					} else {
						int i0 = (i * 3 + 0) * 3; int i1 = (i * 3 + 1) * 3; int i2 = (i * 3 + 2) * 3;
						t.v0 = { vertices[i0], vertices[i0 + 1], vertices[i0 + 2] };
						t.v1 = { vertices[i1], vertices[i1 + 1], vertices[i1 + 2] };
						t.v2 = { vertices[i2], vertices[i2 + 1], vertices[i2 + 2] };
					}

					t.v0 = Vector3Transform(t.v0, model.transform);
					t.v1 = Vector3Transform(t.v1, model.transform);
					t.v2 = Vector3Transform(t.v2, model.transform);

					local_bounds.grow(t.v0);
					local_bounds.grow(t.v1);
					local_bounds.grow(t.v2);

					m_triangles.push_back(t);
				}
			}

			if (m_triangles.empty()) return;

			// 2. Setup Indices for Sorting
			m_triangle_indices.resize(m_triangles.size());
			std::iota(m_triangle_indices.begin(), m_triangle_indices.end(), 0u);

			// 3. Build the Tree
			m_nodes.reserve(m_triangles.size() * 2);
			m_nodes.push_back({}); // Root node
			subdivide(0, 0, static_cast<uint32_t>(m_triangle_indices.size()));
		}

		// THE NEW FAST TRAVERSAL (Replaces brute_force_intersect)
		bool intersect(const Vector3& local_origin, const Vector3& local_dir, float& out_t, Vector3& out_normal) const {
			if (m_nodes.empty()) return false;

			Vector3 inv_dir = {
				1.0f / (std::abs(local_dir.x) > 1e-8f ? local_dir.x : 1e-8f),
				1.0f / (std::abs(local_dir.y) > 1e-8f ? local_dir.y : 1e-8f),
				1.0f / (std::abs(local_dir.z) > 1e-8f ? local_dir.z : 1e-8f)
			};

			float closest_t = FLT_MAX;
			bool hit = false;

			uint32_t stack[64];
			int top = 0;
			stack[top++] = 0;

			while (top > 0) {
				const TriangleNode& node = m_nodes[stack[--top]];

				// Check if the ray hits this box at all
				if (node.bounds.intersect(local_origin, inv_dir, closest_t) == FLT_MAX) {
					continue;
				}

				if (node.is_leaf()) {
					// We reached the bottom! Test the 1 or 2 triangles inside this tiny box.
					for (uint32_t i = node.first_triangle; i < node.first_triangle + node.triangle_count; ++i) {
						const Triangle& tri = m_triangles[m_triangle_indices[i]];
						float t, u, v;
						if (intersect_triangle(local_origin, local_dir, tri.v0, tri.v1, tri.v2, t, u, v)) {
							if (t > 0.001f && t < closest_t) {
								closest_t = t;
								hit = true;
								Vector3 edge1 = Vector3Subtract(tri.v1, tri.v0);
								Vector3 edge2 = Vector3Subtract(tri.v2, tri.v0);
								out_normal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));
							}
						}
					}
				} else {
					// Push children (closest first for early pruning)
					float tL = m_nodes[node.left_child].bounds.intersect(local_origin, inv_dir, closest_t);
					float tR = m_nodes[node.left_child + 1].bounds.intersect(local_origin, inv_dir, closest_t);

					if (tL != FLT_MAX && tR != FLT_MAX) {
						if (tL <= tR) { stack[top++] = node.left_child + 1; stack[top++] = node.left_child; } else { stack[top++] = node.left_child; stack[top++] = node.left_child + 1; }
					} else if (tL != FLT_MAX) stack[top++] = node.left_child;
					else if (tR != FLT_MAX) stack[top++] = node.left_child + 1;
				}
			}

			if (hit) out_t = closest_t;
			return hit;
		}

	private:
		std::vector<Triangle> m_triangles;
		std::vector<TriangleNode> m_nodes;
		std::vector<uint32_t> m_triangle_indices;

		// Helper to get the center point of a triangle
		Vector3 get_centroid(const Triangle& t) const {
			return { (t.v0.x + t.v1.x + t.v2.x) / 3.0f, (t.v0.y + t.v1.y + t.v2.y) / 3.0f, (t.v0.z + t.v1.z + t.v2.z) / 3.0f };
		}

		// The SAH Builder for the Triangles
		void subdivide(uint32_t node_idx, uint32_t first, uint32_t count) {
			for (uint32_t i = first; i < first + count; ++i) {
				const Triangle& tri = m_triangles[m_triangle_indices[i]];
				m_nodes[node_idx].bounds.grow(tri.v0);
				m_nodes[node_idx].bounds.grow(tri.v1);
				m_nodes[node_idx].bounds.grow(tri.v2);
			}

			// Stop branching if we have very few triangles in this box
			if (count <= 2) {
				m_nodes[node_idx].first_triangle = first;
				m_nodes[node_idx].triangle_count = count;
				return;
			}

			int best_axis = -1;
			uint32_t best_split = 0;
			float best_cost = FLT_MAX;

			for (int axis = 0; axis < 3; ++axis) {
				std::sort(m_triangle_indices.begin() + first, m_triangle_indices.begin() + first + count,
					[&](uint32_t a, uint32_t b) {
						Vector3 ca = get_centroid(m_triangles[a]);
						Vector3 cb = get_centroid(m_triangles[b]);
						return (&ca.x)[axis] < (&cb.x)[axis];
					});

				std::vector<AABB> left_box(count), right_box(count);
				AABB run{};
				for (uint32_t i = 0; i < count; ++i) {
					const Triangle& tri = m_triangles[m_triangle_indices[first + i]];
					run.grow(tri.v0); run.grow(tri.v1); run.grow(tri.v2);
					left_box[i] = run;
				}
				run = {};
				for (int i = (int)count - 1; i >= 0; --i) {
					const Triangle& tri = m_triangles[m_triangle_indices[first + i]];
					run.grow(tri.v0); run.grow(tri.v1); run.grow(tri.v2);
					right_box[i] = run;
				}

				float parent_sa = m_nodes[node_idx].bounds.surface_area();
				if (parent_sa <= 0.0f) parent_sa = 1.0f;

				for (uint32_t split = 1; split < count; ++split) {
					float cost = (left_box[split - 1].surface_area() * split + right_box[split].surface_area() * (count - split)) / parent_sa;
					if (cost < best_cost) {
						best_cost = cost;
						best_axis = axis;
						best_split = split;
					}
				}
			}

			if (best_axis < 0 || best_cost >= count * 1.0f) {
				m_nodes[node_idx].first_triangle = first;
				m_nodes[node_idx].triangle_count = count;
				return;
			}

			std::sort(m_triangle_indices.begin() + first, m_triangle_indices.begin() + first + count,
				[&](uint32_t a, uint32_t b) {
					Vector3 ca = get_centroid(m_triangles[a]);
					Vector3 cb = get_centroid(m_triangles[b]);
					return (&ca.x)[best_axis] < (&cb.x)[best_axis];
				});

			uint32_t left_child_idx = static_cast<uint32_t>(m_nodes.size());
			m_nodes.push_back({});
			m_nodes.push_back({});

			m_nodes[node_idx].left_child = left_child_idx;

			subdivide(left_child_idx, first, best_split);
			subdivide(left_child_idx + 1, first + best_split, count - best_split);
		}
	};

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

		inline AABB aabb_from_model(const me::components::TransformComponent& t, const me::components::Model3DComponent& m) {
			const auto* blas = me::assets::internal_get_model_bvh(m.model);
			if (!blas || !blas->local_bounds.valid()) return {};

			// Transform all 8 corners of the model's bounding box to world space
			Vector3 corners[8] = {
				{blas->local_bounds.min.x, blas->local_bounds.min.y, blas->local_bounds.min.z},
				{blas->local_bounds.max.x, blas->local_bounds.min.y, blas->local_bounds.min.z},
				{blas->local_bounds.min.x, blas->local_bounds.max.y, blas->local_bounds.min.z},
				{blas->local_bounds.max.x, blas->local_bounds.max.y, blas->local_bounds.min.z},
				{blas->local_bounds.min.x, blas->local_bounds.min.y, blas->local_bounds.max.z},
				{blas->local_bounds.max.x, blas->local_bounds.min.y, blas->local_bounds.max.z},
				{blas->local_bounds.min.x, blas->local_bounds.max.y, blas->local_bounds.max.z},
				{blas->local_bounds.max.x, blas->local_bounds.max.y, blas->local_bounds.max.z}
			};

			AABB box;
			for (auto& c : corners) box.grow(Vector3Transform(c, t.model_matrix));
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
		// Scans the registry for every entity with Shape3DComponent or Model3DComponent,
		// then partitions them with the Surface Area Heuristic.
		// ----------------------------------------------------------
		void build(me::Registry& registry) {
			m_nodes.clear();
			m_prim_indices.clear();
			m_prims.clear();

			auto& transforms = registry.view<me::components::TransformComponent>();
			for (size_t i = 0; i < transforms.size(); ++i) {
				me::entity::entity_id e = transforms.entity_map[i];
				const auto& t = transforms.components[i];

				auto* shape = registry.try_get_component<me::components::Shape3DComponent>(e);
				auto* model = registry.try_get_component<me::components::Model3DComponent>(e);

				if (shape) {
					m_prims.push_back({ e, detail::compute_aabb(t, *shape) });
				} else if (model) {
					m_prims.push_back({ e, detail::aabb_from_model(t, *model) });
				}
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
			if (!t) return std::nullopt;

			auto* shape = registry.try_get_component<me::components::Shape3DComponent>(prim.entity);
			auto* model = registry.try_get_component<me::components::Model3DComponent>(prim.entity);

			// If the entity has neither a shape nor a model, it's invisible
			if (!shape && !model) return std::nullopt;

			float   t_hit = FLT_MAX;
			Vector3 normal = {};
			me::Color base_col = me::Color::white;

			if (shape) {
				// --- IT'S A PRIMITIVE SHAPE ---
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
				base_col = shape->color;
			} else if (model) {
				// --- IT'S A 3D MODEL (TWO-LEVEL BVH) ---
				const auto* blas = me::assets::internal_get_model_bvh(model->model);
				if (blas) {
					// 1. Transform ray to Model's Local Space
					Matrix inv_model = MatrixInvert(t->model_matrix);
					Vector3 local_origin = Vector3Transform(origin, inv_model);
					Vector3 local_target = Vector3Transform(Vector3Add(origin, dir), inv_model);
					Vector3 local_dir = Vector3Normalize(Vector3Subtract(local_target, local_origin));

					// 2. Intersect the Triangles
					float t_local;
					if (blas->intersect(local_origin, local_dir, t_local, normal)) {

						// 3. Convert Local Hit Point back to World Space to get exact physical distance
						Vector3 local_hit = Vector3Add(local_origin, Vector3Scale(local_dir, t_local));
						Vector3 world_hit = Vector3Transform(local_hit, t->model_matrix);
						t_hit = Vector3Distance(origin, world_hit);

						if (t_hit >= max_dist) return std::nullopt;

						// 4. Transform Normal to World Space (Ignoring translation)
						Matrix nm = MatrixTranspose(inv_model);
						Vector3 world_n;
						world_n.x = normal.x * nm.m0 + normal.y * nm.m4 + normal.z * nm.m8;
						world_n.y = normal.x * nm.m1 + normal.y * nm.m5 + normal.z * nm.m9;
						world_n.z = normal.x * nm.m2 + normal.y * nm.m6 + normal.z * nm.m10;
						normal = Vector3Normalize(world_n);

						base_col = model->tint;
					} else {
						return std::nullopt; // Ray missed all triangles in the model
					}
				} else {
					return std::nullopt; // Model asset not loaded or BVH missing
				}
			}

			// --- Setup Payload ---
			HitPayload p{};
			p.hit_distance = t_hit;
			p.hit_point = Vector3Add(origin, Vector3Scale(dir, t_hit));

			p.front_face = Vector3DotProduct(dir, normal) < 0.0f;
			p.normal = p.front_face ? normal : Vector3Scale(normal, -1.0f);

			auto* mat = registry.try_get_component<me::components::MaterialComponent>(prim.entity);
			if (mat) {
				// Convert sRGB to Linear Space (approximate with pow 2.2)
				p.base_color = {
					std::pow(mat->albedo.r / 255.0f, 2.2f),
					std::pow(mat->albedo.g / 255.0f, 2.2f),
					std::pow(mat->albedo.b / 255.0f, 2.2f)
				};
				p.roughness = mat->roughness;
				p.metallic = mat->metallic;
				p.emission = {
					p.base_color.x * mat->emission_power,
					p.base_color.y * mat->emission_power,
					p.base_color.z * mat->emission_power
				};
				p.transmission = mat->transmission;
				p.ior = mat->ior;
			} else {
				// Convert sRGB to Linear Space
				p.base_color = {
					std::pow(base_col.r / 255.0f, 2.2f),
					std::pow(base_col.g / 255.0f, 2.2f),
					std::pow(base_col.b / 255.0f, 2.2f)
				};
				p.roughness = 1.0f;
				p.metallic = 0.0f;
				p.emission = { 0.0f, 0.0f, 0.0f };
				p.transmission = 0.0f;
				p.ior = 1.0f;
			}

			return p;
		}
	};


} // namespace me::raytracing