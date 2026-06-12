#include "mini-engine-raylib/render/raytracing_math.hpp"
#include <cmath>

namespace me::raytracing {

	// Local EPSILON for intersection tests.
	// Using a named constant avoids relying on any macro from raylib or elsewhere.
	static constexpr float RT_EPSILON = 1e-8f;
	// Minimum hit distance — keeps self-intersection acne off surfaces.
	static constexpr float RAY_TMIN = 0.001f;

	bool intersect_sphere(const Vector3& ray_origin, const Vector3& ray_dir, const Vector3& center, float radius, float& out_t) {
		Vector3 oc = Vector3Subtract(ray_origin, center);
		float a = Vector3DotProduct(ray_dir, ray_dir);
		float b = 2.0f * Vector3DotProduct(oc, ray_dir);
		float c = Vector3DotProduct(oc, oc) - (radius * radius);
		float discriminant = (b * b) - (4.0f * a * c);

		if (discriminant > 0.0f) {
			float sqrt_d = std::sqrt(discriminant);
			float inv2a = 1.0f / (2.0f * a);

			// Try the near hit first; fall through to the far hit when the ray
			// origin is inside the sphere (near hit is behind us).
			float t = (-b - sqrt_d) * inv2a;
			if (t < RAY_TMIN)
				t = (-b + sqrt_d) * inv2a; // far hit (exit point)

			if (t > RAY_TMIN) {
				out_t = t;
				return true;
			}
		}
		return false;
	}

	bool intersect_plane(const Vector3& ray_origin, const Vector3& ray_dir, const Vector3& center, const Vector3& normal, float& out_t) {
		float denom = Vector3DotProduct(normal, ray_dir);
		if (std::abs(denom) > RT_EPSILON) {
			Vector3 oc = Vector3Subtract(center, ray_origin);
			float t = Vector3DotProduct(oc, normal) / denom;
			if (t > RAY_TMIN) {
				out_t = t;
				return true;
			}
		}
		return false;
	}

	bool intersect_obb(const Vector3& ray_origin, const Vector3& ray_dir, const Matrix& model_matrix, float& out_t, Vector3& out_normal) {
		Matrix inv_model = MatrixInvert(model_matrix);
		Vector3 local_origin = Vector3Transform(ray_origin, inv_model);
		Vector3 local_target = Vector3Transform(Vector3Add(ray_origin, ray_dir), inv_model);

		// IMPORTANT: local_dir must be normalized so that t values from the slab
		// test are in local-space units. We later convert the hit point back to
		// world space to get the correct physical distance, exactly like the
		// Model3D path in bvh.hpp does.
		Vector3 local_dir = Vector3Normalize(Vector3Subtract(local_target, local_origin));

		float t_min = -FLT_MAX;
		float t_max = FLT_MAX;
		int   hit_axis = -1;
		float hit_sign = 1.0f;
		int   exit_axis = -1;
		float exit_sign = 1.0f;

		// Unit cube spans [-1, 1] on each axis.
		const float halves[3] = { 1.0f, 1.0f, 1.0f };
		const float dirs[3] = { local_dir.x,    local_dir.y,    local_dir.z };
		const float origs[3] = { local_origin.x, local_origin.y, local_origin.z };

		for (int axis = 0; axis < 3; ++axis) {
			if (std::abs(dirs[axis]) < RT_EPSILON) {
				// Ray is parallel to this slab — miss if origin is outside.
				if (std::abs(origs[axis]) > halves[axis]) return false;
			} else {
				float inv_d = 1.0f / dirs[axis];
				float t1 = (-halves[axis] - origs[axis]) * inv_d; // near slab
				float t2 = (halves[axis] - origs[axis]) * inv_d; // far slab

				// t1 < t2 means we hit the negative-axis face first → normal points
				// in the negative direction on this axis (sign = -1).
				// After swap, t1 is always the entry; sign tracks which face we enter.
				float sign = -1.0f; // entering from the negative face
				if (t1 > t2) {
					std::swap(t1, t2);
					sign = 1.0f; // entering from the positive face
				}

				if (t1 > t_min) { t_min = t1; hit_axis = axis; hit_sign = sign; }
				// The exit face on this axis is the opposite one → opposite sign.
				if (t2 < t_max) { t_max = t2; exit_axis = axis; exit_sign = -sign; }
				if (t_min > t_max) return false;
			}
		}

		// Entry face when the ray starts outside the box; exit face when it starts
		// inside (a refracted ray traveling through glass needs the back-face hit
		// so Beer–Lambert absorption sees the interior distance) — mirrors the
		// sphere's far-hit fallback above.
		float t_local = t_min;
		int   face_axis = hit_axis;
		float face_sign = hit_sign;
		if (t_min < RAY_TMIN) {
			t_local = t_max;
			face_axis = exit_axis;
			face_sign = exit_sign;
		}
		if (t_local < RAY_TMIN || face_axis < 0) return false;

		// Convert the local-space hit distance back to world-space distance.
		// We reconstruct the world hit point and measure it from the ray origin.
		Vector3 local_hit = Vector3Add(local_origin, Vector3Scale(local_dir, t_local));
		Vector3 world_hit = Vector3Transform(local_hit, model_matrix);
		out_t = Vector3Distance(ray_origin, world_hit);

		// Build the local normal and transform it to world space via the
		// normal matrix (transpose of the inverse model matrix).
		Vector3 local_normal = { 0.0f, 0.0f, 0.0f };
		if (face_axis == 0) local_normal.x = face_sign;
		else if (face_axis == 1) local_normal.y = face_sign;
		else                     local_normal.z = face_sign;

		Matrix normal_matrix = MatrixTranspose(inv_model);
		out_normal.x = local_normal.x * normal_matrix.m0 + local_normal.y * normal_matrix.m4 + local_normal.z * normal_matrix.m8;
		out_normal.y = local_normal.x * normal_matrix.m1 + local_normal.y * normal_matrix.m5 + local_normal.z * normal_matrix.m9;
		out_normal.z = local_normal.x * normal_matrix.m2 + local_normal.y * normal_matrix.m6 + local_normal.z * normal_matrix.m10;
		out_normal = Vector3Normalize(out_normal);

		return true;
	}

	// Legacy brute-force AABB test — kept for compatibility, not used by the BVH.
	float intersect_aabb(const Vector3& ray_origin, const Vector3& ray_dir, const AABB& box) {
		float t_min = -FLT_MAX;
		float t_max = FLT_MAX;

		Vector3 inv_dir = {
			1.0f / (std::abs(ray_dir.x) > RT_EPSILON ? ray_dir.x : RT_EPSILON),
			1.0f / (std::abs(ray_dir.y) > RT_EPSILON ? ray_dir.y : RT_EPSILON),
			1.0f / (std::abs(ray_dir.z) > RT_EPSILON ? ray_dir.z : RT_EPSILON)
		};

		float t1, t2;
		t1 = (box.min.x - ray_origin.x) * inv_dir.x;
		t2 = (box.max.x - ray_origin.x) * inv_dir.x;
		t_min = std::max(t_min, std::min(t1, t2));
		t_max = std::min(t_max, std::max(t1, t2));

		t1 = (box.min.y - ray_origin.y) * inv_dir.y;
		t2 = (box.max.y - ray_origin.y) * inv_dir.y;
		t_min = std::max(t_min, std::min(t1, t2));
		t_max = std::min(t_max, std::max(t1, t2));

		t1 = (box.min.z - ray_origin.z) * inv_dir.z;
		t2 = (box.max.z - ray_origin.z) * inv_dir.z;
		t_min = std::max(t_min, std::min(t1, t2));
		t_max = std::min(t_max, std::max(t1, t2));

		if (t_max >= t_min && t_max > 0.0f) return t_min > 0.0f ? t_min : 0.0f;
		return FLT_MAX;
	}

	bool intersect_triangle(const Vector3& ray_origin, const Vector3& ray_dir,
		const Vector3& v0, const Vector3& v1, const Vector3& v2,
		float& out_t, float& out_u, float& out_v) {

		// Möller–Trumbore algorithm.

		Vector3 edge1 = Vector3Subtract(v1, v0);
		Vector3 edge2 = Vector3Subtract(v2, v0);

		Vector3 h = Vector3CrossProduct(ray_dir, edge2);
		float   a = Vector3DotProduct(edge1, h);

		// Ray is parallel to the triangle plane — no intersection.
		if (a > -EPSILON && a < EPSILON) return false;

		float   f = 1.0f / a;
		Vector3 s = Vector3Subtract(ray_origin, v0);
		float   u = f * Vector3DotProduct(s, h);
		if (u < 0.0f || u > 1.0f) return false;

		Vector3 q = Vector3CrossProduct(s, edge1);
		float   v = f * Vector3DotProduct(ray_dir, q);
		if (v < 0.0f || u + v > 1.0f) return false;

		float t = f * Vector3DotProduct(edge2, q);
		if (t > EPSILON) {
			out_t = t;
			out_u = u;
			out_v = v;
			return true;
		}

		// Line intersects the triangle plane, but the hit is behind the ray origin.
		return false;
	}

	Vector3 refract(const Vector3& uv, const Vector3& n, float etai_over_etat) {
		float cos_theta = std::min(Vector3DotProduct(Vector3Scale(uv, -1.0f), n), 1.0f);
		Vector3 r_out_perp = Vector3Scale(Vector3Add(uv, Vector3Scale(n, cos_theta)), etai_over_etat);

		float perp_len_sq = Vector3DotProduct(r_out_perp, r_out_perp);
		float r_out_parallel_mag = -std::sqrt(std::abs(1.0f - perp_len_sq));
		Vector3 r_out_parallel = Vector3Scale(n, r_out_parallel_mag);

		return Vector3Add(r_out_perp, r_out_parallel);
	}

	float reflectance(float cosine, float ref_idx) {
		// Schlick's approximation for Fresnel reflectance.
		float r0 = (1.0f - ref_idx) / (1.0f + ref_idx);
		r0 = r0 * r0;
		return r0 + (1.0f - r0) * std::pow((1.0f - cosine), 5.0f);
	}

	// dir is assumed to be a unit vector.
	// sample_sky(p.normal) is also valid because surface normals are unit-length.
	Vector3 sample_sky(const Vector3& dir) {
		// Remap y from [-1, 1] to [0, 1] for a horizon-to-zenith gradient.
		float t = 0.5f * (dir.y + 1.0f);
		return Vector3{
			(1.0f - t) * 1.0f + t * 0.5f, // white → light blue (R)
			(1.0f - t) * 1.0f + t * 0.7f, // white → light blue (G)
			(1.0f - t) * 1.0f + t * 1.0f  // white → light blue (B)
		};
	}

} // namespace me::raytracing