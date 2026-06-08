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

	float random_float() {
		static thread_local std::mt19937 generator(std::random_device{}());
		std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
		return distribution(generator);
	}

	Vector3 random_unit_vector() {
		while (true) {
			Vector3 p = { random_float() * 2.0f - 1.0f, random_float() * 2.0f - 1.0f, random_float() * 2.0f - 1.0f };
			float len_sq = Vector3DotProduct(p, p);
			if (len_sq > 0.001f && len_sq <= 1.0f) {
				float inv_len = 1.0f / std::sqrt(len_sq);
				return Vector3{ p.x * inv_len, p.y * inv_len, p.z * inv_len };
			}
		}
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