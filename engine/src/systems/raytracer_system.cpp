#include "mini-engine-raylib/systems/raytracer_system.hpp"
#include "mini-engine-raylib/render/raytracing_math.hpp"

#include <mini-engine-raylib/core/logger.hpp>
#include <mini-engine-raylib/core/file_system.hpp>

#include <rlgl.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <execution>
#include <numeric>
#include <string>
#include <vector>

namespace me::systems {

	namespace {
		// Uniformly samples a direction within the cone of half-angle acos(cos_max)
		// around `dir`. cos_max == 1 returns `dir` exactly (a hard, point-like sample);
		// smaller cos_max widens the cone, which softens shadows as samples accumulate.
		Vector3 sample_cone(const Vector3& dir, float cos_max, uint32_t& seed) {
			float r1 = me::raytracing::random_float(seed);
			float r2 = me::raytracing::random_float(seed);
			float cos_t = 1.0f - r1 * (1.0f - cos_max);
			float sin_t = std::sqrt(std::max(0.0f, 1.0f - cos_t * cos_t));
			float phi = 2.0f * PI * r2;
			Vector3 a = (std::abs(dir.x) > 0.9f) ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
			Vector3 u = Vector3Normalize(Vector3CrossProduct(dir, a));
			Vector3 v = Vector3CrossProduct(dir, u);
			Vector3 s = Vector3Add(
				Vector3Add(Vector3Scale(u, std::cos(phi) * sin_t), Vector3Scale(v, std::sin(phi) * sin_t)),
				Vector3Scale(dir, cos_t));
			return Vector3Normalize(s);
		}
	}

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

		// --- LOAD COMPUTE SHADER ---
		std::string compShaderSrc = me::fs::read_text("engine://shaders/raytracer.comp");

		if (!compShaderSrc.empty()) {
			unsigned int csId = rlLoadShader(compShaderSrc.c_str(), RL_COMPUTE_SHADER);
			m_ComputeShaderProgram = rlLoadShaderProgramCompute(csId);

			if (m_ComputeShaderProgram != 0) {
				me::logger::info("Compute Shader compiled and linked successfully!");
			} else {
				me::logger::error("Failed to link compute shader program!");
			}
		} else {
			me::logger::error("Failed to load raytracer.comp!");
		}

		me::logger::info("Raytracer initialized at " + std::to_string(width) + "x" + std::to_string(height));
	}

	void RaytracerSystem::on_stop() {
		if (m_OutputTexture.id != 0) {
			UnloadTexture(m_OutputTexture);
			m_OutputTexture.id = 0;
		}

		if (m_ComputeShaderProgram != 0)
			rlUnloadShaderProgram(m_ComputeShaderProgram);

		// Delete SSBOs from VRAM
		if (m_ssboNodes != 0) rlUnloadShaderBuffer(m_ssboNodes);
		if (m_ssboTriangles != 0) rlUnloadShaderBuffer(m_ssboTriangles);
		if (m_ssboMaterials != 0) rlUnloadShaderBuffer(m_ssboMaterials);
		if (m_ssboPrimitives != 0) rlUnloadShaderBuffer(m_ssboPrimitives);
		if (m_ssboPointLights != 0) rlUnloadShaderBuffer(m_ssboPointLights);
		if (m_ssboDirLights != 0) rlUnloadShaderBuffer(m_ssboDirLights);
		if (m_ssboEmitters != 0) rlUnloadShaderBuffer(m_ssboEmitters);

		m_ssboNodes = 0;
		m_ssboTriangles = 0;
		m_ssboMaterials = 0;
		m_ssboPrimitives = 0;
		m_ssboPointLights = 0;
		m_ssboDirLights = 0;
		m_ssboEmitters = 0;

		m_AccumulationBuffer.clear();
		m_PixelData.clear();
		m_ActiveRegistry = nullptr;
	}

	void RaytracerSystem::resize(int width, int height) {
		if (width < 1)  width = 1;
		if (height < 1) height = 1;

		if (m_OutputTexture.id != 0) {
			UnloadTexture(m_OutputTexture);
			m_OutputTexture.id = 0;
		}

		m_Width = width;
		m_Height = height;

		m_AccumulationBuffer.assign(width * height, Vector3{ 0.0f, 0.0f, 0.0f });
		m_PixelData.assign(width * height, ::BLANK);

		Image img = GenImageColor(width, height, ::BLANK);
		m_OutputTexture = LoadTextureFromImage(img);
		UnloadImage(img);

		m_FrameCount = 1;
	}

	void RaytracerSystem::reset_accumulation(me::Registry* registry) {
		m_FrameCount = 1;
		std::fill(m_AccumulationBuffer.begin(), m_AccumulationBuffer.end(),
			Vector3{ 0.0f, 0.0f, 0.0f });

		if (registry) {
			m_BVH.build(*registry);
			flatten_scene(*registry);
			upload_to_gpu();
		}
	}

	bool RaytracerSystem::focus_on_ray(me::Registry& registry, const Vector3& origin, const Vector3& dir) {
		if (m_BVH.empty()) return false;

		Vector3 inv = {
			1.0f / (std::abs(dir.x) > 1e-8f ? dir.x : 1e-8f),
			1.0f / (std::abs(dir.y) > 1e-8f ? dir.y : 1e-8f),
			1.0f / (std::abs(dir.z) > 1e-8f ? dir.z : 1e-8f)
		};
		auto hit = m_BVH.traverse(origin, dir, inv, FLT_MAX, registry);
		if (!hit) return false;

		focus_distance = hit->hit_distance;
		reset_accumulation(); // re-accumulate against the new focal plane
		me::logger::info("Focus distance set to " + std::to_string(focus_distance));
		return true;
	}

	void RaytracerSystem::export_to_png(const std::string& filepath) {
		if (current_backend == RenderBackend::GPU && m_OutputTexture.id != 0) {
			m_PixelData.resize(m_Width * m_Height);
			auto* pixels = (::Color*)rlReadTexturePixels(
				m_OutputTexture.id, m_Width, m_Height, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
			if (pixels) {
				m_PixelData.assign(pixels, pixels + m_Width * m_Height);
				RL_FREE(pixels);
			}
		}

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

		// Lazy BVH build: handles the case where on_start fired before the scene was populated, so reset_accumulation had no registry to work with.
		if (m_BVH.empty()) {
			m_BVH.build(registry);
			flatten_scene(registry);
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


		if (current_backend == RenderBackend::GPU && m_ComputeShaderProgram != 0) {
			render_gpu_path(registry, camera, cam_transform);
		} else {
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
						x, y, m_Width, m_Height, camera, cam_transform, m_FrameCount, seed);

					// Trace and accumulate.
					Vector3 light = trace_ray(ray, 0, seed);

					// Firefly clamp: cap one sample's brightness so rare ultra-bright paths
					// don't leave permanent noise specks (0 disables).
					if (firefly_clamp > 0.0f) {
						float lum = 0.2126f * light.x + 0.7152f * light.y + 0.0722f * light.z;
						if (lum > firefly_clamp) {
							float s = firefly_clamp / lum;
							light.x *= s; light.y *= s; light.z *= s;
						}
					}

					m_AccumulationBuffer[index].x += light.x;
					m_AccumulationBuffer[index].y += light.y;
					m_AccumulationBuffer[index].z += light.z;

					Vector3 avg = {
						m_AccumulationBuffer[index].x / static_cast<float>(m_FrameCount),
						m_AccumulationBuffer[index].y / static_cast<float>(m_FrameCount),
						m_AccumulationBuffer[index].z / static_cast<float>(m_FrameCount)
					};

					// ---- Tonemapping (ACES Filmic) ----
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

			// CPU path: upload the pixel buffer to the texture.
			UpdateTexture(m_OutputTexture, m_PixelData.data());
		}
		// GPU path writes directly into m_OutputTexture via compute shader — no upload needed.

		if (accumulate) m_FrameCount++;
		m_TotalFramesRendered++;

		m_ActiveRegistry = nullptr;
	}

	me::raytracing::Ray RaytracerSystem::generate_camera_ray(
		int x, int y, int width, int height,
		const me::components::CameraComponent& camera,
		const me::components::TransformComponent& cam_transform,
		uint32_t frame_count, uint32_t& seed) {

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

		Vector3 dir = Vector3Normalize({
			fwd.x + right.x * vx + true_up.x * vy,
			fwd.y + right.y * vx + true_up.y * vy,
			fwd.z + right.z * vx + true_up.z * vy
		});

		// Pinhole camera unless an aperture is set.
		if (aperture <= 0.0f)
			return { cam_transform.position, dir };

		// Depth of field: jitter the ray origin over a lens disk and re-aim it at the
		// focal plane, so points off that plane blur. Accumulation averages it into bokeh.
		// The lens sample MUST be per-pixel (random_float(seed)) — using a per-frame
		// value would render every pixel through the same lens point each frame, making
		// the whole image lurch around between frames instead of blurring smoothly.
		Vector3 focal_point = Vector3Add(cam_transform.position, Vector3Scale(dir, focus_distance));
		float lens_r = aperture * std::sqrt(me::raytracing::random_float(seed));
		float lens_a = 2.0f * PI * me::raytracing::random_float(seed);
		Vector3 lens_offset = Vector3Add(
			Vector3Scale(right, std::cos(lens_a) * lens_r),
			Vector3Scale(true_up, std::sin(lens_a) * lens_r));
		Vector3 new_origin = Vector3Add(cam_transform.position, lens_offset);
		return { new_origin, Vector3Normalize(Vector3Subtract(focal_point, new_origin)) };
	}

	Vector3 RaytracerSystem::sky_color(const Vector3& dir) const {
		// Horizon-to-zenith gradient, scaled by the master intensity.
		// Mirrors sample_sky() in raytracer.comp so both backends match.
		float t = 0.5f * (dir.y + 1.0f);
		return Vector3{
			((1.0f - t) * sky_horizon_color.x + t * sky_zenith_color.x) * sky_intensity,
			((1.0f - t) * sky_horizon_color.y + t * sky_zenith_color.y) * sky_intensity,
			((1.0f - t) * sky_horizon_color.z + t * sky_zenith_color.z) * sky_intensity
		};
	}

	Vector3 RaytracerSystem::shadow_transmittance(const Vector3& origin, const Vector3& dir, float max_t) {
		Vector3 trans = { 1.0f, 1.0f, 1.0f };
		Vector3 o = origin;
		float remaining = max_t;

		// Walks the ray surface-by-surface: an opaque hit blocks fully (returns 0);
		// each glass interface loses light to Fresnel reflection (grazing rays
		// reflect away → the rim of a glass shadow darkens, even for clear glass);
		// each interior segment absorbs by Beer–Lambert, exp(-σ·d) == pow(color, d),
		// so tinted glass shadows scale with thickness. Mirrors the GPU backend.
		for (int k = 0; k < 8; ++k) {
			Vector3 inv = {
				1.0f / (std::abs(dir.x) > 1e-8f ? dir.x : 1e-8f),
				1.0f / (std::abs(dir.y) > 1e-8f ? dir.y : 1e-8f),
				1.0f / (std::abs(dir.z) > 1e-8f ? dir.z : 1e-8f)
			};
			auto hit = m_BVH.traverse(o, dir, inv, remaining, *m_ActiveRegistry);
			if (!hit) break; // nothing more in the way → the light is reached

			if (hit->transmission <= 0.0f) return { 0.0f, 0.0f, 0.0f }; // opaque blocker

			// Fresnel loss at the interface (Schlick r0 is symmetric in ior vs 1/ior).
			float cos_i = std::abs(Vector3DotProduct(dir, hit->normal));
			float f = 1.0f - me::raytracing::reflectance(cos_i, hit->ior);
			trans.x *= f; trans.y *= f; trans.z *= f;

			// A back-face hit means the segment just crossed was inside the glass.
			// tint_strength scales the absorption density (0 = always clear).
			if (!hit->front_face) {
				float d = hit->hit_distance * hit->tint_strength;
				trans.x *= std::pow(std::max(hit->base_color.x, 0.001f), d);
				trans.y *= std::pow(std::max(hit->base_color.y, 0.001f), d);
				trans.z *= std::pow(std::max(hit->base_color.z, 0.001f), d);
			}

			if (std::max({ trans.x, trans.y, trans.z }) < 0.01f) return { 0.0f, 0.0f, 0.0f };

			remaining -= (hit->hit_distance + 0.001f);
			if (remaining <= 0.001f) break;
			o = Vector3Add(hit->hit_point, Vector3Scale(dir, 0.001f));
		}
		return trans;
	}

	Vector3 RaytracerSystem::trace_ray(const me::raytracing::Ray& ray, int depth, uint32_t& seed, bool allow_emissive) {
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

		if (!hit) return sky_color(ray.direction);

		const HitPayload& p = *hit;
		Vector3 shadow_origin = Vector3Add(p.hit_point, Vector3Scale(p.normal, 0.001f));

		// ==========================================
		// 2. EMISSION — early out for light sources
		// ==========================================
		// Camera rays and specular/glass bounces "see" the emitter and show it
		// glowing. After a diffuse bounce, allow_emissive is false because Next
		// Event Estimation already added this emitter's contribution directly at
		// the previous surface — counting it again here would double the energy.
		if ((p.emission.x + p.emission.y + p.emission.z) > 0.0f)
			return allow_emissive ? p.emission : Vector3{ 0.0f, 0.0f, 0.0f };

		// ==========================================
		// 3. DIRECT LIGHTING (Point + Directional)
		// ==========================================
		Vector3 direct = { 0.0f, 0.0f, 0.0f };

		// ---- Point lights: sample a disk of `radius` for soft shadows; 1/d² falloff ----
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

			Vector3 to_l = Vector3Subtract(lt->position, shadow_origin);
			float   center_dist = Vector3Length(to_l);
			if (center_dist < 1e-4f) continue;
			Vector3 ldir_c = Vector3Scale(to_l, 1.0f / center_dist);

			// Sample a point on a disk of radius l.radius facing the receiver.
			Vector3 a = (std::abs(ldir_c.x) > 0.9f) ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
			Vector3 u = Vector3Normalize(Vector3CrossProduct(ldir_c, a));
			Vector3 vv = Vector3CrossProduct(ldir_c, u);
			float   rr = l.radius * std::sqrt(random_float(seed));
			float   phi = 2.0f * PI * random_float(seed);
			Vector3 sample_pos = Vector3Add(lt->position,
				Vector3Add(Vector3Scale(u, std::cos(phi) * rr), Vector3Scale(vv, std::sin(phi) * rr)));

			Vector3 lvec = Vector3Subtract(sample_pos, shadow_origin);
			float   ldist = Vector3Length(lvec);
			Vector3 ldir = Vector3Scale(lvec, 1.0f / ldist);
			float   ndotl = std::max(0.0f, Vector3DotProduct(p.normal, ldir));
			if (ndotl <= 0.0f) continue;

			Vector3 vis = shadow_transmittance(shadow_origin, ldir, ldist);
			float   falloff = 1.0f / std::max(ldist * ldist, 1e-4f);
			float   w = ndotl * l.intensity * falloff;
			direct.x += vis.x * lcolor.x * w;
			direct.y += vis.y * lcolor.y * w;
			direct.z += vis.z * lcolor.z * w;
		}

		// ---- Directional lights: cone-sample within `angular_radius` for soft shadows ----
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
			Vector3 ldir_base = { -fwd.x, -fwd.y, -fwd.z };

			float   cos_max = std::cos(dl.angular_radius * DEG2RAD);
			Vector3 ldir = sample_cone(ldir_base, cos_max, seed);
			float   ndotl = std::max(0.0f, Vector3DotProduct(p.normal, ldir));
			if (ndotl <= 0.0f) continue;

			Vector3 vis = shadow_transmittance(shadow_origin, ldir, FLT_MAX);
			direct.x += vis.x * lcolor.x * (ndotl * dl.intensity);
			direct.y += vis.y * lcolor.y * (ndotl * dl.intensity);
			direct.z += vis.z * lcolor.z * (ndotl * dl.intensity);
		}

		// ---- Emissive area lights (Next Event Estimation) ----
		// Each emitter is treated as a sphere light sampled at its center. Because
		// the sample point is deterministic (no RNG), this adds illumination from
		// glowing objects WITHOUT adding any noise — it converges instantly like
		// the point/directional lights do. The solid angle of the emitter's
		// bounding sphere drives a physically-plausible size/distance falloff.
		constexpr float INV_PI = 0.31830988618f;
		for (const auto& em : m_GPUEmitters) {
			Vector3 lvec = Vector3Subtract(em.position, shadow_origin);
			float   dist = Vector3Length(lvec);
			if (dist < 1e-4f) continue;
			Vector3 ldir_c = Vector3Scale(lvec, 1.0f / dist);

			// Solid angle subtended by a sphere of radius R seen from `dist`.
			float sin2 = std::min((em.radius * em.radius) / (dist * dist), 0.9999f);
			float cos_max = std::sqrt(1.0f - sin2);
			float omega = 2.0f * PI * (1.0f - cos_max);

			// Aim the shadow ray at a random point inside the emitter's cone (instead
			// of dead-center) so the penumbra softens as samples accumulate.
			Vector3 ldir = sample_cone(ldir_c, cos_max, seed);
			float   ndotl = std::max(0.0f, Vector3DotProduct(p.normal, ldir));
			if (ndotl <= 0.0f) continue;

			// Stop the shadow ray just before the emitter's near surface so it
			// doesn't register as an occluder of itself.
			float   shadow_max = dist - em.radius - 0.001f;
			Vector3 vis = { 1.0f, 1.0f, 1.0f };
			if (shadow_max > 0.001f) vis = shadow_transmittance(shadow_origin, ldir, shadow_max);

			float w = ndotl * omega * INV_PI;
			direct.x += vis.x * em.emission.x * w;
			direct.y += vis.y * em.emission.y * w;
			direct.z += vis.z * em.emission.z * w;
		}

		// Direct light is purely diffuse. Metals have no diffuse properties!
		direct.x *= p.roughness * (1.0f - p.metallic);
		direct.y *= p.roughness * (1.0f - p.metallic);
		direct.z *= p.roughness * (1.0f - p.metallic);

		// ==========================================
		// 4. AMBIENT (sky-derived, art-directable)
		// ==========================================
		Vector3 sky_amb = sky_color(p.normal);
		Vector3 ambient = {
			sky_amb.x * ambient_strength * p.roughness,
			sky_amb.y * ambient_strength * p.roughness,
			sky_amb.z * ambient_strength * p.roughness
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

		// Whether the NEXT hit is allowed to add emitter surface radiance.
		// Glass/mirror-like bounces keep it true (you should see emitters through
		// and reflected in them); diffuse bounces set it false below so NEE owns
		// the direct emitter lighting and we don't double-count.
		bool next_allow_emissive = true;

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

			// Near-mirror surfaces are specular enough to show emitters; rougher
			// surfaces are diffuse and already received emitter light via NEE.
			next_allow_emissive = (p.roughness < 0.1f);
		}

		Vector3 bounce_color = trace_ray({ bounce_origin, bounce_dir }, depth + 1, seed, next_allow_emissive);

		// --- RR ENERGY COMPENSATION ---
		bounce_color.x /= survival_p;
		bounce_color.y /= survival_p;
		bounce_color.z /= survival_p;

		// ---- Combine bounce contribution ----
		Vector3 bounce = { 0.0f, 0.0f, 0.0f };
		if (p.transmission > 0.0f) {
			// Beer–Lambert: tint accrues only while traveling INSIDE the glass (a
			// back-face hit means the segment just crossed was interior), so thick
			// tinted glass looks denser than thin. Entry hits add no tint — their
			// energy split is already handled by the Fresnel reflect/refract above.
			Vector3 absorb = { 1.0f, 1.0f, 1.0f };
			if (!p.front_face) {
				float d = p.hit_distance * p.tint_strength;
				absorb.x = std::pow(std::max(p.base_color.x, 0.001f), d);
				absorb.y = std::pow(std::max(p.base_color.y, 0.001f), d);
				absorb.z = std::pow(std::max(p.base_color.z, 0.001f), d);
			}
			bounce = { bounce_color.x * absorb.x, bounce_color.y * absorb.y, bounce_color.z * absorb.z };
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

	void RaytracerSystem::flatten_scene(me::Registry& registry) {
		m_GPUNodes.clear();
		m_GPUTriangles.clear();
		m_GPUMaterials.clear();
		m_GPUPrimitives.clear();
		m_GPUEmitters.clear();

		if (m_BVH.m_nodes.empty()) return;

		// ========================================================
		// 1. FLATTEN THE TOP-LEVEL BVH (TLAS)
		// ========================================================
		for (const auto& cpu_node : m_BVH.m_nodes) {
			me::render::gpu::GPUNode gpu_node;
			gpu_node.bounds_min = cpu_node.bounds.min;
			gpu_node.bounds_max = cpu_node.bounds.max;
			gpu_node.count = cpu_node.triangle_count;

			if (cpu_node.is_leaf()) {
				// Point directly to where the primitive will be in m_GPUPrimitives
				gpu_node.left_child = cpu_node.first_triangle_index;
			} else {
				gpu_node.left_child = cpu_node.left_child; // Point to next TLAS node
			}
			m_GPUNodes.push_back(gpu_node);
		}

		// ========================================================
		// 2. FLATTEN PRIMITIVES & MATERIALS (Sorted Order)
		// ========================================================
		for (uint32_t i = 0; i < m_BVH.m_prim_indices.size(); ++i) {
			uint32_t original_prim_idx = m_BVH.m_prim_indices[i];
			const auto& cpu_prim = m_BVH.m_prims[original_prim_idx];
			me::entity::entity_id e = cpu_prim.entity;

			me::render::gpu::GPUPrimitive gpu_prim{};
			gpu_prim.blas_root_index = -1; // Default to no BLAS

			auto* t = registry.try_get_component<me::components::TransformComponent>(e);
			auto* shape = registry.try_get_component<me::components::Shape3DComponent>(e);
			auto* model = registry.try_get_component<me::components::Model3DComponent>(e);
			auto* mat = registry.try_get_component<me::components::MaterialComponent>(e);

			// --- Setup Matrices ---
			if (t) {
				gpu_prim.inverse_model = MatrixInvert(t->model_matrix);
				gpu_prim.normal_matrix = MatrixTranspose(gpu_prim.inverse_model);
			}

			// --- Setup Material ---
			me::render::gpu::GPUMaterial gpu_mat{};
			if (mat) {
				gpu_mat.base_color = { std::pow(mat->albedo.r / 255.0f, 2.2f), std::pow(mat->albedo.g / 255.0f, 2.2f), std::pow(mat->albedo.b / 255.0f, 2.2f) };
				gpu_mat.roughness = mat->roughness;
				gpu_mat.metallic = mat->metallic;
				gpu_mat.emission = mat->emission_power;
				gpu_mat.transmission = mat->transmission;
				gpu_mat.ior = mat->ior;
				gpu_mat.tint_strength = mat->tint_strength;
			} else {
				me::Color base_col = shape ? shape->color : (model ? model->tint : me::Color::white);
				gpu_mat.base_color = { std::pow(base_col.r / 255.0f, 2.2f), std::pow(base_col.g / 255.0f, 2.2f), std::pow(base_col.b / 255.0f, 2.2f) };
				gpu_mat.roughness = 1.0f;
				gpu_mat.metallic = 0.0f;
				gpu_mat.emission = 0.0f;
				gpu_mat.transmission = 0.0f;
				gpu_mat.ior = 1.0f;
				gpu_mat.tint_strength = 1.0f;
			}
			gpu_prim.material_index = static_cast<int>(m_GPUMaterials.size());
			m_GPUMaterials.push_back(gpu_mat);

			// --- Collect emissive area lights (for Next Event Estimation) ---
			// Any object with a non-zero emission becomes a sphere light so it can
			// illuminate the rest of the scene. Infinite planes are skipped: their
			// bounding sphere would be enormous and behave like a sky-sized light.
			if (gpu_mat.emission > 0.0f && t) {
				bool is_plane = shape && shape->type == me::components::Shape3DComponent::Plane;
				if (!is_plane) {
					me::render::gpu::GPUEmitter em{};
					const auto& box = cpu_prim.aabb;
					em.position = box.centroid();

					if (shape && shape->type == me::components::Shape3DComponent::Sphere) {
						em.radius = t->scale.x; // exact sphere radius (engine convention)
					} else {
						// Cube / Model: bounding-sphere radius = half the AABB diagonal.
						Vector3 d = Vector3Subtract(box.max, box.min);
						em.radius = 0.5f * Vector3Length(d);
					}

					// Le = linear base color * emission power (already in gpu_mat).
					em.emission = {
						gpu_mat.base_color.x * gpu_mat.emission,
						gpu_mat.base_color.y * gpu_mat.emission,
						gpu_mat.base_color.z * gpu_mat.emission
					};
					m_GPUEmitters.push_back(em);
				}
			}

			// --- Setup Types & Models (BLAS) ---
			if (shape) {
				if (shape->type == me::components::Shape3DComponent::Sphere) gpu_prim.type = 0;
				else if (shape->type == me::components::Shape3DComponent::Plane) gpu_prim.type = 1;
				else if (shape->type == me::components::Shape3DComponent::Cube) gpu_prim.type = 2;
			} else if (model) {
				gpu_prim.type = 3;

				// We hit a 3D model! We must copy its entire BVH tree and all its triangles 
				// into our global GPU arrays, carefully shifting the pointers!
				const auto* blas = me::assets::internal_get_model_bvh(model->model);
				if (blas && !blas->m_nodes.empty()) {

					gpu_prim.blas_root_index = static_cast<int>(m_GPUNodes.size()); // Record where this model starts
					uint32_t triangle_offset = static_cast<uint32_t>(m_GPUTriangles.size());

					// Flatten BLAS Nodes
					for (const auto& b_node : blas->m_nodes) {
						me::render::gpu::GPUNode gb_node;
						gb_node.bounds_min = b_node.bounds.min;
						gb_node.bounds_max = b_node.bounds.max;
						gb_node.count = b_node.triangle_count;

						if (b_node.is_leaf()) {
							gb_node.left_child = b_node.first_triangle + triangle_offset; // Point to global triangle array
						} else {
							gb_node.left_child = b_node.left_child + gpu_prim.blas_root_index; // Point to global node array
						}
						m_GPUNodes.push_back(gb_node);
					}

					// Flatten BLAS Triangles
					for (uint32_t j = 0; j < blas->m_triangle_indices.size(); ++j) {
						const auto& tri = blas->m_triangles[blas->m_triangle_indices[j]];
						me::render::gpu::GPUTriangle g_tri;
						g_tri.v0 = tri.v0; g_tri.v1 = tri.v1; g_tri.v2 = tri.v2;
						m_GPUTriangles.push_back(g_tri);
					}
				}
			}

			m_GPUPrimitives.push_back(gpu_prim);
		}

		// ========================================================
		// 3. FLATTEN LIGHTS
		// ========================================================
		m_GPUPointLights.clear();
		m_GPUDirLights.clear();

		auto& light_pool = registry.view<me::components::LightComponent>();
		for (size_t i = 0; i < light_pool.size(); ++i) {
			auto* lt = registry.try_get_component<me::components::TransformComponent>(light_pool.entity_map[i]);
			if (!lt) continue;
			auto& l = light_pool.components[i];
			me::render::gpu::GPUPointLight gl{};
			gl.position = lt->position;
			gl.radius = l.radius;
			gl.color = { std::pow(l.color.r / 255.0f, 2.2f), std::pow(l.color.g / 255.0f, 2.2f), std::pow(l.color.b / 255.0f, 2.2f) };
			gl.intensity = l.intensity;
			m_GPUPointLights.push_back(gl);
		}

		auto& dir_pool = registry.view<me::components::DirectionalLightComponent>();
		for (size_t i = 0; i < dir_pool.size(); ++i) {
			auto* lt = registry.try_get_component<me::components::TransformComponent>(dir_pool.entity_map[i]);
			if (!lt) continue;
			auto& dl = dir_pool.components[i];
			float pitch = lt->rotation.x * DEG2RAD;
			float yaw = lt->rotation.y * DEG2RAD;
			Vector3 fwd = Vector3Normalize({ std::cos(pitch) * std::sin(yaw), -std::sin(pitch), std::cos(pitch) * std::cos(yaw) });
			me::render::gpu::GPUDirLight gd{};
			gd.direction = { -fwd.x, -fwd.y, -fwd.z }; // TOWARD the light
			gd.cos_angular = std::cos(dl.angular_radius * DEG2RAD);
			gd.color = { std::pow(dl.color.r / 255.0f, 2.2f), std::pow(dl.color.g / 255.0f, 2.2f), std::pow(dl.color.b / 255.0f, 2.2f) };
			gd.intensity = dl.intensity;
			m_GPUDirLights.push_back(gd);
		}

		me::logger::info("GPU Buffers Flattened: " + std::to_string(m_GPUNodes.size()) + " Nodes, " +
			std::to_string(m_GPUTriangles.size()) + " Triangles, " +
			std::to_string(m_GPUPrimitives.size()) + " Prims, " +
			std::to_string(m_GPUPointLights.size()) + " PointLights, " +
			std::to_string(m_GPUDirLights.size()) + " DirLights, " +
			std::to_string(m_GPUEmitters.size()) + " Emitters.");
	}

	void RaytracerSystem::upload_to_gpu() {
		// 1. Delete the old buffers from VRAM if they exist
		if (m_ssboNodes != 0) rlUnloadShaderBuffer(m_ssboNodes);
		if (m_ssboTriangles != 0) rlUnloadShaderBuffer(m_ssboTriangles);
		if (m_ssboMaterials != 0) rlUnloadShaderBuffer(m_ssboMaterials);
		if (m_ssboPrimitives != 0) rlUnloadShaderBuffer(m_ssboPrimitives);

		// 2. Load the new buffers into VRAM! 
		// (RL_DYNAMIC_DRAW tells the GPU we might change this data frequently)
		if (!m_GPUNodes.empty()) {
			m_ssboNodes = rlLoadShaderBuffer(static_cast<unsigned int>(m_GPUNodes.size() * sizeof(me::render::gpu::GPUNode)), m_GPUNodes.data(), RL_DYNAMIC_DRAW);
		} else {
			me::render::gpu::GPUNode dummy{};
			m_ssboNodes = rlLoadShaderBuffer(sizeof(dummy), &dummy, RL_DYNAMIC_DRAW);
		}

		if (!m_GPUTriangles.empty()) {
			m_ssboTriangles = rlLoadShaderBuffer(static_cast<unsigned int>(m_GPUTriangles.size() * sizeof(me::render::gpu::GPUTriangle)), m_GPUTriangles.data(), RL_DYNAMIC_DRAW);
		} else {
			me::render::gpu::GPUTriangle dummy{};
			m_ssboTriangles = rlLoadShaderBuffer(sizeof(dummy), &dummy, RL_DYNAMIC_DRAW);
		}

		if (!m_GPUMaterials.empty()) {
			m_ssboMaterials = rlLoadShaderBuffer(static_cast<unsigned int>(m_GPUMaterials.size() * sizeof(me::render::gpu::GPUMaterial)), m_GPUMaterials.data(), RL_DYNAMIC_DRAW);
		} else {
			me::render::gpu::GPUMaterial dummy{};
			m_ssboMaterials = rlLoadShaderBuffer(sizeof(dummy), &dummy, RL_DYNAMIC_DRAW);
		}

		if (!m_GPUPrimitives.empty()) {
			m_ssboPrimitives = rlLoadShaderBuffer(static_cast<unsigned int>(m_GPUPrimitives.size() * sizeof(me::render::gpu::GPUPrimitive)), m_GPUPrimitives.data(), RL_DYNAMIC_DRAW);
		} else {
			me::render::gpu::GPUPrimitive dummy{};
			m_ssboPrimitives = rlLoadShaderBuffer(sizeof(dummy), &dummy, RL_DYNAMIC_DRAW);
		}

		if (m_ssboPointLights != 0) rlUnloadShaderBuffer(m_ssboPointLights);
		if (!m_GPUPointLights.empty()) {
			m_ssboPointLights = rlLoadShaderBuffer(static_cast<unsigned int>(m_GPUPointLights.size() * sizeof(me::render::gpu::GPUPointLight)), m_GPUPointLights.data(), RL_DYNAMIC_DRAW);
		} else {
			me::render::gpu::GPUPointLight dummy{};
			m_ssboPointLights = rlLoadShaderBuffer(sizeof(dummy), &dummy, RL_DYNAMIC_DRAW);
		}

		if (m_ssboDirLights != 0) rlUnloadShaderBuffer(m_ssboDirLights);
		if (!m_GPUDirLights.empty()) {
			m_ssboDirLights = rlLoadShaderBuffer(static_cast<unsigned int>(m_GPUDirLights.size() * sizeof(me::render::gpu::GPUDirLight)), m_GPUDirLights.data(), RL_DYNAMIC_DRAW);
		} else {
			me::render::gpu::GPUDirLight dummy{};
			m_ssboDirLights = rlLoadShaderBuffer(sizeof(dummy), &dummy, RL_DYNAMIC_DRAW);
		}

		if (m_ssboEmitters != 0) rlUnloadShaderBuffer(m_ssboEmitters);
		if (!m_GPUEmitters.empty()) {
			m_ssboEmitters = rlLoadShaderBuffer(static_cast<unsigned int>(m_GPUEmitters.size() * sizeof(me::render::gpu::GPUEmitter)), m_GPUEmitters.data(), RL_DYNAMIC_DRAW);
		} else {
			me::render::gpu::GPUEmitter dummy{};
			m_ssboEmitters = rlLoadShaderBuffer(sizeof(dummy), &dummy, RL_DYNAMIC_DRAW);
		}

		me::logger::info("GPU SSBOs Uploaded Successfully.");
	}

	void RaytracerSystem::render_gpu_path(me::Registry& registry, const me::components::CameraComponent& camera, const me::components::TransformComponent& cam_transform) {

		rlEnableShader(m_ComputeShaderProgram);

		// 1. Send Camera Uniforms
		int loc_cam_pos = rlGetLocationUniform(m_ComputeShaderProgram, "cam_pos");
		int loc_cam_fwd = rlGetLocationUniform(m_ComputeShaderProgram, "cam_fwd");
		int loc_cam_up = rlGetLocationUniform(m_ComputeShaderProgram, "cam_up");
		int loc_cam_right = rlGetLocationUniform(m_ComputeShaderProgram, "cam_right");
		int loc_cam_fov = rlGetLocationUniform(m_ComputeShaderProgram, "cam_fov_scale");
		int loc_frame_count = rlGetLocationUniform(m_ComputeShaderProgram, "frame_count");
		int loc_total_frames = rlGetLocationUniform(m_ComputeShaderProgram, "total_frames");
		int loc_max_bounces = rlGetLocationUniform(m_ComputeShaderProgram, "max_bounces");

		float fov_scale = std::tan((camera.fov * 0.5f) * DEG2RAD);
		Vector3 fwd = Vector3Normalize(Vector3Subtract({ camera.target.x, camera.target.y, camera.target.z }, cam_transform.position));
		Vector3 up = Vector3Normalize({ camera.up.x, camera.up.y, camera.up.z });
		Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, up));
		Vector3 true_up = Vector3CrossProduct(right, fwd);
		int max_b = max_bounces; // keep GPU in sync with the CPU / UI setting

		rlSetUniform(loc_cam_pos, &cam_transform.position, RL_SHADER_UNIFORM_VEC3, 1);
		rlSetUniform(loc_cam_fwd, &fwd, RL_SHADER_UNIFORM_VEC3, 1);
		rlSetUniform(loc_cam_up, &true_up, RL_SHADER_UNIFORM_VEC3, 1);
		rlSetUniform(loc_cam_right, &right, RL_SHADER_UNIFORM_VEC3, 1);
		rlSetUniform(loc_cam_fov, &fov_scale, RL_SHADER_UNIFORM_FLOAT, 1);
		rlSetUniform(loc_frame_count, &m_FrameCount, RL_SHADER_UNIFORM_UINT, 1);
		rlSetUniform(loc_total_frames, &m_TotalFramesRendered, RL_SHADER_UNIFORM_UINT, 1);
		rlSetUniform(loc_max_bounces, &max_b, RL_SHADER_UNIFORM_INT, 1);

		// 2. Bind SSBOs (binding numbers must match the shader)
		rlBindShaderBuffer(m_ssboNodes, 1);
		rlBindShaderBuffer(m_ssboTriangles, 2);
		rlBindShaderBuffer(m_ssboMaterials, 3);
		rlBindShaderBuffer(m_ssboPrimitives, 4);
		rlBindShaderBuffer(m_ssboPointLights, 5);
		rlBindShaderBuffer(m_ssboDirLights, 6);
		rlBindShaderBuffer(m_ssboEmitters, 7);

		int num_point = (int)m_GPUPointLights.size();
		int num_dir = (int)m_GPUDirLights.size();
		int num_emit = (int)m_GPUEmitters.size();
		int loc_npl = rlGetLocationUniform(m_ComputeShaderProgram, "num_point_lights");
		int loc_ndl = rlGetLocationUniform(m_ComputeShaderProgram, "num_dir_lights");
		int loc_nem = rlGetLocationUniform(m_ComputeShaderProgram, "num_emitters");
		rlSetUniform(loc_npl, &num_point, RL_SHADER_UNIFORM_INT, 1);
		rlSetUniform(loc_ndl, &num_dir, RL_SHADER_UNIFORM_INT, 1);
		rlSetUniform(loc_nem, &num_emit, RL_SHADER_UNIFORM_INT, 1);

		// Environment uniforms (sky gradient + ambient fill) — match the CPU path.
		int loc_amb = rlGetLocationUniform(m_ComputeShaderProgram, "ambient_strength");
		int loc_sky_h = rlGetLocationUniform(m_ComputeShaderProgram, "sky_horizon");
		int loc_sky_z = rlGetLocationUniform(m_ComputeShaderProgram, "sky_zenith");
		int loc_sky_i = rlGetLocationUniform(m_ComputeShaderProgram, "sky_intensity");
		rlSetUniform(loc_amb, &ambient_strength, RL_SHADER_UNIFORM_FLOAT, 1);
		rlSetUniform(loc_sky_h, &sky_horizon_color, RL_SHADER_UNIFORM_VEC3, 1);
		rlSetUniform(loc_sky_z, &sky_zenith_color, RL_SHADER_UNIFORM_VEC3, 1);
		rlSetUniform(loc_sky_i, &sky_intensity, RL_SHADER_UNIFORM_FLOAT, 1);

		// Camera / lens uniforms (tonemap exposure + depth of field).
		int loc_exposure = rlGetLocationUniform(m_ComputeShaderProgram, "exposure");
		int loc_aperture = rlGetLocationUniform(m_ComputeShaderProgram, "aperture");
		int loc_focus = rlGetLocationUniform(m_ComputeShaderProgram, "focus_distance");
		rlSetUniform(loc_exposure, &exposure, RL_SHADER_UNIFORM_FLOAT, 1);
		rlSetUniform(loc_aperture, &aperture, RL_SHADER_UNIFORM_FLOAT, 1);
		rlSetUniform(loc_focus, &focus_distance, RL_SHADER_UNIFORM_FLOAT, 1);
		int loc_firefly = rlGetLocationUniform(m_ComputeShaderProgram, "firefly_clamp");
		rlSetUniform(loc_firefly, &firefly_clamp, RL_SHADER_UNIFORM_FLOAT, 1);

		// 3. Bind the Output Texture
		// We tell OpenGL: "Take m_OutputTexture, and let the Compute Shader write directly into its memory!"
		rlBindImageTexture(m_OutputTexture.id, 0, m_OutputTexture.format, false);

		// 4. DISPATCH — in horizontal row bands, never one giant dispatch.
		// A single full-image dispatch at export resolutions (with high bounce
		// counts) can run longer than the OS GPU watchdog allows (~2s on
		// Windows/TDR); the driver then resets and the GL context dies, which
		// kills the app. Banding keeps each dispatch short and preemptible.
		// The per-dispatch pixel budget shrinks as bounces rise, since the
		// per-pixel cost scales with them. Viewport-sized images still fit in
		// a single dispatch, so the interactive path is unchanged.
		int group_x = (int)std::ceil(m_Width / 8.0f);

		const int budget_px = 2'000'000 / std::max(1, max_bounces);
		int band_rows = std::max(1, budget_px / std::max(1, m_Width));
		band_rows = std::max(8, (band_rows / 8) * 8); // align to the 8x8 workgroup

		int loc_row_off = rlGetLocationUniform(m_ComputeShaderProgram, "row_offset");
		for (int y = 0; y < m_Height; y += band_rows) {
			int rows = std::min(band_rows, m_Height - y);
			rlSetUniform(loc_row_off, &y, RL_SHADER_UNIFORM_INT, 1);
			rlComputeShaderDispatch(group_x, (unsigned int)std::ceil(rows / 8.0f), 1);
		}

		rlDisableShader();
	}

} // namespace me::systems