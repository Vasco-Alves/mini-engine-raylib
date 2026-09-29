// Dev tool: renders a scene file headlessly through the editor's path tracer and
// reports the time per sample.
//
// It drives RaytracerSystem exactly as File > Export to PNG does (same settings,
// same accumulation loop, same PNG writer), from the editor's default camera, so
// its images match an export from the editor. Uses: reproducible figures, and
// timing the CPU and GPU backends on the same scene at equal settings.
//
//   render_bench <scene.json> <out.png> [options]
//     --size W H         output resolution            (default 1080 1080)
//     --samples N        samples per pixel            (default 64)
//     --bounces N        max bounces                  (default 8, as the export)
//     --backend cpu|gpu  path tracer backend          (default gpu, as the editor)
//     --exposure X  --sky X  --ambient X              (Raytracer Settings)
//     --aperture X  --focus D                         depth of field (0 = pinhole)
//     --camera X Y Z TX TY TZ [FOV]                   view, instead of the editor's default
//     --no-gi  --no-soft-shadows  --no-reflections  --no-refraction
//     --game DIR         project assets folder, for scenes that load models
//     --rasterizer       draw one frame with the real-time renderer (the viewport's)
//                        instead of path tracing it; --no-shadows drops the shadow pass
//
// The GPU backend needs a GL 4.3 context, so the tool opens a hidden window and
// is not registered with CTest. Time it in the Release build.

#include <raylib.h>
#include <rlgl.h>

#include <mini-engine-raylib/core/engine.hpp>
#include <mini-engine-raylib/core/vfs.hpp>
#include <mini-engine-raylib/ecs/components.hpp>
#include <mini-engine-raylib/render/renderer.hpp>
#include <mini-engine-raylib/scene/scene_manager.hpp>
#include <mini-engine-raylib/systems/raytracer_system.hpp>
#include <mini-engine-raylib/systems/transform_system.hpp>
#include <mini-ecs/registry.hpp>

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using Clock = std::chrono::steady_clock;
using me::systems::RenderBackend;

#ifdef _WIN32
// Hybrid-GPU laptops: ask the NVIDIA/AMD drivers for the discrete GPU. A new
// executable otherwise gets the integrated one, and the GPU timings with it.
extern "C" {
	__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace {

	// Prints warnings and the GL vendor/renderer lines, so every run states
	// which GPU it measured.
	void trace_log(int level, const char* text, va_list args) {
		char line[512];
		std::vsnprintf(line, sizeof(line), text, args);
		if (level >= LOG_WARNING || std::strstr(line, "> Vendor:") || std::strstr(line, "> Renderer:"))
			std::fprintf(stderr, "%s\n", line);
	}

	void usage() {
		std::fprintf(stderr,
			"usage: render_bench <scene.json> <out.png> [--size W H] [--samples N] [--bounces N]\n"
			"       [--backend cpu|gpu] [--exposure X] [--sky X] [--ambient X] [--no-gi]\n"
			"       [--no-soft-shadows] [--no-reflections] [--no-refraction] [--game DIR]\n"
			"       [--aperture X] [--focus D] [--camera X Y Z TX TY TZ [FOV]]\n");
	}

	double ms_since(Clock::time_point t0) {
		return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
	}

}

int main(int argc, char** argv) {
	if (argc < 3) { usage(); return 2; }
	const std::string scene = argv[1];
	const std::string out = argv[2];

	int width = 1080, height = 1080, samples = 64, bounces = 8;
	RenderBackend backend = RenderBackend::GPU;
	float exposure = -1.0f, sky = -1.0f, ambient = -1.0f; // < 0: keep the default
	float aperture = 0.0f, focus = -1.0f;
	bool gi = true, soft = true, refl = true, refr = true;
	bool rasterizer = false, shadows = true;
	std::string game_dir;

	// The editor's default view (EditorApp::m_EditorCamera / m_EditorCameraTransform).
	Vector3 eye = { 7.07f, 5.0f, 7.07f }, target = { 0.0f, 0.0f, 0.0f };
	float fov = 45.0f;

	for (int i = 3; i < argc; ++i) {
		const char* a = argv[i];
		auto next = [&]() -> const char* {
			if (i + 1 >= argc) { usage(); std::exit(2); }
			return argv[++i];
		};
		if (!std::strcmp(a, "--size")) { width = std::atoi(next()); height = std::atoi(next()); }
		else if (!std::strcmp(a, "--samples")) samples = std::atoi(next());
		else if (!std::strcmp(a, "--bounces")) bounces = std::atoi(next());
		else if (!std::strcmp(a, "--backend")) backend = std::strcmp(next(), "cpu") ? RenderBackend::GPU : RenderBackend::CPU;
		else if (!std::strcmp(a, "--exposure")) exposure = (float)std::atof(next());
		else if (!std::strcmp(a, "--sky")) sky = (float)std::atof(next());
		else if (!std::strcmp(a, "--ambient")) ambient = (float)std::atof(next());
		else if (!std::strcmp(a, "--aperture")) aperture = (float)std::atof(next());
		else if (!std::strcmp(a, "--focus")) focus = (float)std::atof(next());
		else if (!std::strcmp(a, "--camera")) {
			eye = { (float)std::atof(next()), (float)std::atof(next()), (float)std::atof(next()) };
			target = { (float)std::atof(next()), (float)std::atof(next()), (float)std::atof(next()) };
			if (i + 1 < argc && argv[i + 1][0] != '-') fov = (float)std::atof(next());
		}
		else if (!std::strcmp(a, "--no-gi")) gi = false;
		else if (!std::strcmp(a, "--no-soft-shadows")) soft = false;
		else if (!std::strcmp(a, "--no-reflections")) refl = false;
		else if (!std::strcmp(a, "--no-refraction")) refr = false;
		else if (!std::strcmp(a, "--rasterizer")) rasterizer = true;
		else if (!std::strcmp(a, "--no-shadows")) shadows = false;
		else if (!std::strcmp(a, "--game")) game_dir = next();
		else { usage(); return 2; }
	}
	if (width < 1 || height < 1 || samples < 1 || bounces < 1) { usage(); return 2; }

	SetTraceLogCallback(trace_log);
	SetTraceLogLevel(LOG_INFO);
	SetConfigFlags(FLAG_WINDOW_HIDDEN);
	me::AppConfig config;
	config.title = "render_bench";
	config.width = 64;
	config.height = 64;
	if (!me::init(config)) return 1;

	me::vfs::mount("engine", ME_ENGINE_ASSETS);
	if (!game_dir.empty()) me::vfs::mount("game", game_dir);

	if (!me::scene_manager::load(scene)) {
		std::fprintf(stderr, "could not load %s\n", scene.c_str());
		return 1;
	}
	me::systems::transform_update(); // world matrices, before the BVH is built

	me::components::CameraComponent camera;
	camera.target = target;
	camera.fov = fov;
	me::components::TransformComponent camera_transform;
	camera_transform.position = eye;

	// --- One frame of the real-time renderer, as the editor's viewport draws it ---
	if (rasterizer) {
		me::render::init();
		RenderTexture2D rt_tex = LoadRenderTexture(width, height);
		auto t0 = Clock::now();
		// The shadow pass binds its own framebuffer, so it runs before the target.
		if (shadows) me::render::render_shadows(eye);
		BeginTextureMode(rt_tex);
		me::render::clear_world(me::Color{ 30, 30, 30, 255 });
		me::render::render_world(&camera_transform, &camera);
		EndTextureMode();
		double frame_ms = ms_since(t0);

		Image img = LoadImageFromTexture(rt_tex.texture);
		ImageFlipVertical(&img); // the render texture is stored bottom-up
		ExportImage(img, out.c_str());
		UnloadImage(img);
		UnloadRenderTexture(rt_tex);
		std::printf("%s | %dx%d | rasterizador | sombras %s | %.1f ms\n",
			out.c_str(), width, height, shadows ? "sí" : "no", frame_ms);
		me::render::shutdown();
		CloseWindow();
		return 0;
	}

	me::systems::RaytracerSystem rt;
	rt.current_backend = backend;
	if (exposure >= 0.0f) rt.exposure = exposure;
	if (sky >= 0.0f) rt.sky_intensity = sky;
	if (ambient >= 0.0f) rt.ambient_strength = ambient;
	rt.aperture = aperture;
	if (focus >= 0.0f) rt.focus_distance = focus;
	rt.enable_indirect = gi;
	rt.enable_soft_shadows = soft;
	rt.enable_reflections = refl;
	rt.enable_refraction = refr;

	rt.on_start(width, height);
	if (backend == RenderBackend::GPU && rt.get_texture()->id == 0) {
		std::fprintf(stderr, "no output texture\n");
		return 1;
	}

	// Export configuration (EditorApp's "Render & Save").
	rt.accumulate = true;
	rt.preview_samples = samples;
	rt.max_bounces = bounces;

	auto t_build = Clock::now();
	rt.reset_accumulation(&me::get_registry()); // BVH + GPU buffers
	double build_ms = ms_since(t_build);

	auto t0 = Clock::now();
	while (rt.get_sample_count() < samples)
		rt.on_update(me::get_registry(), camera, camera_transform);
	if (backend == RenderBackend::GPU) {
		// Dispatches are asynchronous: a readback waits for all of them.
		Texture2D* tex = rt.get_texture();
		void* px = rlReadTexturePixels(tex->id, tex->width, tex->height, tex->format);
		RL_FREE(px);
	}
	double render_ms = ms_since(t0);
	rt.export_to_png(out);

	std::printf("%s | %dx%d | %d spp | %d bounces | %s | GI %s | build %.1f ms | "
		"render %.1f ms | %.3f ms/sample\n",
		out.c_str(), width, height, rt.get_sample_count(), bounces,
		backend == RenderBackend::GPU ? "GPU" : "CPU", gi ? "on" : "off",
		build_ms, render_ms, render_ms / rt.get_sample_count());

	rt.on_stop();
	CloseWindow();
	return 0;
}
