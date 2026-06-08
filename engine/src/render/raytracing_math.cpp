#include "mini-engine-raylib/render/raytracing_math.hpp"
#include <cmath>
#include <random>

namespace me::raytracing {

	bool intersect_sphere(const Vector3& ray_origin, const Vector3& ray_dir, const Vector3& center, float radius, float& out_t) {
		Vector3 oc = Vector3Subtract(ray_origin, center);
		float a = Vector3DotProduct(ray_dir, ray_dir);
		float b = 2.0f * Vector3DotProduct(oc, ray_dir);
		float c = Vector3DotProduct(oc, oc) - (radius * radius);
		float discriminant = (b * b) - (4.0f * a * c);

		if (discriminant > 0.0f) {
			float t = (-b - std::sqrt(discriminant)) / (2.0f * a);
			if (t > 0.001f) {
				out_t = t;
				return true;
			}
		}
		return false;
	}

	bool intersect_plane(const Vector3& ray_origin, const Vector3& ray_dir, const Vector3& center, const Vector3& normal, float& out_t) {
		float denom = Vector3DotProduct(normal, ray_dir);
		if (std::abs(denom) > 1e-6f) {
			Vector3 oc = Vector3Subtract(center, ray_origin);
			float t = Vector3DotProduct(oc, normal) / denom;
			if (t > 0.001f) {
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
		Vector3 local_dir = Vector3Subtract(local_target, local_origin);

		float t_min = -FLT_MAX;
		float t_max = FLT_MAX;
		int hit_axis = -1;
		float hit_sign = 1.0f;

		const float halves[3] = { 1.0f, 1.0f, 1.0f };
		const float dirs[3] = { local_dir.x,    local_dir.y,    local_dir.z };
		const float origs[3] = { local_origin.x, local_origin.y, local_origin.z };

		for (int axis = 0; axis < 3; ++axis) {
			if (std::abs(dirs[axis]) < 1e-8f) {
				if (std::abs(origs[axis]) > halves[axis]) return false;
			} else {
				float inv_d = 1.0f / dirs[axis];
				float t1 = (-halves[axis] - origs[axis]) * inv_d;
				float t2 = (halves[axis] - origs[axis]) * inv_d;
				float sign = -1.0f;

				if (t1 > t2) { std::swap(t1, t2); sign = 1.0f; }
				if (t1 > t_min) { t_min = t1; hit_axis = axis; hit_sign = sign; }
				t_max = std::min(t_max, t2);
				if (t_min > t_max) return false;
			}
		}

		if (t_min < 0.001f || hit_axis < 0) return false;
		out_t = t_min;

		Vector3 local_normal = { 0.0f, 0.0f, 0.0f };
		if (hit_axis == 0) local_normal.x = hit_sign;
		else if (hit_axis == 1) local_normal.y = hit_sign;
		else                    local_normal.z = hit_sign;

		Matrix normal_matrix = MatrixTranspose(inv_model);
		out_normal.x = local_normal.x * normal_matrix.m0 + local_normal.y * normal_matrix.m4 + local_normal.z * normal_matrix.m8;
		out_normal.y = local_normal.x * normal_matrix.m1 + local_normal.y * normal_matrix.m5 + local_normal.z * normal_matrix.m9;
		out_normal.z = local_normal.x * normal_matrix.m2 + local_normal.y * normal_matrix.m6 + local_normal.z * normal_matrix.m10;
		out_normal = Vector3Normalize(out_normal);

		return true;
	}

	float intersect_aabb(const Vector3& ray_origin, const Vector3& ray_dir, const AABB& box) {
		float t_min = -FLT_MAX;
		float t_max = FLT_MAX;

		Vector3 inv_dir = {
			1.0f / (ray_dir.x == 0.0f ? 1e-8f : ray_dir.x),
			1.0f / (ray_dir.y == 0.0f ? 1e-8f : ray_dir.y),
			1.0f / (ray_dir.z == 0.0f ? 1e-8f : ray_dir.z)
		};

		float t1 = (box.min.x - ray_origin.x) * inv_dir.x;
		float t2 = (box.max.x - ray_origin.x) * inv_dir.x;
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
		//const float EPSILON = 1e-8f;

		// 1. Find vectors for two edges sharing V0
		Vector3 edge1 = Vector3Subtract(v1, v0);
		Vector3 edge2 = Vector3Subtract(v2, v0);

		// 2. Begin calculating determinant - also used to calculate U parameter
		Vector3 h = Vector3CrossProduct(ray_dir, edge2);
		float a = Vector3DotProduct(edge1, h);

		// If determinant is near zero, ray lies in plane of triangle (or is parallel to it)
		if (a > -EPSILON && a < EPSILON) {
			return false;
		}

		float f = 1.0f / a;

		// 3. Calculate distance from V0 to ray origin
		Vector3 s = Vector3Subtract(ray_origin, v0);

		// 4. Calculate U parameter and test bounds (Is it outside the triangle?)
		float u = f * Vector3DotProduct(s, h);
		if (u < 0.0f || u > 1.0f) {
			return false;
		}

		// 5. Prepare to test V parameter
		Vector3 q = Vector3CrossProduct(s, edge1);

		// 6. Calculate V parameter and test bounds
		float v = f * Vector3DotProduct(ray_dir, q);
		if (v < 0.0f || u + v > 1.0f) { // Notice the u + v > 1.0f! (Barycentric limit)
			return false;
		}

		// 7. Ray intersects triangle! Calculate T (distance)
		float t = f * Vector3DotProduct(edge2, q);

		// Make sure it's in front of the camera, not behind it
		if (t > EPSILON) {
			out_t = t;
			out_u = u;
			out_v = v;
			return true;
		}

		return false; // Line intersects, but it's behind the ray origin
	}

	Vector3 refract(const Vector3& uv, const Vector3& n, float etai_over_etat) {
		float cos_theta = std::min(Vector3DotProduct(Vector3Scale(uv, -1.0f), n), 1.0f);
		Vector3 r_out_perp = Vector3Scale(Vector3Add(uv, Vector3Scale(n, cos_theta)), etai_over_etat);

		float r_out_parallel_mag = -std::sqrt(std::abs(1.0f - Vector3DotProduct(r_out_perp, r_out_perp)));
		Vector3 r_out_parallel = Vector3Scale(n, r_out_parallel_mag);

		return Vector3Add(r_out_perp, r_out_parallel);
	}

	float reflectance(float cosine, float ref_idx) {
		// Schlick's approximation for Fresnel reflectance
		float r0 = (1.0f - ref_idx) / (1.0f + ref_idx);
		r0 = r0 * r0;
		return r0 + (1.0f - r0) * std::pow((1.0f - cosine), 5.0f);
	}

	Vector3 sample_sky(const Vector3& dir) {
		float t = 0.5f * (dir.y + 1.0f);
		return Vector3{
			(1.0f - t) * 1.0f + t * 0.5f,
			(1.0f - t) * 1.0f + t * 0.7f,
			(1.0f - t) * 1.0f + t * 1.0f
		};
	}

} // namespace me::raytracing