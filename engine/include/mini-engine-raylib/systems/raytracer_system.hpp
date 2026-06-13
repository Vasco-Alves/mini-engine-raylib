#pragma once

#include <raylib.h>
#include <mini-ecs/registry.hpp>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/render/bvh.hpp>
#include <mini-engine-raylib/render/gpu_types.hpp>
#include <vector>
#include <string>

namespace me::systems {

	enum class RenderBackend { CPU, GPU };

	// One-click feature bundles. Each is a shortcut for a combination of the
	// four feature toggles below. The key insight: every noise source in a path
	// tracer is a *random* sample, so turning the stochastic features off
	// (soft shadows, indirect/GI) gives a noise-free image AND a faster one.
	//   Full — everything (reference quality, needs accumulation to converge)
	//   Lite — no GI, hard shadows; keeps (deterministic) mirrors + sharp glass.
	//          Noise-free at 1 sample, still has the "ray-traced" look.
	//   Flat — direct light + hard shadows only. Fastest, flattest, zero noise.
	enum class RenderQualityPreset { Full, Lite, Flat };

	class RaytracerSystem {
	public:
		RaytracerSystem();
		~RaytracerSystem();

		void on_start(int width, int height);
		void on_update(me::Registry& registry, const me::components::CameraComponent& camera, const me::components::TransformComponent& cam_transform);
		void on_stop();

		// Renders one real-time game frame: rebuilds the scene structures (things
		// moved since last frame), then accumulates play_samples_per_frame paths
		// per pixel from the given camera. Call once per frame while a game is
		// running; get_texture() holds the finished frame afterwards.
		void render_realtime_frame(me::Registry& registry, const me::components::CameraComponent& camera, const me::components::TransformComponent& cam_transform);

		// Recreates the output texture at a new resolution without reloading
		// the compute shader or rebuilding SSBOs. Use this for all resize events.
		void resize(int width, int height);

		Texture2D* get_texture() { return &m_OutputTexture; }
		void       export_to_png(const std::string& filepath);

		// Sets focus_distance from the nearest scene hit along a ray (click-to-focus).
		// Returns true if something was hit. Requires the BVH (built in render mode).
		bool focus_on_ray(me::Registry& registry, const Vector3& origin, const Vector3& dir);

		// Pass the registry so the BVH can be rebuilt whenever the scene changes.
		// Passing nullptr just resets the frame counter without a rebuild
		// (useful for the non-accumulation real-time noisy mode).
		void reset_accumulation(me::Registry* registry = nullptr);

		int  get_accumulated_frames() const { return m_FrameCount; }
		int  get_width()  const { return m_Width; }
		int  get_height() const { return m_Height; }

	public:
		// --- Viewport Settings ---
		RenderBackend current_backend = RenderBackend::GPU;
		bool  accumulate = false;
		int   preview_samples = 50;
		float resolution_scale = 0.5f;

		// --- Real-time play settings ---
		// Used by render_realtime_frame (the editor's "RT Play" mode and the
		// raytraced game runtime). Noise falls with sqrt(samples); low internal
		// resolutions (small resolution_scale) are what make high counts affordable.
		int play_samples_per_frame = 8;
		// Reuse the same sample seeds every frame: the residual noise freezes into
		// a stable dither pattern instead of animated static — the difference
		// between "retro dithered look" and "broken TV".
		bool lock_noise_pattern = true;

		// --- Lighting/Math Settings ---
		int max_bounces = 3;

		// --- Feature toggles (shared by both backends and exported games) ---
		// Each off both removes work AND removes a noise source (when stochastic):
		//   soft_shadows off → shadow rays aim at the light center → HARD, noise-free
		//   indirect off     → no diffuse GI bounce (the biggest noise + cost source)
		//   reflections off  → opaque surfaces don't bounce specularly (no mirrors)
		//   refraction off   → glass renders as a solid matte object (no see-through)
		// Mirror reflections and sharp (roughness-0) glass are deterministic, so
		// "indirect + soft shadows off, reflections + refraction on" is still
		// noise-free — see RenderQualityPreset.
		bool enable_soft_shadows = true;
		bool enable_indirect = true;
		bool enable_reflections = true;
		bool enable_refraction = true;

		// Sets the four toggles above from a preset (does not touch unrelated
		// settings like aperture; for a fully sharp image keep Aperture at 0).
		void apply_quality_preset(RenderQualityPreset preset) {
			switch (preset) {
			case RenderQualityPreset::Full:
				enable_soft_shadows = enable_indirect = enable_reflections = enable_refraction = true;
				break;
			case RenderQualityPreset::Lite:
				enable_soft_shadows = false; enable_indirect = false;
				enable_reflections = true;   enable_refraction = true;
				break;
			case RenderQualityPreset::Flat:
				enable_soft_shadows = false; enable_indirect = false;
				enable_reflections = false;  enable_refraction = false;
				break;
			}
		}

		// --- Environment Settings (shared by the CPU and GPU backends) ---
		// Defaults reproduce the classic outdoor-sky look (white horizon -> blue
		// zenith with a subtle ambient fill). Kept as named constants so there is a
		// single source of truth and the UI can offer a one-click reset.
		static constexpr float   DEFAULT_AMBIENT_STRENGTH = 0.03f;
		static constexpr Vector3 DEFAULT_SKY_HORIZON = { 1.0f, 1.0f, 1.0f };
		static constexpr Vector3 DEFAULT_SKY_ZENITH = { 0.5f, 0.7f, 1.0f };
		static constexpr float   DEFAULT_SKY_INTENSITY = 1.0f;

		// Ambient fill applied to every surface from the sky gradient. It ignores
		// occlusion, so it is the brightness floor you can never get below.
		// Set to 0 for true black where no direct/indirect light reaches.
		float   ambient_strength = DEFAULT_AMBIENT_STRENGTH;
		// Sky gradient sampled by escaping rays (the background) and by the ambient
		// term: horizon at the bottom of the dome, zenith at the top.
		Vector3 sky_horizon_color = DEFAULT_SKY_HORIZON;
		Vector3 sky_zenith_color = DEFAULT_SKY_ZENITH;
		// Master multiplier on the sky brightness (background + ambient source).
		// Set to 0 for a pure black background.
		float   sky_intensity = DEFAULT_SKY_INTENSITY;

		// Restores the four environment settings above to their defaults.
		void reset_environment() {
			ambient_strength = DEFAULT_AMBIENT_STRENGTH;
			sky_horizon_color = DEFAULT_SKY_HORIZON;
			sky_zenith_color = DEFAULT_SKY_ZENITH;
			sky_intensity = DEFAULT_SKY_INTENSITY;
		}

		// --- Camera / Tonemap (shared by both backends) ---
		float exposure = 0.6f;        // tonemap brightness before the ACES curve
		float aperture = 0.0f;        // lens radius for depth of field; 0 = pinhole (all sharp)
		float focus_distance = 10.0f; // distance to the in-focus plane
		float firefly_clamp = 20.0f;  // cap per-sample brightness to kill noise specks; 0 = off

		// --- Export Settings ---
		// Used only while exporting; the viewport's own resolution, sample
		// target and bounce count are saved and restored around the export.
		int export_width = 1920;
		int export_height = 1080;
		int export_samples = 50;
		int export_bounces = 8;

		uint32_t m_TotalFramesRendered = 0;

	private:
		// allow_emissive lets the integrator suppress an emitter's surface
		// contribution after a diffuse bounce (it was already counted by Next
		// Event Estimation at the previous shading point), avoiding double-counting.
		Vector3 trace_ray(const me::raytracing::Ray& ray, int depth, uint32_t& seed, bool allow_emissive = true);

		// Visibility along a shadow ray: colored transmittance reaching the light
		// (zero if an opaque surface blocks it; tinted by any glass it crosses).
		Vector3 shadow_transmittance(const Vector3& origin, const Vector3& dir, float max_t);

		// Evaluates the art-directable sky gradient (horizon -> zenith -> intensity).
		// Used both as the background for escaping rays and as the ambient source.
		// dir must be unit length.
		Vector3 sky_color(const Vector3& dir) const;

		me::raytracing::Ray generate_camera_ray(int x, int y, int width, int height, const me::components::CameraComponent& camera, const me::components::TransformComponent& cam_transform, uint32_t frame_count, uint32_t& seed);

	private:
		int m_Width = 0;
		int m_Height = 0;
		int m_FrameCount = 1;

		std::vector<Vector3> m_AccumulationBuffer;
		std::vector<::Color> m_PixelData;
		Texture2D            m_OutputTexture;

		// BVH — rebuilt on reset_accumulation(registry)
		me::raytracing::BVH m_BVH;

		// Valid only during on_update — lets trace_ray reach the registry and BVH without passing them through every recursive bounce call.
		me::Registry* m_ActiveRegistry = nullptr;

		// --- The GPU Flattener ---
		void flatten_scene(me::Registry& registry);
		void upload_to_gpu();

		// Last logged flatten stats — flattening is per-frame in RT Play, so the
		// stats only get logged when they change.
		std::string m_LastFlattenSummary;

		std::vector<me::render::gpu::GPUNode>       m_GPUNodes;
		std::vector<me::render::gpu::GPUTriangle>   m_GPUTriangles;
		std::vector<me::render::gpu::GPUMaterial>   m_GPUMaterials;
		std::vector<me::render::gpu::GPUPrimitive>  m_GPUPrimitives;
		std::vector<me::render::gpu::GPUPointLight> m_GPUPointLights;
		std::vector<me::render::gpu::GPUDirLight>   m_GPUDirLights;
		std::vector<me::render::gpu::GPUEmitter>    m_GPUEmitters;

		// --- SSBO IDs (VRAM Pointers) ---
		unsigned int m_ssboNodes = 0;
		unsigned int m_ssboTriangles = 0;
		unsigned int m_ssboMaterials = 0;
		unsigned int m_ssboPrimitives = 0;
		unsigned int m_ssboPointLights = 0;
		unsigned int m_ssboDirLights = 0;
		unsigned int m_ssboEmitters = 0;

		// --- GPU Compute Variables ---
		unsigned int m_ComputeShaderProgram = 0;
		void render_gpu_path(me::Registry& registry, const me::components::CameraComponent& camera, const me::components::TransformComponent& cam_transform);
	};

} // namespace me::systems