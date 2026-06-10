#pragma once
#include <raymath.h>
#include <cfloat>
#include <algorithm>
#include <cmath>

namespace me::raytracing {

	// A pure mathematical Ray
	struct Ray {
		Vector3 origin;
		Vector3 direction;
	};

	// The result of an intersection
	struct HitPayload {
		float   hit_distance;
		Vector3 hit_point;
		Vector3 normal;
		Vector3 base_color;
		float   roughness;
		float   metallic;
		Vector3 emission;
		float   transmission;
		float   ior;
		float   tint_strength; // Beer–Lambert density multiplier (0 = always clear)
		bool    front_face; // Are we entering the glass, or exiting it?
	};

	// A raw 3D Triangle
	struct Triangle {
		Vector3 v0;
		Vector3 v1;
		Vector3 v2;
		// TODO: add normals and UVs here for texturing
	};

	// Axis-Aligned Bounding Box (Used for the BVH)
	struct AABB {
		Vector3 min = { FLT_MAX,  FLT_MAX,  FLT_MAX };
		Vector3 max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

		void grow(const Vector3& p) {
			min.x = std::min(min.x, p.x);  min.y = std::min(min.y, p.y);  min.z = std::min(min.z, p.z);
			max.x = std::max(max.x, p.x);  max.y = std::max(max.y, p.y);  max.z = std::max(max.z, p.z);
		}

		void grow(const AABB& b) {
			if (b.min.x != FLT_MAX) { grow(b.min); grow(b.max); }
		}

		// Half surface area — used by the SAH cost function.
		// We use half-SA so the numbers stay smaller; ratios are identical.
		float surface_area() const {
			float ex = max.x - min.x;
			float ey = max.y - min.y;
			float ez = max.z - min.z;
			return ex * ey + ey * ez + ez * ex; // half surface area
		}

		// Slab-based ray/AABB intersection.
		// inv_dir must be pre-computed by the caller (1/ray_dir per component).
		// Returns the entry t-value, or FLT_MAX on a miss.
		// max_dist lets shadow rays reject the node early without a full traversal.
		float intersect(const Vector3& origin, const Vector3& inv_dir, float max_dist) const {
			float tx1 = (min.x - origin.x) * inv_dir.x;
			float tx2 = (max.x - origin.x) * inv_dir.x;
			float ty1 = (min.y - origin.y) * inv_dir.y;
			float ty2 = (max.y - origin.y) * inv_dir.y;
			float tz1 = (min.z - origin.z) * inv_dir.z;
			float tz2 = (max.z - origin.z) * inv_dir.z;

			float t_min = std::max({ std::min(tx1, tx2), std::min(ty1, ty2), std::min(tz1, tz2) });
			float t_max = std::min({ std::max(tx1, tx2), std::max(ty1, ty2), std::max(tz1, tz2) });

			if (t_max < 0.0f || t_min > t_max || t_min > max_dist) return FLT_MAX;
			return t_min;
		}

		Vector3 centroid() const {
			return { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f };
		}

		bool valid() const { return min.x != FLT_MAX; }
	};

	// --- Intersections ---
	bool  intersect_sphere(const Vector3& ray_origin, const Vector3& ray_dir, const Vector3& center, float radius, float& out_t);
	bool  intersect_plane(const Vector3& ray_origin, const Vector3& ray_dir, const Vector3& center, const Vector3& normal, float& out_t);
	bool  intersect_obb(const Vector3& ray_origin, const Vector3& ray_dir, const Matrix& model_matrix, float& out_t, Vector3& out_normal);
	float intersect_aabb(const Vector3& ray_origin, const Vector3& ray_dir, const AABB& box); // legacy, kept for compat
	bool intersect_triangle(const Vector3& ray_origin, const Vector3& ray_dir, const Vector3& v0, const Vector3& v1, const Vector3& v2, float& out_t, float& out_u, float& out_v);
	Vector3 refract(const Vector3& uv, const Vector3& n, float etai_over_etat);
	float   reflectance(float cosine, float ref_idx);

	// --- Quasi-Monte Carlo
	inline uint32_t pcg_hash(uint32_t& seed) {
		uint32_t state = seed * 747796405u + 2891336453u;
		uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
		seed = (word >> 22u) ^ word;
		return seed;
	}

	inline float random_float(uint32_t& seed) {
		return (float)pcg_hash(seed) / (float)UINT32_MAX;
	}

	inline Vector3 random_unit_vector(uint32_t& seed) {
		while (true) {
			Vector3 p = { random_float(seed) * 2.0f - 1.0f, random_float(seed) * 2.0f - 1.0f, random_float(seed) * 2.0f - 1.0f };
			float len_sq = Vector3DotProduct(p, p);
			if (len_sq > 0.001f && len_sq <= 1.0f) {
				float inv_len = 1.0f / std::sqrt(len_sq);
				return Vector3{ p.x * inv_len, p.y * inv_len, p.z * inv_len };
			}
		}
	}

	inline float halton(uint32_t index, uint32_t base) {
		float f = 1.0f;
		float r = 0.0f;
		while (index > 0) {
			f = f / (float)base;
			r = r + f * (float)(index % base);
			index = index / base;
		}
		return r;
	}

	Vector3 sample_sky(const Vector3& dir);

} // namespace me::raytracing