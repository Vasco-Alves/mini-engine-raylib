#pragma once

#include <raylib.h>
#include <mini-ecs/registry.hpp>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/render/bvh.hpp>
#include <vector>
#include <string>

namespace me::systems {

	class RaytracerSystem {
	public:
		RaytracerSystem();
		~RaytracerSystem();

		void on_start(int width, int height);
		void on_update(me::Registry& registry,
			const me::components::CameraComponent& camera,
			const me::components::TransformComponent& cam_transform);
		void on_stop();

		Texture2D* get_texture() { return &m_OutputTexture; }
		void       export_to_png(const std::string& filepath);

		// Pass the registry so the BVH can be rebuilt whenever the scene changes.
		// Passing nullptr just resets the frame counter without a rebuild
		// (useful for the non-accumulation real-time noisy mode).
		void reset_accumulation(me::Registry* registry = nullptr);

		int  get_accumulated_frames() const { return m_FrameCount; }
		int  get_width()  const { return m_Width; }
		int  get_height() const { return m_Height; }

	public:
		// --- Viewport Settings ---
		bool  accumulate = false;
		int   preview_samples = 50;
		float resolution_scale = 0.5f;

		// --- Lighting/Math Settings ---
		int max_bounces = 3;

		// --- Export Settings ---
		int export_width = 1920;
		int export_height = 1080;
		int export_samples = 50;

		uint32_t m_TotalFramesRendered = 0;

	private:
		// trace_ray no longer takes Registry& — it reads m_ActiveRegistry,
		// which is set for the lifetime of a single on_update call.
		Vector3 trace_ray(const me::raytracing::Ray& ray, int depth, uint32_t& seed);

		me::raytracing::Ray generate_camera_ray(
			int x, int y, int width, int height,
			const me::components::CameraComponent& camera,
			const me::components::TransformComponent& cam_transform, uint32_t frame_count);

	private:
		int m_Width = 0;
		int m_Height = 0;
		int m_FrameCount = 1;

		std::vector<Vector3> m_AccumulationBuffer;
		std::vector<::Color> m_PixelData;
		Texture2D            m_OutputTexture;

		// BVH — rebuilt on reset_accumulation(registry)
		me::raytracing::BVH m_BVH;

		// Valid only during on_update — lets trace_ray reach the registry and BVH
		// without passing them through every recursive bounce call.
		me::Registry* m_ActiveRegistry = nullptr;
	};

} // namespace me::systems