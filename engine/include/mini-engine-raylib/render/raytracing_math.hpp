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
		Vector3 emission;
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

	// --- Monte Carlo / Lighting Helpers ---
	float   random_float();
	Vector3 random_unit_vector();
	Vector3 sample_sky(const Vector3& dir);

} // namespace me::raytracing