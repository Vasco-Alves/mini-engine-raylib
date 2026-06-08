#include "mini-engine-raylib/systems/raytracer_system.hpp"
#include "mini-engine-raylib/render/raytracing_math.hpp"

#include <raymath.h>
#include <mini-engine-raylib/core/logger.hpp>

#include <algorithm>
#include <cmath>
#include <execution>
#include <numeric>
#include <string>
#include <vector>

namespace me::systems {

	RaytracerSystem::RaytracerSystem() {
		m_OutputTexture.id = 0;
	}

	RaytracerSystem::~RaytracerSystem() {
		on_stop();
	}

	void RaytracerSystem::on_start(int width, int height) {
		m_Width = width;
		m_Height = height;

		m_AccumulationBuffer.resize(width * height, { 0.0f, 0.0f, 0.0f });
		m_PixelData.resize(width * height, ::BLANK);

		Image img = GenImageColor(width, height, ::BLANK);
		m_OutputTexture = LoadTextureFromImage(img);
		UnloadImage(img);

		// Don't build the BVH here — the registry isn't available at start time.
		// It will be built on the first on_update call.
		m_FrameCount = 1;

		me::logger::info("Raytracer initialized at "
			+ std::to_string(width) + "x" + std::to_string(height));
	}

	void RaytracerSystem::on_stop() {
		if (m_OutputTexture.id != 0) {
			UnloadTexture(m_OutputTexture);
			m_OutputTexture.id = 0;
		}
		m_AccumulationBuffer.clear();
		m_PixelData.clear();
		m_ActiveRegistry = nullptr;
	}

	void RaytracerSystem::reset_accumulation(me::Registry* registry) {
		m_FrameCount = 1;
		std::fill(m_AccumulationBuffer.begin(), m_AccumulationBuffer.end(),
			Vector3{ 0.0f, 0.0f, 0.0f });

		if (registry) {
			m_BVH.build(*registry);
		}
	}

	void RaytracerSystem::export_to_png(const std::string& filepath) {
		Image img = {
			.data = m_PixelData.data(),
			.width = m_Width,
			.height = m_Height,
			.mipmaps = 1,
			.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
		};
		ExportImage(img, filepath.c_str());
		me::logger::info("Raytrace image saved to: " + filepath);
	}

	void RaytracerSystem::on_update(
		me::Registry& registry,
		const me::components::CameraComponent& camera,
		const me::components::TransformComponent& cam_transform) {
		// Lazy BVH build: handles the case where on_start fired before the scene
		// was populated, so reset_accumulation had no registry to work with.
		if (m_BVH.empty()) {
			m_BVH.build(registry);
		}

		// Pin the registry for this frame so trace_ray can reach it without
		// carrying the pointer through every recursive bounce call.
		m_ActiveRegistry = &registry;

		if (!accumulate) {
			// Real-time noisy mode: reset the pixel history every frame.
			// Don't pass the registry — the BVH is still valid, no need to rebuild.
			reset_accumulation();
		}

		if (accumulate && m_FrameCount >= preview_samples) {
			m_ActiveRegistry = nullptr;
			return; // CPU rest — we've reached target quality
		}

		std::vector<int> rows(m_Height);
		std::iota(rows.begin(), rows.end(), 0);

		std::for_each(std::execution::par_unseq, rows.begin(), rows.end(), [&](int y) {
			for (int x = 0; x < m_Width; ++x) {
				int index = y * m_Width + x;

				me::raytracing::Ray ray = generate_camera_ray(x, y, m_Width, m_Height, camera, cam_transform);
				Vector3 light = trace_ray(ray, 0);

				m_AccumulationBuffer[index].x += light.x;
				m_AccumulationBuffer[index].y += light.y;
				m_AccumulationBuffer[index].z += light.z;

				Vector3 avg = {
					m_AccumulationBuffer[index].x / (float)m_FrameCount,
					m_AccumulationBuffer[index].y / (float)m_FrameCount,
					m_AccumulationBuffer[index].z / (float)m_FrameCount
				};

				// Gamma correction (approx gamma 2.2 via sqrt).
				// Linear light accumulation looks washed out on a display —
				// sqrt maps it into the perceptual range monitors expect.
				avg.x = std::sqrt(std::max(0.0f, avg.x));
				avg.y = std::sqrt(std::max(0.0f, avg.y));
				avg.z = std::sqrt(std::max(0.0f, avg.z));

				m_PixelData[index] = ::Color{
					(unsigned char)(std::clamp(avg.x, 0.0f, 1.0f) * 255.0f),
					(unsigned char)(std::clamp(avg.y, 0.0f, 1.0f) * 255.0f),
					(unsigned char)(std::clamp(avg.z, 0.0f, 1.0f) * 255.0f),
					255
				};
			}
			});

		UpdateTexture(m_OutputTexture, m_PixelData.data());

		if (accumulate) m_FrameCount++;

		m_ActiveRegistry = nullptr;
	}

	me::raytracing::Ray RaytracerSystem::generate_camera_ray(
		int x, int y, int width, int height,
		const me::components::CameraComponent& camera,
		const me::components::TransformComponent& cam_transform) {
		float ndc_x = (2.0f * (x + 0.5f) / (float)width) - 1.0f;
		float ndc_y = 1.0f - (2.0f * (y + 0.5f) / (float)height);

		float aspect = (float)width / (float)height;
		float scale = std::tan((camera.fov * 0.5f) * DEG2RAD);

		float vx = ndc_x * aspect * scale;
		float vy = ndc_y * scale;

		Vector3 fwd = Vector3Normalize(Vector3Subtract(
			{ camera.target.x, camera.target.y, camera.target.z },
			cam_transform.position));
		Vector3 up = Vector3Normalize({ camera.up.x, camera.up.y, camera.up.z });
		Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, up));
		Vector3 true_up = Vector3CrossProduct(right, fwd);

		Vector3 dir = {
			fwd.x + right.x * vx + true_up.x * vy,
			fwd.y + right.y * vx + true_up.y * vy,
			fwd.z + right.z * vx + true_up.z * vy
		};

		return { cam_transform.position, Vector3Normalize(dir) };
	}

	Vector3 RaytracerSystem::trace_ray(const me::raytracing::Ray& ray, int depth) {
		using namespace me::raytracing;

		if (depth >= max_bounces)
			return { 0.0f, 0.0f, 0.0f };

		// Pre-compute 1/dir once; reused for every AABB slab test this frame.
		// Guard against exact zeros to avoid inf*0 NaNs in the slab math.
		Vector3 inv_dir = {
			1.0f / (std::abs(ray.direction.x) > 1e-8f ? ray.direction.x : 1e-8f),
			1.0f / (std::abs(ray.direction.y) > 1e-8f ? ray.direction.y : 1e-8f),
			1.0f / (std::abs(ray.direction.z) > 1e-8f ? ray.direction.z : 1e-8f)
		};

		// ==========================================
		// 1. PRIMARY HIT via BVH
		// ==========================================
		auto hit = m_BVH.traverse(ray.origin, ray.direction, inv_dir, FLT_MAX, *m_ActiveRegistry);

		if (!hit) return sample_sky(ray.direction);

		const HitPayload& p = *hit;
		Vector3 shadow_origin = Vector3Add(p.hit_point, Vector3Scale(p.normal, 0.001f));

		// ==========================================
		// 2. EMISSION — early out for emissive surfaces
		// ==========================================
		// Return emission immediately — the surface IS a light source.
		// Bounce rays that land on emissive geometry will pick this up,
		// which is how emissive objects illuminate neighbours through GI.
		if ((p.emission.x + p.emission.y + p.emission.z) > 0.0f)
			return p.emission;

		// ==========================================
		// 3. DIRECT LIGHTING (Point + Directional)
		// ==========================================
		Vector3 direct = { 0.0f, 0.0f, 0.0f };

		// Point lights
		auto& light_pool = m_ActiveRegistry->view<me::components::LightComponent>();
		for (size_t i = 0; i < light_pool.size(); ++i) {
			auto* lt = m_ActiveRegistry->try_get_component<me::components::TransformComponent>(
				light_pool.entity_map[i]);
			if (!lt) continue;

			auto& l = light_pool.components[i];
			Vector3 lcolor = { l.color.r / 255.0f, l.color.g / 255.0f, l.color.b / 255.0f };
			Vector3 lvec = Vector3Subtract(lt->position, p.hit_point);
			float   ldist = Vector3Length(lvec);
			Vector3 ldir = { lvec.x / ldist, lvec.y / ldist, lvec.z / ldist };

			Vector3 sinv = {
				1.0f / (std::abs(ldir.x) > 1e-8f ? ldir.x : 1e-8f),
				1.0f / (std::abs(ldir.y) > 1e-8f ? ldir.y : 1e-8f),
				1.0f / (std::abs(ldir.z) > 1e-8f ? ldir.z : 1e-8f)
			};
			// Shadow: stop at ldist so occluders behind the light don't count
			bool in_shadow = m_BVH.traverse(shadow_origin, ldir, sinv,
				ldist, *m_ActiveRegistry).has_value();
			if (!in_shadow) {
				float ndotl = std::max(0.0f, Vector3DotProduct(p.normal, ldir));
				direct.x += ndotl * lcolor.x * l.intensity;
				direct.y += ndotl * lcolor.y * l.intensity;
				direct.z += ndotl * lcolor.z * l.intensity;
			}
		}

		// Directional lights
		auto& dir_pool = m_ActiveRegistry->view<me::components::DirectionalLightComponent>();
		for (size_t i = 0; i < dir_pool.size(); ++i) {
			auto* lt = m_ActiveRegistry->try_get_component<me::components::TransformComponent>(
				dir_pool.entity_map[i]);
			if (!lt) continue;

			auto& dl = dir_pool.components[i];
			Vector3 lcolor = { dl.color.r / 255.0f, dl.color.g / 255.0f, dl.color.b / 255.0f };
			float   pitch = lt->rotation.x * DEG2RAD;
			float   yaw = lt->rotation.y * DEG2RAD;
			Vector3 fwd = Vector3Normalize({
				std::cos(pitch) * std::sin(yaw), -std::sin(pitch), std::cos(pitch) * std::cos(yaw) });
			Vector3 ldir = { -fwd.x, -fwd.y, -fwd.z };

			Vector3 sinv = {
				1.0f / (std::abs(ldir.x) > 1e-8f ? ldir.x : 1e-8f),
				1.0f / (std::abs(ldir.y) > 1e-8f ? ldir.y : 1e-8f),
				1.0f / (std::abs(ldir.z) > 1e-8f ? ldir.z : 1e-8f)
			};
			// Directional light is infinitely far — any occluder counts
			bool in_shadow = m_BVH.traverse(shadow_origin, ldir, sinv,
				FLT_MAX, *m_ActiveRegistry).has_value();
			if (!in_shadow) {
				float ndotl = std::max(0.0f, Vector3DotProduct(p.normal, ldir));
				direct.x += ndotl * lcolor.x * dl.intensity;
				direct.y += ndotl * lcolor.y * dl.intensity;
				direct.z += ndotl * lcolor.z * dl.intensity;
			}
		}

		// Energy conservation: mirrors (roughness=0) are fully specular → no diffuse direct
		direct.x *= p.roughness;
		direct.y *= p.roughness;
		direct.z *= p.roughness;

		// ==========================================
		// 4. AMBIENT (sky-derived)
		// ==========================================
		constexpr float AMBIENT = 0.03f;
		Vector3 sky_amb = sample_sky(p.normal);
		Vector3 ambient = {
			sky_amb.x * AMBIENT * p.roughness,
			sky_amb.y * AMBIENT * p.roughness,
			sky_amb.z * AMBIENT * p.roughness
		};

		// ==========================================
		// 5. BOUNCE (GI + reflections)
		// ==========================================
		Vector3 reflect_dir = Vector3Normalize(Vector3Reflect(ray.direction, p.normal));
		Vector3 diffuse_dir = Vector3Normalize(Vector3Add(p.normal, random_unit_vector()));
		Vector3 bounce_dir = Vector3Normalize(Vector3Lerp(reflect_dir, diffuse_dir, p.roughness));

		if (Vector3DotProduct(bounce_dir, p.normal) < 0.0f)
			bounce_dir = diffuse_dir;

		Vector3 bounce_color = trace_ray({ shadow_origin, bounce_dir }, depth + 1);

		float   sw = 1.0f - p.roughness;           // specular weight
		float   dw = p.roughness;                  // diffuse weight
		float   bw = sw + dw * 0.5f;               // total bounce weight [0.5, 1.0]
		Vector3 bounce = {
			bounce_color.x * p.base_color.x * bw,
			bounce_color.y * p.base_color.y * bw,
			bounce_color.z * p.base_color.z * bw
		};

		// ==========================================
		// 6. COMBINE
		// radiance = albedo*(direct+ambient) + bounce
		// ==========================================
		return Vector3{
			std::clamp(p.base_color.x * (direct.x + ambient.x) + bounce.x, 0.0f, 1.0f),
			std::clamp(p.base_color.y * (direct.y + ambient.y) + bounce.y, 0.0f, 1.0f),
			std::clamp(p.base_color.z * (direct.z + ambient.z) + bounce.z, 0.0f, 1.0f)
		};
	}

} // namespace me::systems