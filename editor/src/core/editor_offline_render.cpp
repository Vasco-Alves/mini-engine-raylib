#include "editor/core/editor_app.hpp"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <mini-engine-raylib/core/engine.hpp>
#include <mini-engine-raylib/core/file_system.hpp>
#include <mini-engine-raylib/core/logger.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>

// Offline rendering: the PNG export and the animation render. Both flows own
// the raytracer while they run (m_IsExporting pauses the viewport pass and the
// viewport-size tracker), render at their own export resolution/samples, and
// restore the viewport configuration when they finish or get cancelled.

namespace editor {

	// "game://scenes/foo.json" -> "game://scenes/foo.anim.json"
	std::string EditorApp::anim_sidecar_path() const {
		std::string path = m_CurrentScenePath;
		if (auto pos = path.rfind(".json"); pos != std::string::npos) path.erase(pos);
		return path + ".anim.json";
	}

	void EditorApp::save_animation_sidecar() {
		if (m_CurrentScenePath.empty()) return;

		// Refresh each track's saved name from the live Tag (entities are
		// referenced by name in the sidecar, so renames must carry over).
		auto& reg = me::get_registry();
		for (auto& tr : m_Animation.entity_tracks) {
			if (tr.entity == me::entity::null || !reg.is_alive(tr.entity)) continue;
			if (auto* tag = reg.try_get_component<me::components::TagComponent>(tr.entity))
				if (!tag->name.empty()) tr.entity_name = tag->name;
		}

		auto physical = me::fs::resolve_if_virtual(anim_sidecar_path());

		if (m_Animation.empty()) {
			// No track: don't leave a stale sidecar behind.
			std::error_code ec;
			std::filesystem::remove(physical, ec);
			return;
		}
		std::ofstream ofs(physical);
		if (ofs) ofs << m_Animation.to_json().dump(2);
	}

	void EditorApp::load_animation_sidecar() {
		m_Animation = {};
		m_AnimationPanel.stop_preview();
		if (m_CurrentScenePath.empty()) return;

		std::ifstream ifs(me::fs::resolve_if_virtual(anim_sidecar_path()));
		if (!ifs) return;
		try {
			nlohmann::json j;
			ifs >> j;
			m_Animation.from_json(j);
			// Entity ids change on every scene load — re-bind tracks by name.
			m_Animation.resolve_entities(me::get_registry());
		} catch (...) {
			me::logger::warn("Could not parse animation sidecar: " + anim_sidecar_path());
		}
	}

	// Switches the raytracer to the export configuration and hands rendering to
	// the "Rendering Animation" popup, which renders frame 0..N-1 back to back.
	void EditorApp::start_animation_render() {
		// Output folder: renders/anim_<timestamp>/frame_0001.png ...
		std::string renders_dir = (m_ProjectPath / "renders").string();
		if (!me::fs::exists(renders_dir)) me::fs::create_directory(renders_dir);

		auto now = std::time(nullptr);
		char time_str[64];
		std::tm tm_buf{};
#ifdef _WIN32
		localtime_s(&tm_buf, &now);
#else
		localtime_r(&now, &tm_buf);
#endif
		std::strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", &tm_buf);
		m_AnimOutDir = (m_ProjectPath / "renders" / (std::string("anim_") + time_str)).string();
		if (!me::fs::exists(m_AnimOutDir)) me::fs::create_directory(m_AnimOutDir);

		// Sanitize the settings (shared with the PNG export).
		if (m_Raytracer.export_width < 1)  m_Raytracer.export_width = 1;
		if (m_Raytracer.export_height < 1) m_Raytracer.export_height = 1;
		if (m_Raytracer.export_samples < 2) m_Raytracer.export_samples = 2;
		if (m_Raytracer.export_bounces < 1)  m_Raytracer.export_bounces = 1;
		if (m_Raytracer.export_bounces > 16) m_Raytracer.export_bounces = 16;
		if (m_AnimFps < 1)   m_AnimFps = 1;
		if (m_AnimFps > 240) m_AnimFps = 240;

		// Save the viewport state the render overrides (same fields as the PNG
		// export — the two offline flows are mutually exclusive).
		m_PreviewW = m_Raytracer.get_width();
		m_PreviewH = m_Raytracer.get_height();
		m_ExportSavedPreviewSamples = m_Raytracer.preview_samples;
		m_ExportSavedAccumulate = m_Raytracer.accumulate;
		m_ExportSavedBounces = m_Raytracer.max_bounces;

		m_Raytracer.resize(m_Raytracer.export_width, m_Raytracer.export_height);
		m_Raytracer.accumulate = true;
		m_Raytracer.preview_samples = m_Raytracer.export_samples;
		m_Raytracer.max_bounces = m_Raytracer.export_bounces;

		float dur = m_Animation.duration();
		m_AnimTotalFrames = dur > 0.0f ? (int)std::ceil(dur * (float)m_AnimFps) + 1 : 1;
		m_AnimRenderFrame = 0;

		// Put the playhead on frame 0 and start accumulating.
		apply_animation_at(0.0f);

		m_IsExporting = true;     // pauses the viewport pass + resolution tracker
		m_IsRenderingAnim = true; // routes progress UI to the animation popup
	}

	void EditorApp::apply_animation_at(float t) {
		if (!m_Animation.keys.empty())
			apply_keyframe(m_Animation.evaluate(t), m_EditorCameraTransform, m_EditorCamera, m_Raytracer);
		if (!m_Animation.env_keys.empty())
			apply_environment(m_Animation.evaluate_env(t), m_Raytracer);

		if (m_Animation.entity_tracks.empty()) {
			m_Raytracer.reset_accumulation();
		} else {
			apply_entity_tracks(m_Animation, t, me::get_registry());
			me::world_update(0.0f); // rebuild model matrices before the BVH
			m_Raytracer.reset_accumulation(&me::get_registry());
		}
	}

	// All offline-render popups: the export/animation settings dialogs and the
	// two progress dialogs that actually drive the renders chunk by chunk.
	void EditorApp::draw_offline_render_modals() {
		// ==========================================
		// 2. EXPORT RENDER MODAL (Settings)
		// ==========================================
		if (m_ShowExportModal) ImGui::OpenPopup("Export High-Res Render");
		if (ImGui::BeginPopupModal("Export High-Res Render", &m_ShowExportModal, ImGuiWindowFlags_AlwaysAutoResize)) {

			ImGui::Text("Export Settings");
			ImGui::Separator();
			ImGui::Dummy(ImVec2(0, 5));

			// These apply ONLY to the export — the viewport keeps its own
			// resolution / samples / bounces and gets them back afterwards.
			ImGui::InputInt("Width", &m_Raytracer.export_width);
			ImGui::InputInt("Height", &m_Raytracer.export_height);
			ImGui::InputInt("Samples (Rays per Pixel)", &m_Raytracer.export_samples);
			ImGui::InputInt("Max Bounces", &m_Raytracer.export_bounces);

			// Workload estimate — helps pick a backend before committing. GPU
			// work is dispatched in short row bands (so a heavy frame can't trip
			// the OS GPU watchdog), but very large exports still tax the UI.
			{
				double mpx = (double)m_Raytracer.export_width * (double)m_Raytracer.export_height / 1e6;
				double work = mpx * (double)std::max(1, m_Raytracer.export_bounces);
				int vram_mb = (int)(mpx * 8.0) + 1; // output image + readback staging

				ImGui::Dummy(ImVec2(0, 6));
				ImGui::Separator();
				ImGui::TextDisabled("Estimate: %.1f MP/sample  |  ~%d MB VRAM  |  %d samples",
					mpx, vram_mb, m_Raytracer.export_samples);

				if (m_Raytracer.current_backend == me::systems::RenderBackend::GPU) {
					if (work > 120.0) {
						ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
							"Very heavy GPU export — the UI will be sluggish while it runs.");
						ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
							"If your GPU driver still resets, switch the Backend to CPU.");
					} else if (work > 40.0) {
						ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f),
							"Heavy GPU export — rendered in bands; the UI may stutter.");
					}
				} else if (work > 8.0) {
					ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f),
						"CPU at this size will be slow — the GPU backend renders in");
					ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f),
						"watchdog-safe bands and is usually the faster, safe choice.");
				}
			}

			ImGui::Dummy(ImVec2(0, 10));

			if (ImGui::Button("Render & Save", ImVec2(120, 0))) {

				// 1. Create directory and filename
				std::string renders_dir = (m_ProjectPath / "renders").string();
				if (!me::fs::exists(renders_dir)) me::fs::create_directory(renders_dir);

				auto now = std::time(nullptr);
				char time_str[64];
				std::tm tm_buf{};
#ifdef _WIN32
				localtime_s(&tm_buf, &now);
#else
				localtime_r(&now, &tm_buf);
#endif
				std::strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", &tm_buf);
				m_ExportPath = (m_ProjectPath / "renders" / (std::string("render_") + time_str + ".png")).string();

				// 2. Sanitize the export settings (InputInt allows anything).
				if (m_Raytracer.export_width < 1)  m_Raytracer.export_width = 1;
				if (m_Raytracer.export_height < 1) m_Raytracer.export_height = 1;
				if (m_Raytracer.export_samples < 2) m_Raytracer.export_samples = 2;
				if (m_Raytracer.export_bounces < 1)  m_Raytracer.export_bounces = 1;
				if (m_Raytracer.export_bounces > 16) m_Raytracer.export_bounces = 16;

				// 3. Save the viewport state the export overrides.
				m_PreviewW = m_Raytracer.get_width();
				m_PreviewH = m_Raytracer.get_height();
				m_ExportSavedPreviewSamples = m_Raytracer.preview_samples;
				m_ExportSavedAccumulate = m_Raytracer.accumulate;
				m_ExportSavedBounces = m_Raytracer.max_bounces;

				// 4. Switch the raytracer to the export configuration. resize()
				// preserves the shader + SSBOs and restarts accumulation; the
				// viewport pass and the viewport-size tracker pause while
				// m_IsExporting is set, so the export owns every render.
				m_Raytracer.resize(m_Raytracer.export_width, m_Raytracer.export_height);
				m_Raytracer.accumulate = true;
				m_Raytracer.preview_samples = m_Raytracer.export_samples;
				m_Raytracer.max_bounces = m_Raytracer.export_bounces;

				// 5. Hand rendering over to the progress modal.
				m_IsExporting = true;

				m_ShowExportModal = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				m_ShowExportModal = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		// ==========================================
		// RENDER ANIMATION MODAL (Settings)
		// ==========================================
		if (m_ShowAnimRenderModal) ImGui::OpenPopup("Render Animation");
		if (ImGui::BeginPopupModal("Render Animation", &m_ShowAnimRenderModal, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Animation Settings");
			ImGui::Separator();
			ImGui::Dummy(ImVec2(0, 5));

			ImGui::InputInt("Width", &m_Raytracer.export_width);
			ImGui::InputInt("Height", &m_Raytracer.export_height);
			ImGui::InputInt("Samples per Frame", &m_Raytracer.export_samples);
			ImGui::InputInt("Max Bounces", &m_Raytracer.export_bounces);
			ImGui::InputInt("Frame Rate (FPS)", &m_AnimFps);

			int show_fps = std::max(1, m_AnimFps);
			int frames = (int)std::ceil(m_Animation.duration() * (float)show_fps) + 1;
			ImGui::Dummy(ImVec2(0, 6));
			ImGui::Separator();
			ImGui::TextDisabled("Duration: %.2fs  ->  %d frames at %d fps",
				m_Animation.duration(), frames, show_fps);
			ImGui::TextDisabled("Output: renders/anim_<timestamp>/frame_0001.png ...");
			ImGui::TextDisabled("Join:   ffmpeg -framerate %d -i frame_%%04d.png -c:v libx264 -pix_fmt yuv420p out.mp4", show_fps);

			ImGui::Dummy(ImVec2(0, 10));
			if (ImGui::Button("Start Render", ImVec2(120, 0))) {
				start_animation_render();
				m_ShowAnimRenderModal = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) { m_ShowAnimRenderModal = false; ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}

		// ==========================================
		// EXPORTING PROGRESS BAR (The Render Loop)
		// ==========================================
		if (m_IsExporting && !m_IsRenderingAnim) ImGui::OpenPopup("Rendering...");

		if (ImGui::BeginPopupModal("Rendering...", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {

			// Restores everything the export overrode. resize() back to the
			// viewport resolution also restarts the viewport accumulation.
			auto finish_export = [&]() {
				m_Raytracer.resize(m_PreviewW, m_PreviewH);
				m_Raytracer.preview_samples = m_ExportSavedPreviewSamples;
				m_Raytracer.accumulate = m_ExportSavedAccumulate;
				m_Raytracer.max_bounces = m_ExportSavedBounces;
				m_IsExporting = false;
			};

			ImGui::Text("Rendering High-Resolution Image (%dx%d)", m_Raytracer.export_width, m_Raytracer.export_height);
			ImGui::Dummy(ImVec2(0, 5));

			// Progress reads the raytracer's own accumulation counter, so the
			// bar can never drift from what was actually rendered.
			int done = m_Raytracer.get_accumulated_frames();
			float progress = (float)done / (float)m_Raytracer.export_samples;
			if (progress > 1.0f) progress = 1.0f;
			ImGui::ProgressBar(progress, ImVec2(300, 20));
			ImGui::Text("Calculating sample: %d / %d", done, m_Raytracer.export_samples);

			// Render a chunk of samples per UI frame, sized so each UI frame does
			// roughly constant work: small exports batch up to 10 samples, huge
			// ones take 1 per frame so the Cancel button stays responsive.
			long long px = (long long)m_Raytracer.export_width * (long long)m_Raytracer.export_height;
			int samples_per_frame = (int)std::clamp(4'000'000LL / std::max(1LL, px), 1LL, 10LL);
			for (int i = 0; i < samples_per_frame
				&& m_Raytracer.get_accumulated_frames() < m_Raytracer.export_samples; ++i) {
				m_Raytracer.on_update(me::get_registry(), m_EditorCamera, m_EditorCameraTransform);
			}

			if (m_Raytracer.get_accumulated_frames() >= m_Raytracer.export_samples) {
				m_Raytracer.export_to_png(m_ExportPath);
				finish_export();
				ImGui::CloseCurrentPopup();
			}

			ImGui::Dummy(ImVec2(0, 5));
			if (ImGui::Button("Cancel", ImVec2(300, 0))) {
				finish_export(); // no PNG written
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		// ==========================================
		// ANIMATION RENDER PROGRESS (frame-by-frame loop)
		// ==========================================
		if (m_IsRenderingAnim) ImGui::OpenPopup("Rendering Animation");
		if (ImGui::BeginPopupModal("Rendering Animation", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {

			auto finish_anim = [&]() {
				m_Raytracer.resize(m_PreviewW, m_PreviewH);
				m_Raytracer.preview_samples = m_ExportSavedPreviewSamples;
				m_Raytracer.accumulate = m_ExportSavedAccumulate;
				m_Raytracer.max_bounces = m_ExportSavedBounces;
				m_IsRenderingAnim = false;
				m_IsExporting = false;
			};

			float fps = (float)std::max(1, m_AnimFps);
			ImGui::Text("Rendering %d frames at %dx%d", m_AnimTotalFrames, m_Raytracer.export_width, m_Raytracer.export_height);
			ImGui::Dummy(ImVec2(0, 5));

			float frame_prog = m_AnimTotalFrames > 0 ? (float)m_AnimRenderFrame / (float)m_AnimTotalFrames : 0.0f;
			ImGui::ProgressBar(frame_prog, ImVec2(320, 20));
			ImGui::Text("Frame %d / %d (t = %.2fs)", m_AnimRenderFrame + 1, m_AnimTotalFrames,
				std::min((float)m_AnimRenderFrame / fps, m_Animation.duration()));

			int done_samples = m_Raytracer.get_accumulated_frames();
			float sample_prog = (float)done_samples / (float)m_Raytracer.export_samples;
			if (sample_prog > 1.0f) sample_prog = 1.0f;
			ImGui::ProgressBar(sample_prog, ImVec2(320, 14));
			ImGui::Text("Sample %d / %d", done_samples, m_Raytracer.export_samples);

			// Same adaptive pacing as the PNG export: constant work per UI frame.
			long long px = (long long)m_Raytracer.export_width * (long long)m_Raytracer.export_height;
			int spf = (int)std::clamp(4'000'000LL / std::max(1LL, px), 1LL, 10LL);
			for (int i = 0; i < spf
				&& m_Raytracer.get_accumulated_frames() < m_Raytracer.export_samples; ++i) {
				m_Raytracer.on_update(me::get_registry(), m_EditorCamera, m_EditorCameraTransform);
			}

			// Frame converged -> save it, move the playhead to the next frame.
			if (m_Raytracer.get_accumulated_frames() >= m_Raytracer.export_samples) {
				char frame_name[64];
				snprintf(frame_name, sizeof(frame_name), "frame_%04d.png", m_AnimRenderFrame + 1);
				m_Raytracer.export_to_png((std::filesystem::path(m_AnimOutDir) / frame_name).string());

				m_AnimRenderFrame++;
				if (m_AnimRenderFrame >= m_AnimTotalFrames) {
					me::logger::info("Animation rendered to: " + m_AnimOutDir);
					me::logger::info("Join it with: ffmpeg -framerate " + std::to_string(m_AnimFps)
						+ " -i frame_%04d.png -c:v libx264 -pix_fmt yuv420p out.mp4");
					finish_anim();
					ImGui::CloseCurrentPopup();
				} else {
					float t = std::min((float)m_AnimRenderFrame / fps, m_Animation.duration());
					apply_animation_at(t);
				}
			}

			ImGui::Dummy(ImVec2(0, 5));
			if (ImGui::Button("Cancel", ImVec2(320, 0))) {
				me::logger::info("Animation render cancelled. Finished frames kept in: " + m_AnimOutDir);
				finish_anim();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

} // namespace editor
