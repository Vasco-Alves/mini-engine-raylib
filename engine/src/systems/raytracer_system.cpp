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

		// BVH is built lazily on the first on_update call because the registry may not be fully populated at start time.
		m_FrameCount = 1;

		me::logger::info("Raytracer initialized at " + std::to_string(width) + "x" + std::to_string(height));
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

	void RaytracerSystem::on_update(me::Registry& registry, const me::components::CameraComponent& camera, const me::components::TransformComponent& cam_transform) {

		// Lazy BVH build: handles the case where on_start fired before the scene
		// was populated, so reset_accumulation had no registry to work with.
		if (m_BVH.empty()) {
			m_BVH.build(registry);
		}

		// Pin the registry as const for this frame — the trace loop is read-only.
		// Using a const pointer enforces that and prevents accidental writes from
		// any future code path inside trace_ray.
		m_ActiveRegistry = &registry;

		if (!accumulate) {
			// Real-time noisy mode: reset pixel history every frame.
			// Reuse the existing BVH — no need to rebuild.
			// m_TotalFramesRendered keeps incrementing so the PCG seed and the
			// Halton jitter stay in sync even across accumulation resets.
			reset_accumulation();
		}

		if (accumulate && m_FrameCount >= preview_samples) {
			m_ActiveRegistry = nullptr;
			return; // We've reached target quality — let the CPU rest.
		}

		std::vector<int> rows(m_Height);
		std::iota(rows.begin(), rows.end(), 0);

		std::for_each(std::execution::par_unseq, rows.begin(), rows.end(), [&](int y) {
			for (int x = 0; x < m_Width; ++x) {
				int index = y * m_Width + x;

				uint32_t seed = static_cast<uint32_t>(y * m_Width + x) + m_TotalFramesRendered * 719393u;

				// Generate the QMC anti-aliased camera ray.
				// m_FrameCount drives the Halton sequence (starts at 1, never 0,
				// which avoids the degenerate (0,0) sample at the pixel center).
				me::raytracing::Ray ray = generate_camera_ray(
					x, y, m_Width, m_Height, camera, cam_transform, m_FrameCount);

				// Trace and accumulate.
				Vector3 light = trace_ray(ray, 0, seed);

				m_AccumulationBuffer[index].x += light.x;
				m_AccumulationBuffer[index].y += light.y;
				m_AccumulationBuffer[index].z += light.z;

				Vector3 avg = {
					m_AccumulationBuffer[index].x / static_cast<float>(m_FrameCount),
					m_AccumulationBuffer[index].y / static_cast<float>(m_FrameCount),
					m_AccumulationBuffer[index].z / static_cast<float>(m_FrameCount)
				};

				// ---- Tonemapping (ACES Filmic) ----
				float exposure = 0.6f;
				avg.x *= exposure;
				avg.y *= exposure;
				avg.z *= exposure;

				const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
				avg.x = (avg.x * (a * avg.x + b)) / (avg.x * (c * avg.x + d) + e);
				avg.y = (avg.y * (a * avg.y + b)) / (avg.y * (c * avg.y + d) + e);
				avg.z = (avg.z * (a * avg.z + b)) / (avg.z * (c * avg.z + d) + e);

				// ---- Gamma correction (approx γ 2.2 via sqrt) ----
				avg.x = std::sqrt(std::max(0.0f, avg.x));
				avg.y = std::sqrt(std::max(0.0f, avg.y));
				avg.z = std::sqrt(std::max(0.0f, avg.z));

				m_PixelData[index] = ::Color{
					static_cast<unsigned char>(std::clamp(avg.x, 0.0f, 1.0f) * 255.0f),
					static_cast<unsigned char>(std::clamp(avg.y, 0.0f, 1.0f) * 255.0f),
					static_cast<unsigned char>(std::clamp(avg.z, 0.0f, 1.0f) * 255.0f),
					255
				};
			}
			});

		UpdateTexture(m_OutputTexture, m_PixelData.data());

		if (accumulate) m_FrameCount++;
		m_TotalFramesRendered++;

		m_ActiveRegistry = nullptr;
	}

	me::raytracing::Ray RaytracerSystem::generate_camera_ray(
		int x, int y, int width, int height,
		const me::components::CameraComponent& camera,
		const me::components::TransformComponent& cam_transform,
		uint32_t frame_count) {

		// Halton(base-2) for X, Halton(base-3) for Y.
		// frame_count starts at 1, so we never evaluate the degenerate (0, 0)
		// sample — which would always land exactly at the pixel centre and
		// produce structured aliasing instead of smooth convergence.
		float jitter_x = me::raytracing::halton(frame_count, 2) - 0.5f;
		float jitter_y = me::raytracing::halton(frame_count, 3) - 0.5f;

		float ndc_x = (2.0f * (x + 0.5f + jitter_x) / static_cast<float>(width)) - 1.0f;
		float ndc_y = 1.0f - (2.0f * (y + 0.5f + jitter_y) / static_cast<float>(height));

		float aspect = static_cast<float>(width) / static_cast<float>(height);
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

	Vector3 RaytracerSystem::trace_ray(const me::raytracing::Ray& ray, int depth, uint32_t& seed) {
		using namespace me::raytracing;

		if (depth >= max_bounces)
			return { 0.0f, 0.0f, 0.0f };

		// Pre-compute 1/dir; guards against exact zeros to avoid inf*0 NaNs.
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
		// 2. EMISSION — early out for light sources
		// ==========================================
		if ((p.emission.x + p.emission.y + p.emission.z) > 0.0f)
			return p.emission;

		// ==========================================
		// 3. DIRECT LIGHTING (Point + Directional)
		// ==========================================
		Vector3 direct = { 0.0f, 0.0f, 0.0f };

		// ---- Point lights ----
		auto& light_pool = m_ActiveRegistry->view<me::components::LightComponent>();
		for (size_t i = 0; i < light_pool.size(); ++i) {
			auto* lt = m_ActiveRegistry->try_get_component<me::components::TransformComponent>(
				light_pool.entity_map[i]);
			if (!lt) continue;

			auto& l = light_pool.components[i];

			Vector3 lcolor = {
				std::pow(l.color.r / 255.0f, 2.2f),
				std::pow(l.color.g / 255.0f, 2.2f),
				std::pow(l.color.b / 255.0f, 2.2f)
			};

			Vector3 lvec = Vector3Subtract(lt->position, p.hit_point);
			float   ldist = Vector3Length(lvec);
			Vector3 ldir = { lvec.x / ldist, lvec.y / ldist, lvec.z / ldist };

			Vector3 sinv = {
				1.0f / (std::abs(ldir.x) > 1e-8f ? ldir.x : 1e-8f),
				1.0f / (std::abs(ldir.y) > 1e-8f ? ldir.y : 1e-8f),
				1.0f / (std::abs(ldir.z) > 1e-8f ? ldir.z : 1e-8f)
			};

			bool in_shadow = m_BVH.traverse(shadow_origin, ldir, sinv, ldist, *m_ActiveRegistry).has_value();
			if (!in_shadow) {
				float ndotl = std::max(0.0f, Vector3DotProduct(p.normal, ldir));
				direct.x += ndotl * lcolor.x * l.intensity;
				direct.y += ndotl * lcolor.y * l.intensity;
				direct.z += ndotl * lcolor.z * l.intensity;
			}
		}

		// ---- Directional lights ----
		auto& dir_pool = m_ActiveRegistry->view<me::components::DirectionalLightComponent>();
		for (size_t i = 0; i < dir_pool.size(); ++i) {
			auto* lt = m_ActiveRegistry->try_get_component<me::components::TransformComponent>(
				dir_pool.entity_map[i]);
			if (!lt) continue;

			auto& dl = dir_pool.components[i];

			Vector3 lcolor = {
				std::pow(dl.color.r / 255.0f, 2.2f),
				std::pow(dl.color.g / 255.0f, 2.2f),
				std::pow(dl.color.b / 255.0f, 2.2f)
			};

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

			bool in_shadow = m_BVH.traverse(shadow_origin, ldir, sinv, FLT_MAX, *m_ActiveRegistry).has_value();
			if (!in_shadow) {
				float ndotl = std::max(0.0f, Vector3DotProduct(p.normal, ldir));
				direct.x += ndotl * lcolor.x * dl.intensity;
				direct.y += ndotl * lcolor.y * dl.intensity;
				direct.z += ndotl * lcolor.z * dl.intensity;
			}
		}

		// Direct light is purely diffuse. Metals have no diffuse properties!
		direct.x *= p.roughness * (1.0f - p.metallic);
		direct.y *= p.roughness * (1.0f - p.metallic);
		direct.z *= p.roughness * (1.0f - p.metallic);

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
		// 5. BOUNCE (GI + Reflections + Refractions)
		// ==========================================
		float survival_p = 1.0f;

		// --- RUSSIAN ROULETTE ---
		if (depth >= 2) {
			float max_channel = std::max({ p.base_color.x, p.base_color.y, p.base_color.z });
			survival_p = std::clamp(max_channel, 0.1f, 0.95f);

			if (random_float(seed) > survival_p) {
				// Ray dies early! Return gathered light.
				return Vector3{
					p.base_color.x * (direct.x + ambient.x),
					p.base_color.y * (direct.y + ambient.y),
					p.base_color.z * (direct.z + ambient.z)
				};
			}
		}

		Vector3 bounce_dir;
		Vector3 bounce_origin;

		if (p.transmission > 0.0f) {
			// ---- Glass / Refraction path ----
			float refraction_ratio = p.front_face ? (1.0f / p.ior) : p.ior;

			Vector3 unit_dir = Vector3Normalize(ray.direction);
			float   cos_theta = std::min(Vector3DotProduct(Vector3Scale(unit_dir, -1.0f), p.normal), 1.0f);
			float   sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);

			bool cannot_refract = refraction_ratio * sin_theta > 1.0f;

			if (cannot_refract || reflectance(cos_theta, refraction_ratio) > random_float(seed)) {
				bounce_dir = Vector3Normalize(Vector3Reflect(unit_dir, p.normal));
			} else {
				bounce_dir = Vector3Normalize(refract(unit_dir, p.normal, refraction_ratio));
			}

			if (p.roughness > 0.0f) {
				bounce_dir = Vector3Normalize(Vector3Add(
					bounce_dir, Vector3Scale(random_unit_vector(seed), p.roughness)));
			}

			if (Vector3DotProduct(bounce_dir, p.normal) < 0.0f)
				bounce_origin = Vector3Subtract(p.hit_point, Vector3Scale(p.normal, 0.001f));
			else
				bounce_origin = Vector3Add(p.hit_point, Vector3Scale(p.normal, 0.001f));

		} else {
			// ---- Opaque / Solid path ----
			Vector3 reflect_dir = Vector3Normalize(Vector3Reflect(ray.direction, p.normal));
			Vector3 diffuse_dir = Vector3Normalize(Vector3Add(p.normal, random_unit_vector(seed)));
			bounce_dir = Vector3Normalize(Vector3Lerp(reflect_dir, diffuse_dir, p.roughness));

			if (Vector3DotProduct(bounce_dir, p.normal) < 0.0f) bounce_dir = diffuse_dir;

			bounce_origin = Vector3Add(p.hit_point, Vector3Scale(p.normal, 0.001f));
		}

		Vector3 bounce_color = trace_ray({ bounce_origin, bounce_dir }, depth + 1, seed);

		// --- RR ENERGY COMPENSATION ---
		bounce_color.x /= survival_p;
		bounce_color.y /= survival_p;
		bounce_color.z /= survival_p;

		// ---- Combine bounce contribution ----
		Vector3 bounce = { 0.0f, 0.0f, 0.0f };
		if (p.transmission > 0.0f) {
			bounce = { bounce_color.x * p.base_color.x, bounce_color.y * p.base_color.y, bounce_color.z * p.base_color.z };
		} else {
			float sw = 1.0f - p.roughness;
			float dw = p.roughness;
			float bw = sw + dw * 0.5f;

			// FIX: Only smooth plastics reflect pure white. Everything else uses base_color!
			float white_blend = (1.0f - p.metallic) * (1.0f - p.roughness);
			Vector3 bounce_tint = {
				std::lerp(p.base_color.x, 1.0f, white_blend),
				std::lerp(p.base_color.y, 1.0f, white_blend),
				std::lerp(p.base_color.z, 1.0f, white_blend)
			};

			bounce = {
				bounce_color.x * bounce_tint.x * bw,
				bounce_color.y * bounce_tint.y * bw,
				bounce_color.z * bounce_tint.z * bw
			};
		}

		// ==========================================
		// 6. COMBINE
		// ==========================================
		return Vector3{
			p.base_color.x * (direct.x + ambient.x) + bounce.x,
			p.base_color.y * (direct.y + ambient.y) + bounce.y,
			p.base_color.z * (direct.z + ambient.z) + bounce.z
		};
	}

} // namespace me::systems