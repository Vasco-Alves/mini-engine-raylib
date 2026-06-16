#include "editor/core/editor_app.hpp"
#include "editor/core/entity_commands.hpp"
#include "editor/utils/file_dialogs.hpp"

#include <imgui.h>
#include <rlImGui.h>
#include <ImGuizmo.h>
#include <raymath.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <cstdio>

#include <mini-engine-raylib/core/engine.hpp>
#include <mini-engine-raylib/core/version.hpp>
#include <mini-engine-raylib/core/vfs.hpp>
#include <mini-engine-raylib/core/file_system.hpp>
#include <mini-engine-raylib/core/events.hpp>
#include <mini-engine-raylib/core/logger.hpp>
#include <mini-engine-raylib/input/input.hpp>
#include <mini-engine-raylib/audio/audio.hpp>
#include <mini-engine-raylib/render/renderer.hpp>
#include <mini-engine-raylib/scene/scene_manager.hpp>
#include <mini-engine-raylib/ecs/script_component.hpp>
#include <mini-engine-raylib/ecs/audio_components.hpp>
#include <mini-engine-raylib/ecs/physics_components.hpp>
#include <mini-engine-raylib/ecs/component_registry.hpp>
#include <mini-engine-raylib/systems/camera_system.hpp>
#include <mini-engine-raylib/systems/physics_system.hpp>
#include <mini-engine-raylib/systems/audio_system.hpp>

namespace editor {

	void EditorApp::on_start() {
		me::input::bind_digital_axis("MoveY", me::input::Key::Q, me::input::Key::E, 1.0f);

		// Mount the root folder (where the executable lives)
		me::vfs::mount("root", ".");

		// Mount the engine assets folder
		me::vfs::mount("engine", "assets");

		// Window/taskbar icon (the .exe file icon is baked in via resources/app.rc).
		{
			Image icon = LoadImage("assets/icon.png");
			if (icon.data) {
				ImageFormat(&icon, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
				SetWindowIcon(icon);
				UnloadImage(icon);
			}
		}

		rlImGuiSetup(true);
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		// Explicit-save layout model: disable ImGui's continuous imgui.ini
		// autosave. The layout you see on startup is the last one you SAVED
		// (layout_edit.ini), falling back to the default shipped with the app —
		// so a fresh install looks the same as the author's setup.
		ImGui::GetIO().IniFilename = nullptr;
		{
			auto user_file = me::vfs::resolve("root://layout_edit.ini");
			if (std::filesystem::exists(user_file)) m_PendingLayoutLoad = "root://layout_edit.ini";
			else m_PendingLayoutLoad = "root://assets/layouts/layout_edit.ini"; // skipped if absent
		}

		apply_theme();

		subcribe_events();

		// Wire up the Project Hub Callbacks
		m_HubPanel.on_project_open = [this](const std::filesystem::path& p) { load_project(p); };
		m_HubPanel.on_project_create = [this](const std::filesystem::path& p) { create_project(p); };
		m_HubPanel.on_recent_remove = [this](const std::string& p) {
			m_RecentProjects.erase(std::remove(m_RecentProjects.begin(), m_RecentProjects.end(), p), m_RecentProjects.end());
			m_HubPanel.set_recent_projects(m_RecentProjects);
			save_engine_config();
		};

		load_engine_config();
		m_ViewportPanel.on_start();
		apply_frame_pacing();

		me::render::init();
		me::physics::init();

		// Any edit, undo or redo marks the scene dirty (window-title "*") and, in
		// Render mode, restarts the raytracer accumulation.
		m_CommandHistory.on_scene_changed = [this]() {
			m_SceneDirty = true;
			if (m_SceneState == SceneState::Render) {
				m_Raytracer.reset_accumulation(&me::get_registry());
			}
			};
	}

	void EditorApp::on_shutdown() {
		me::physics::shutdown();
		me::render::shutdown();
		m_ViewportPanel.on_shutdown();
		rlImGuiShutdown();
	}

	void EditorApp::subcribe_events() {
		auto& bus = me::get_event_bus();
		bus.subscribe<me::events::LogEvent>([](auto* e) { ConsolePanel::add_log(e->message, e->level); });
		bus.subscribe<me::events::SceneLoadedEvent>([this](auto* e) {
			// Every load rebuilds all entities under new ids, so the undo
			// history (and the selection) can't survive the swap.
			m_CommandHistory.Clear();
			m_HierarchyPanel.set_selected_entity(me::entity::null);
			// A load during play is a runtime transition (Lua's Scene.load), not
			// an editing operation: leave the current-scene path, the dirty flag
			// and the animation sidecar alone — otherwise Ctrl+S after stopping
			// would write the restored edit scene over the script-loaded file.
			if (me::is_playing()) return;
			if (e->filepath.find(".temp_play.json") == std::string::npos) {
				m_CurrentScenePath = e->filepath;
				m_SceneDirty = false; // freshly loaded == on-disk state
				load_animation_sidecar();
				remember_last_scene(); // reopen this scene next time the project loads
			}
			});
		bus.subscribe<me::events::SceneOpenRequestEvent>([this](auto* e) {
			request_open_scene(e->filepath);
			});
		bus.subscribe<me::events::EntitySelectedEvent>([this](auto* e) {
			m_HierarchyPanel.set_selected_entity(e->entity_id);

			auto& reg = me::get_registry();
			if (auto* t = reg.try_get_component<me::components::TransformComponent>(e->entity_id)) {
				m_OrbitTarget = { t->position.x, t->position.y, t->position.z };
			}
			});

		bus.subscribe<me::events::PlayStateChangedEvent>([](auto* e) {});
		bus.subscribe<me::events::PauseStateChangedEvent>([](auto* e) {});

		// Lua's Engine.quit(): in the editor that means "leave play mode" — but it
		// fires mid-script-update, and stopping reloads the scene (destroying the
		// script pool being iterated). Park it; on_update honors it after the frame.
		bus.subscribe<me::events::QuitRequestedEvent>([this](auto*) {
			if (me::is_playing()) m_QuitToEditorRequested = true;
			});
	}

	void EditorApp::on_resize(int width, int height) {}

	void EditorApp::on_update(float dt) {
		if (me::input::action_pressed("Quit") && !ImGui::GetIO().WantCaptureKeyboard) {
			request_exit(); // prompts about unsaved changes instead of dropping them
		}

		if (!m_IsProjectLoaded) return;

		// A script's Engine.quit() arrived during world_update; stop play now,
		// at a safe point outside the script iteration.
		if (m_QuitToEditorRequested) {
			m_QuitToEditorRequested = false;
			if (m_SceneState == SceneState::Play) on_stop();
		}

		// During Play, the *game* owns the view + mouse only when the scene has
		// an active camera. With no scene camera the play view falls back to the
		// editor fly-cam (see on_render), so that camera must stay drivable.
		const bool game_owns_view = (m_SceneState == SceneState::Play) && has_active_scene_camera();

		// Playtest focus: click the viewport to hand input + cursor to the game;
		// press Escape to take them back for the editor. The gate suppresses the
		// game's input and frees the cursor while unfocused (ImGui/raylib calls
		// here are editor-level, never gated). Done before poll_shortcuts so the
		// Escape that unfocuses doesn't also reach anything else.
		if (game_owns_view) {
			if (!m_PlaytestFocused) {
				if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && m_ViewportPanel.is_hovered()) {
					m_PlaytestFocused = true;
					me::input::set_input_gate(true); // game gets input; its lock intent applies
				}
			} else if (IsKeyPressed(KEY_ESCAPE)) {
				m_PlaytestFocused = false;
				me::input::set_input_gate(false); // free the mouse for the editor
			}
		} else if (m_SceneState == SceneState::Play) {
			// Play with no scene camera: the editor fly-cam is the view, so keep
			// input flowing to it (gate open) and skip the focus handover.
			m_PlaytestFocused = false;
			if (!me::input::is_input_gate_open()) me::input::set_input_gate(true);
		}

		if (m_FlySpeedToastTimer > 0.0f) m_FlySpeedToastTimer -= dt;

		// Animation preview playback (moves the editor camera along the track).
		// Paused while an offline render owns the raytracer.
		if (!m_IsExporting) {
			m_AnimationPanel.update_preview(dt, m_Animation,
				m_EditorCameraTransform, m_EditorCamera, m_Raytracer,
				m_SceneState == SceneState::Render);
		}

		// OS-Level File Dropping
		if (IsFileDropped()) {
			FilePathList dropped_files = LoadDroppedFiles();
			std::filesystem::path target_dir = m_BrowserPanel.get_current_directory();

			for (unsigned int i = 0; i < dropped_files.count; i++) {
				try {
					std::filesystem::copy(dropped_files.paths[i], target_dir / std::filesystem::path(dropped_files.paths[i]).filename(),
						std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
					me::logger::info("Imported: " + std::filesystem::path(dropped_files.paths[i]).filename().string());
				} catch (const std::exception& e) {
					me::logger::error("Failed to import file: " + std::string(e.what()));
				}
			}
			UnloadDroppedFiles(dropped_files);
		}

		// Editor Camera Flying. Runs whenever the editor fly-cam owns the view:
		// always in Edit/Render, and in Play only when the scene has no active
		// camera (game_owns_view is false). When the game owns the view its scene
		// camera is in charge and the mouse belongs to the game, so the RMB-to-fly
		// grab must not run here or right-clicking would steal/release the cursor.
		if (!game_owns_view) {
			if (m_ViewportPanel.is_hovered() && me::input::action_pressed("MouseRight")) {
				m_IsFlying = true;
				me::input::lock_cursor();
			}

			if (m_IsFlying) {
				// Scroll while flying tunes the base fly speed (Shift/Ctrl in the
				// camera system give a temporary sprint/precision modifier on top).
				float wheel = GetMouseWheelMove();
				if (wheel != 0.0f) {
					m_EditorCamera.move_speed = std::clamp(m_EditorCamera.move_speed * (1.0f + 0.15f * wheel), 0.5f, 100.0f);
					m_FlySpeedToastTimer = 1.25f; // show the new speed briefly
				}

				bool orbiting = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
				if (orbiting)
					me::camera::orbit_editor_camera(m_EditorCameraTransform, m_EditorCamera, m_OrbitTarget, dt);
				else
					me::camera::update_editor_camera(m_EditorCameraTransform, m_EditorCamera, dt);

				if (me::input::action_released("MouseRight")) {
					m_IsFlying = false;
					me::input::unlock_cursor();
				}
			}
		}

		// ==========================================
		// RAYTRACER UPDATE
		// ==========================================
		// While exporting, the progress modal drives the renders at the export
		// resolution — pause the viewport pass so all time goes to the export.
		if (m_SceneState == SceneState::Render && !m_IsExporting) {
			m_Raytracer.on_update(me::get_registry(), m_EditorCamera, m_EditorCameraTransform);
		}

		// RT Play mode: one burst of path-traced samples per frame, rendered from
		// the scene's active camera (the same view the raster play pass would use).
		if (m_SceneState == SceneState::Play && m_RaytracePlayMode && !m_IsExporting) {
			auto& reg = me::get_registry();
			const me::components::TransformComponent* view_t = &m_EditorCameraTransform;
			const me::components::CameraComponent* view_c = &m_EditorCamera;
			for (auto [e, cam] : reg.view<me::components::CameraComponent>()) {
				if (!cam.active) continue;
				if (auto* t = reg.try_get_component<me::components::TransformComponent>(e)) {
					view_t = t;
					view_c = &cam;
					break;
				}
			}
			m_Raytracer.render_realtime_frame(reg, *view_c, *view_t);
		}

		poll_shortcuts();
		update_window_title();

		// ==========================================
		// 3D AUDIO SPATIAL UPDATE
		// ==========================================
		// Calculate the Editor Camera's forward vector
		Vector3 editor_pos = { m_EditorCameraTransform.position.x, m_EditorCameraTransform.position.y, m_EditorCameraTransform.position.z };
		Vector3 editor_target = { m_EditorCamera.target.x, m_EditorCamera.target.y, m_EditorCamera.target.z };
		Vector3 editor_forward = Vector3Normalize(Vector3Subtract(editor_target, editor_pos));
		Vector3 editor_up = { m_EditorCamera.up.x, m_EditorCamera.up.y, m_EditorCamera.up.z };

		me::systems::audio_update(me::get_registry(), editor_pos, editor_forward, editor_up);
	}

	void EditorApp::on_render() {
		if (!m_IsProjectLoaded) {
			m_HubPanel.on_imgui_render();
			return;
		}

		// Choose the viewport camera: the editor fly-cam while editing; the active
		// scene CameraComponent while playing (falling back to the editor cam if the
		// scene has none, so the view never goes black).
		const me::components::TransformComponent* view_t = &m_EditorCameraTransform;
		const me::components::CameraComponent*    view_c = &m_EditorCamera;
		if (m_SceneState == SceneState::Play) {
			auto& reg = me::get_registry();
			for (auto [e, cam] : reg.view<me::components::CameraComponent>()) {
				if (!cam.active) continue;
				if (auto* t = reg.try_get_component<me::components::TransformComponent>(e)) {
					view_t = t;
					view_c = &cam;
					break;
				}
			}
		}

		// 1. Shadow pass — binds its own depth framebuffer, so it must run BEFORE
		// the viewport texture is bound. Skipped in Render mode (the path tracer
		// owns the view and computes its own shadows).
		if (m_SceneState != SceneState::Render)
			me::render::render_shadows({ view_t->position.x, view_t->position.y, view_t->position.z });

		// 2. Render the 3D World to the Viewport Texture
		m_ViewportPanel.begin_render();

		me::render::clear_world(me::Color{ 30, 30, 30, 255 });
		me::render::render_world(view_t, view_c);

		// --- Pack ECS data into a raw Raylib struct to draw the grid & gizmos ---
		Camera3D gridCam = { 0 };
		gridCam.position = view_t->world_position();
		gridCam.target = { view_c->target.x, view_c->target.y, view_c->target.z };
		gridCam.up = { view_c->up.x, view_c->up.y, view_c->up.z };
		gridCam.fovy = view_c->fov;
		gridCam.projection = (view_c->projection == 1) ? CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE;

		BeginMode3D(gridCam);

		// Draw the baseline editor grid (edit mode only — keep the play view clean)
		if (m_SceneState == SceneState::Edit) DrawGrid(100, 1.0f);

		// ==========================================
		// DRAW EDITOR GIZMOS (Lights, Cameras, etc.)
		// ==========================================
		if (m_SceneState == SceneState::Edit) {
			auto& reg = me::get_registry();

			// 1. Point Light Gizmos (A simple wireframe sphere)
			for (auto [e, l, t] : reg.view<me::components::LightComponent, me::components::TransformComponent>()) {
				(void)e;
				::Color c = { l.color.r, l.color.g, l.color.b, 255 };
				DrawSphereWires(t.world_position(), 0.25f, 8, 8, c);
			}

			// 2. Directional Light Gizmo (A sphere with a directional arrow)
			for (auto [e, dl, t] : reg.view<me::components::DirectionalLightComponent, me::components::TransformComponent>()) {
				(void)e;
				::Color c = { dl.color.r, dl.color.g, dl.color.b, 255 };
				Vector3 pos = t.world_position();

				// Calculate the exact forward direction based on rotation
				float pitch = t.rotation.x * DEG2RAD;
				float yaw = t.rotation.y * DEG2RAD;
				Vector3 dir = {
					std::cos(pitch) * std::sin(yaw),
					-std::sin(pitch),
					std::cos(pitch) * std::cos(yaw)
				};
				dir = Vector3Normalize(dir);

				// Draw the Sun origin and the line pointing out
				DrawSphereWires(pos, 0.5f, 12, 12, c);

				Vector3 endPos = { pos.x + dir.x * 3.0f, pos.y + dir.y * 3.0f, pos.z + dir.z * 3.0f };
				DrawLine3D(pos, endPos, c);

				// Draw a tiny solid sphere at the end to act as an arrowhead
				DrawSphere(endPos, 0.15f, c);
			}

			// 3. Camera Gizmos (a wireframe body + the view frustum toward the target)
			for (auto [e, cam, t] : reg.view<me::components::CameraComponent, me::components::TransformComponent>()) {
				(void)e;

				// The active (rendering) camera draws warm yellow; inactive ones gray.
				::Color c = cam.active ? ::Color{ 245, 200, 70, 255 } : ::Color{ 150, 150, 150, 255 };
				Vector3 pos = t.world_position();

				// Camera body
				DrawCubeWires(pos, 0.45f, 0.35f, 0.6f, c);

				// View basis from the look-at target (what the camera actually renders).
				Vector3 fwd = Vector3Subtract({ cam.target.x, cam.target.y, cam.target.z }, pos);
				if (Vector3LengthSqr(fwd) < 1e-6f) fwd = { 0.0f, 0.0f, 1.0f };
				fwd = Vector3Normalize(fwd);
				Vector3 up_ref = Vector3Normalize({ cam.up.x, cam.up.y, cam.up.z });
				Vector3 right = Vector3CrossProduct(fwd, up_ref);
				if (Vector3LengthSqr(right) < 1e-6f) right = { 1.0f, 0.0f, 0.0f };
				right = Vector3Normalize(right);
				Vector3 up = Vector3Normalize(Vector3CrossProduct(right, fwd));

				// A short frustum pyramid sized by the camera's FOV (16:9 face).
				const float depth = 1.4f;
				float half_h = std::tan(cam.fov * 0.5f * DEG2RAD) * depth;
				float half_w = half_h * (16.0f / 9.0f);

				Vector3 center = { pos.x + fwd.x * depth, pos.y + fwd.y * depth, pos.z + fwd.z * depth };
				Vector3 corners[4];
				for (int k = 0; k < 4; ++k) {
					float sx = (k == 0 || k == 3) ? -1.0f : 1.0f;
					float sy = (k < 2) ? 1.0f : -1.0f;
					corners[k] = {
						center.x + right.x * half_w * sx + up.x * half_h * sy,
						center.y + right.y * half_w * sx + up.y * half_h * sy,
						center.z + right.z * half_w * sx + up.z * half_h * sy
					};
				}
				for (int k = 0; k < 4; ++k) {
					DrawLine3D(pos, corners[k], c);
					DrawLine3D(corners[k], corners[(k + 1) % 4], c);
				}

				// A small "up" tick on the top edge so orientation reads at a glance.
				Vector3 top_mid = {
					(corners[0].x + corners[1].x) * 0.5f,
					(corners[0].y + corners[1].y) * 0.5f,
					(corners[0].z + corners[1].z) * 0.5f
				};
				DrawLine3D(top_mid, { top_mid.x + up.x * 0.25f, top_mid.y + up.y * 0.25f, top_mid.z + up.z * 0.25f }, c);
			}
		}

		// --- Draw Physics Hitboxes ---
		me::physics::draw_debug(me::get_registry());

		EndMode3D();
		m_ViewportPanel.end_render();

		// 2. Render the UI
		ClearBackground(BLACK);
		rlImGuiBegin();
		ImGuizmo::BeginFrame();

		// ==========================================
		// DEFERRED UI LAYOUT LOADING & SAVING
		// ==========================================
		// Layout files are saved/loaded by name so the same machinery serves the
		// user's custom layout AND the automatic per-mode (Edit/Render) layouts.
		if (!m_PendingLayoutSave.empty()) {
			std::string path = me::vfs::resolve(m_PendingLayoutSave);
			ImGui::SaveIniSettingsToDisk(path.c_str());
			me::logger::info("Saved layout to: " + path);
			m_PendingLayoutSave.clear();
		}

		if (!m_PendingLayoutLoad.empty()) {
			std::string path = me::vfs::resolve(m_PendingLayoutLoad);
			// A mode layout that was never saved simply keeps the current layout.
			if (std::filesystem::exists(path)) {
				ImGui::LoadIniSettingsFromDisk(path.c_str());
				me::logger::info("Loaded layout from: " + path);
			}
			m_PendingLayoutLoad.clear();
		}

		if (!m_PendingLayoutLoadMem.empty()) {
			// In-session stash from the mode we're returning to.
			ImGui::LoadIniSettingsFromMemory(m_PendingLayoutLoadMem.c_str(), m_PendingLayoutLoadMem.size());
			m_PendingLayoutLoadMem.clear();
		}

		ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

		draw_menu_bar();
		draw_toolbar();

		// --- Draw Panels (per-mode visibility; toggled in View > Panels) ---
		PanelSet& panels = active_panels();

		if (panels.hierarchy) m_HierarchyPanel.on_imgui_render(m_CommandHistory);
		me::Entity selected_entity = m_HierarchyPanel.get_selected_entity();

		// Keep orbit target in sync with whatever is selected
		if (selected_entity.is_valid()) {
			auto* t = selected_entity.try_get_component<me::components::TransformComponent>();
			if (t) m_OrbitTarget = { t->position.x, t->position.y, t->position.z };
		}

		if (panels.inspector) m_InspectorPanel.on_imgui_render(selected_entity, m_CommandHistory);
		if (panels.browser) m_BrowserPanel.on_imgui_render();
		if (panels.console) m_ConsolePanel.on_imgui_render();
		if (panels.animation) {
			m_AnimationPanel.on_imgui_render(m_Animation,
				m_EditorCameraTransform, m_EditorCamera, m_Raytracer,
				selected_entity,
				m_SceneState == SceneState::Render,
				m_CommandHistory,
				[this]() { m_ShowAnimRenderModal = true; });
		}

		int active_gizmo = (m_SceneState == SceneState::Edit) ? m_GizmoType : -1;
		bool is_rendering = (m_SceneState == SceneState::Render);
		// The raytracer's output replaces the raster view in Render mode and in
		// RT Play; passing nullptr shows the regular viewport texture.
		bool show_rt_view = is_rendering || (m_SceneState == SceneState::Play && m_RaytracePlayMode);

		m_ViewportPanel.on_imgui_render(
			m_EditorCameraTransform,
			m_EditorCamera,
			selected_entity,
			active_gizmo,
			m_CommandHistory,
			show_rt_view ? m_Raytracer.get_texture() : nullptr,
			is_rendering,                         // Pass the state boolean
			m_SceneState == SceneState::Play,     // For the mode-colored frame
			&m_Raytracer                          // For click-to-focus (DoF)
		);

		// ==========================================
		// RAYTRACER SETTINGS WINDOW
		// ==========================================
		// Keep the raytracer sized to the viewport in Render mode (and RT Play) —
		// independent of whether the settings window itself is visible. Skipped
		// while exporting: the offline render owns the resolution then, and this
		// tracker would stomp it back to viewport size (resetting accumulation
		// and silently exporting at viewport resolution).
		const bool rt_play_active = (m_SceneState == SceneState::Play && m_RaytracePlayMode);
		if ((m_SceneState == SceneState::Render || rt_play_active) && !m_IsExporting) {
			int new_w = (int)(m_ViewportPanel.get_bounds().x * m_Raytracer.resolution_scale);
			int new_h = (int)(m_ViewportPanel.get_bounds().y * m_Raytracer.resolution_scale);
			if (new_w < 1) new_w = 1;
			if (new_h < 1) new_h = 1;
			if (new_w != m_Raytracer.get_width() || new_h != m_Raytracer.get_height()) {
				m_Raytracer.resize(new_w, new_h);
			}
		}

		if ((m_SceneState == SceneState::Render || rt_play_active) && panels.raytracer_settings) {
			ImGui::Begin("Raytracer Settings");

			// --- RT PLAY (real-time) controls — shown while path-tracing play mode ---
			if (rt_play_active) {
				ImGui::TextDisabled("RT Play (real-time)");
				ImGui::SliderInt("Samples / Frame", &m_Raytracer.play_samples_per_frame, 1, 64);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Paths traced per pixel each frame. Noise falls with the square root;\nlower the Resolution Scale to afford more.");
				ImGui::Checkbox("Lock Noise Pattern", &m_Raytracer.lock_noise_pattern);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Reuse the same sample seeds every frame: residual noise becomes a\nstable dither pattern instead of animated static.");
				ImGui::Separator();
				ImGui::Dummy(ImVec2(0, 5));
			}

			// --- BACKEND TOGGLE ---
			const char* backend_names[] = { "CPU PathTracer", "GPU Compute Shader" };
			int current_backend = (int)m_Raytracer.current_backend;
			if (ImGui::Combo("Backend", &current_backend, backend_names, IM_ARRAYSIZE(backend_names))) {
				m_Raytracer.current_backend = (me::systems::RenderBackend)current_backend;
				m_Raytracer.reset_accumulation(&me::get_registry());
			}

			ImGui::Text("Frames Accumulated: %d / %d", m_Raytracer.get_accumulated_frames(), m_Raytracer.preview_samples);

			ImGui::Separator();
			ImGui::Dummy(ImVec2(0, 5));

			if (ImGui::Checkbox("Accumulate Data", &m_Raytracer.accumulate)) {
				if (m_Raytracer.accumulate) m_Raytracer.reset_accumulation(&me::get_registry());
			}

			if (ImGui::SliderInt("Preview Samples", &m_Raytracer.preview_samples, 1, 500)) {
				// If we lower the sample count below what we currently have, restart
				if (m_Raytracer.get_accumulated_frames() > m_Raytracer.preview_samples) {
					m_Raytracer.reset_accumulation();
				}
			}

			// --- RESOLUTION SCALE SLIDER ---
			if (ImGui::SliderFloat("Resolution Scale", &m_Raytracer.resolution_scale, 0.1f, 1.0f, "%.2f")) {
				int new_w = (int)(m_ViewportPanel.get_bounds().x * m_Raytracer.resolution_scale);
				int new_h = (int)(m_ViewportPanel.get_bounds().y * m_Raytracer.resolution_scale);
				if (new_w < 1) new_w = 1;
				if (new_h < 1) new_h = 1;
				m_Raytracer.resize(new_w, new_h);
			}

			if (ImGui::SliderInt("Max Bounces", &m_Raytracer.max_bounces, 1, 16)) {
				m_Raytracer.reset_accumulation();
			}

			// --- RENDER FEATURES (toggles + presets) — applies to CPU and GPU ---
			ImGui::Dummy(ImVec2(0, 6));
			ImGui::Separator();
			ImGui::TextDisabled("Render Features");

			// Presets are shortcuts for the four toggles below. Noise comes from
			// random sampling, so the lighter presets are both sharper and faster.
			using Preset = me::systems::RenderQualityPreset;
			if (ImGui::SmallButton("Full")) { m_Raytracer.apply_quality_preset(Preset::Full); m_Raytracer.reset_accumulation(); }
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Everything on: GI, soft shadows, reflections, glass.\nReference quality - needs accumulation to converge.");
			ImGui::SameLine();
			if (ImGui::SmallButton("Lite")) { m_Raytracer.apply_quality_preset(Preset::Lite); m_Raytracer.reset_accumulation(); }
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("No GI, hard shadows; keeps mirrors + sharp glass.\nNoise-free at 1 sample, still looks ray-traced.");
			ImGui::SameLine();
			if (ImGui::SmallButton("Flat")) { m_Raytracer.apply_quality_preset(Preset::Flat); m_Raytracer.reset_accumulation(); }
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Direct light + hard shadows only. Fastest, flattest, zero noise.\nBest for a simple game that just wants crisp lit shapes.");

			bool feat_changed = false;
			feat_changed |= ImGui::Checkbox("Indirect Lighting (GI)", &m_Raytracer.enable_indirect);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Bounced/global illumination. The biggest source of both realism\nAND noise - turning it off gives a sharp image at 1 sample.");
			feat_changed |= ImGui::Checkbox("Soft Shadows", &m_Raytracer.enable_soft_shadows);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Penumbras from light size. Off = hard, crisp, noise-free shadows.");
			feat_changed |= ImGui::Checkbox("Reflections", &m_Raytracer.enable_reflections);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Specular bounces on opaque surfaces (mirrors).\nMirror reflections are deterministic - sharp, no noise.");
			feat_changed |= ImGui::Checkbox("Refraction (Glass)", &m_Raytracer.enable_refraction);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("See-through glass. Off = glass renders as a solid matte object.");
			if (feat_changed) m_Raytracer.reset_accumulation();

			// A live hint about whether the current combo converges instantly.
			bool noise_free = !m_Raytracer.enable_indirect && !m_Raytracer.enable_soft_shadows && m_Raytracer.aperture <= 0.0f;
			if (noise_free) ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.5f, 1.0f), "Noise-free (sharp at 1 sample)");
			else ImGui::TextDisabled("Needs accumulation to denoise");

			// --- ENVIRONMENT (sky + ambient) — applies to CPU and GPU backends ---
			ImGui::Dummy(ImVec2(0, 6));
			ImGui::Separator();
			ImGui::TextDisabled("Environment");

			// Ambient fill: the occlusion-free brightness floor. 0 = true black
			// where no direct/indirect light reaches.
			if (ImGui::SliderFloat("Ambient Fill", &m_Raytracer.ambient_strength, 0.0f, 1.0f, "%.3f")) {
				m_Raytracer.reset_accumulation();
			}
			// Sky gradient: also the background for rays that escape the scene.
			if (ImGui::ColorEdit3("Sky Horizon", &m_Raytracer.sky_horizon_color.x)) {
				m_Raytracer.reset_accumulation();
			}
			if (ImGui::ColorEdit3("Sky Zenith", &m_Raytracer.sky_zenith_color.x)) {
				m_Raytracer.reset_accumulation();
			}
			// Master sky brightness. 0 = pure black background.
			if (ImGui::SliderFloat("Sky Intensity", &m_Raytracer.sky_intensity, 0.0f, 2.0f, "%.2f")) {
				m_Raytracer.reset_accumulation();
			}
			// One-click escape hatch from a black-screen / over-tweaked state.
			if (ImGui::SmallButton("Reset Environment")) {
				m_Raytracer.reset_environment();
				m_Raytracer.reset_accumulation();
			}

			// --- CAMERA / LENS ---
			ImGui::Dummy(ImVec2(0, 6));
			ImGui::Separator();
			ImGui::TextDisabled("Camera");

			// Tonemap exposure (overall brightness before the ACES curve).
			if (ImGui::SliderFloat("Exposure", &m_Raytracer.exposure, 0.0f, 3.0f, "%.2f")) {
				m_Raytracer.reset_accumulation();
			}
			// Depth of field: 0 aperture = pinhole (all sharp); larger = more blur.
			if (ImGui::SliderFloat("Aperture (DoF)", &m_Raytracer.aperture, 0.0f, 0.5f, "%.3f")) {
				m_Raytracer.reset_accumulation();
			}
			if (ImGui::SliderFloat("Focus Distance", &m_Raytracer.focus_distance, 0.1f, 100.0f, "%.2f")) {
				m_Raytracer.reset_accumulation();
			}
			// Firefly clamp: lower to suppress bright noise specks; raise above your
			// brightest emitter so it doesn't dim the lights. 0 turns it off.
			if (ImGui::SliderFloat("Firefly Clamp", &m_Raytracer.firefly_clamp, 0.0f, 50.0f, "%.1f")) {
				m_Raytracer.reset_accumulation();
			}

			ImGui::Dummy(ImVec2(0, 10));
			ImGui::Separator();
			ImGui::Dummy(ImVec2(0, 10));

			// Export Image Button
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
			if (ImGui::Button("Export to PNG", ImVec2(-1, 30))) {
				m_ShowExportModal = true;
			}
			ImGui::PopStyleColor();

			ImGui::End();
		}

		draw_overlays();
		draw_modals();

		rlImGuiEnd();
	}

	void EditorApp::poll_shortcuts() {
		bool ctrl = ImGui::GetIO().KeyCtrl;
		bool shift = ImGui::GetIO().KeyShift;
		bool wantText = ImGui::GetIO().WantTextInput;
		me::entity::entity_id selected = m_HierarchyPanel.get_selected_entity();

		// Play / Stop toggle (Ctrl + P) — the ONLY editor shortcut allowed during
		// Play. Everything below either edits the scene or competes for keys the
		// running game wants (WASD overlaps the W/S gizmo switches, Delete, etc.),
		// so while playing the game owns the keyboard and we bail right after
		// handling stop. A shipped/exported game has no editor shortcuts at all.
		if (!wantText && !m_IsFlying && ctrl && ImGui::IsKeyPressed(ImGuiKey_P)) {
			if (m_SceneState == SceneState::Edit) {
				on_play();
				me::set_paused(false);
			} else {
				on_stop();
			}
			return;
		}
		if (m_SceneState == SceneState::Play) return;

		// Global shortcuts — always fire (unless typing in a text box or flying,
		// where Ctrl/Shift are the camera speed modifiers and S flies backward).
		if (!wantText && !m_IsFlying) {

			// Save Scene (Ctrl + S) / Save Scene As (Ctrl + Shift + S)
			if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_S)) {
				open_save_as_modal();
			} else if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
				save_scene();
				me::logger::info("Saved scene in " + m_CurrentScenePath);
			}

			// New Scene (Ctrl + N)
			if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N)) {
				request_new_scene();
			}

			// Undo (Ctrl + Z) / Redo (Ctrl + Shift + Z or Ctrl + Y)
			if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
				if (shift) m_CommandHistory.Redo();
				else       m_CommandHistory.Undo();
			}
			if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
				m_CommandHistory.Redo();
			}
		}

		// Entity shortcuts — only when not flying and not typing
		if (!m_IsFlying && !wantText) {

			// 4. Escape to deselect
			if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
				m_HierarchyPanel.set_selected_entity(me::entity::null);
			}

			// 5. Delete selected entity (undoable)
			if (selected != me::entity::null && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
				delete_selected_entity();
			}

			// 6. Duplicate entity (Ctrl + D, undoable)
			if (selected != me::entity::null && ctrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
				duplicate_selected_entity();
			}

			// 7. Rename selected entity (F2)
			if (selected != me::entity::null && ImGui::IsKeyPressed(ImGuiKey_F2)) {
				m_HierarchyPanel.begin_rename(selected);
			}

			if (selected != me::entity::null && ImGui::IsKeyPressed(ImGuiKey_F)) {
				auto* transform = me::get_registry().try_get_component<me::components::TransformComponent>(selected);
				if (transform) {
					Vector3 wp = transform->world_position();
					m_OrbitTarget = { wp.x, wp.y, wp.z };
					m_EditorCamera.target = { wp.x, wp.y, wp.z };
					float distance = 10.0f;
					m_EditorCameraTransform.position.x = wp.x - std::sin(m_EditorCameraTransform.rotation.y * (PI / 180.0f)) * distance;
					m_EditorCameraTransform.position.y = wp.y + 5.0f;
					m_EditorCameraTransform.position.z = wp.z - std::cos(m_EditorCameraTransform.rotation.y * (PI / 180.0f)) * distance;
					m_EditorCameraTransform.rotation.x = -25.0f;
				}
			}
		}

		// Viewport-focused shortcuts
		//if (m_ViewportPanel.is_focused() && !m_IsFlying && !wantText) {
		// (!ctrl keeps Ctrl+S / Ctrl+Shift+S from also switching the gizmo)
		if (!m_IsFlying && !wantText && !ctrl) {

			// 8. Gizmo tool switching (Q / W / R / S)
			if (ImGui::IsKeyPressed(ImGuiKey_Q)) m_GizmoType = -1;
			if (ImGui::IsKeyPressed(ImGuiKey_W)) m_GizmoType = ImGuizmo::TRANSLATE;
			if (ImGui::IsKeyPressed(ImGuiKey_R)) m_GizmoType = ImGuizmo::ROTATE;
			if (ImGui::IsKeyPressed(ImGuiKey_S)) m_GizmoType = ImGuizmo::SCALE;
		}
	}

	// Both helpers route through the command history, so the operations are
	// undoable and shared by the shortcuts and the Edit menu.
	void EditorApp::delete_selected_entity() {
		auto selected = m_HierarchyPanel.get_selected_entity().get_id();
		if (selected == me::entity::null) return;
		m_CommandHistory.AddCommand(std::make_unique<editor::DeleteEntityCommand>(me::get_registry(), selected));
		m_HierarchyPanel.set_selected_entity(me::entity::null);
	}

	void EditorApp::duplicate_selected_entity() {
		auto selected = m_HierarchyPanel.get_selected_entity().get_id();
		if (selected == me::entity::null) return;
		auto cmd = std::make_unique<editor::DuplicateEntityCommand>(me::get_registry(), selected);
		auto* raw = cmd.get();
		m_CommandHistory.AddCommand(std::move(cmd));
		if (raw->clone_id() != me::entity::null)
			m_HierarchyPanel.set_selected_entity(raw->clone_id());
	}

	// ====================================================================
	// PROJECT & SCENE LOGIC
	// ====================================================================

	void EditorApp::create_project(const std::filesystem::path& path) {
		me::vfs::mount("game", (path / "assets").string());
		me::fs::create_directory("game://scenes");
		me::fs::create_directory("game://scripts");
		me::fs::create_directory("game://models");
		me::fs::create_directory("game://textures");
		me::fs::create_directory("game://audio");

		// Seed the new project with the shipped demo (scene + model + script +
		// sound + a sample animation), so the first thing a user sees shows both
		// halves of the engine: press Play and SPACE makes the cube jump; press
		// Render and the same scene path-traces with glass, a mirror and glow.
		std::error_code ec;
		std::filesystem::path demo_dir = std::filesystem::path(GetApplicationDirectory()) / "assets" / "demo";
		if (std::filesystem::exists(demo_dir)) {
			std::filesystem::copy(demo_dir, path / "assets",
				std::filesystem::copy_options::recursive | std::filesystem::copy_options::skip_existing, ec);
			if (ec) me::logger::warn("Could not seed the demo project: " + ec.message());
		}

		load_project(path);
	}

	void EditorApp::load_project(const std::filesystem::path& path) {
		// Guard every entry point (hub, recents, future CLI): loading a project
		// folder that doesn't exist would crash the content browser's iterator.
		if (!std::filesystem::exists(path)) {
			me::logger::error("Project folder not found: " + path.generic_string());
			return;
		}

		m_HubPanel.unload_background(); // leaving the hub — free its texture
		add_recent_project(path.generic_string());

		m_ProjectPath = path;
		m_IsProjectLoaded = true;
		me::vfs::mount("game", (m_ProjectPath / "assets").string());

		m_HierarchyPanel.set_context(&me::get_registry());
		m_BrowserPanel.set_project_path(path);

		// Reopen the scene last edited in this project (see resolve_startup_scene).
		// If the project has no scenes at all, start an untitled one — Ctrl+S then
		// prompts for a name. Nothing here creates or assumes a "main.json".
		std::string startup_scene = resolve_startup_scene(path);
		if (!startup_scene.empty()) {
			m_CurrentScenePath = startup_scene;
			me::scene_manager::load(startup_scene); // SceneLoadedEvent does the bookkeeping
		} else {
			new_scene();
			m_CurrentScenePath.clear(); // untitled until the first Save As
		}

		m_CommandHistory.Clear();
		m_SceneDirty = false;
	}

	std::string EditorApp::resolve_startup_scene(const std::filesystem::path& path) {
		// 1. The scene last edited in this project, if it still exists on disk.
		if (auto it = m_LastScenes.find(path.generic_string()); it != m_LastScenes.end()) {
			if (me::fs::exists(it->second)) return it->second;
		}

		// 2. Otherwise the first real scene in the project's scenes folder
		//    (alphabetical; skip the play scratch file and ".anim.json" sidecars).
		std::filesystem::path scenes_dir = path / "assets" / "scenes";
		std::vector<std::string> names;
		if (std::filesystem::exists(scenes_dir)) {
			for (const auto& entry : std::filesystem::directory_iterator(scenes_dir)) {
				if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
				std::string fname = entry.path().filename().string();
				if (fname == ".temp_play.json") continue;
				if (fname.size() >= 10 && fname.compare(fname.size() - 10, 10, ".anim.json") == 0) continue;
				names.push_back(fname);
			}
		}
		std::sort(names.begin(), names.end());
		if (!names.empty()) return "game://scenes/" + names.front();

		// 3. No scenes — the caller starts a fresh untitled scene.
		return "";
	}

	void EditorApp::remember_last_scene() {
		if (m_ProjectPath.empty() || m_CurrentScenePath.empty()) return;
		if (m_CurrentScenePath.find(".temp_play.json") != std::string::npos) return;
		m_LastScenes[m_ProjectPath.generic_string()] = m_CurrentScenePath;
		save_engine_config();
	}

	void EditorApp::new_scene() {
		me::scene_manager::clear();
		m_HierarchyPanel.set_selected_entity(me::entity::null);
		// The history references entities from the old scene — they're gone now.
		m_CommandHistory.Clear();
		m_SceneDirty = true; // unsaved until the first Ctrl+S

		// Create a Directional Light Source (The Sun)
		auto sun = me::get_registry().create_entity();
		sun.add_component(me::components::TagComponent{ "Directional Light" });
		sun.add_component(me::components::TransformComponent{
			{0.0f, 20.0f, 0.0f},   // Position
			{-45.0f, 45.0f, 0.0f}, // Rotation
			{1.0f, 1.0f, 1.0f}     // Scale
			});
		sun.add_component(me::components::DirectionalLightComponent{ me::Color::white, 1.0f });

		// Create a simple Cube
		auto cube = me::get_registry().create_entity();
		cube.add_component(me::components::TagComponent{ "Cube" });
		cube.add_component(me::components::TransformComponent{ {0.0f, 0.05f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f} });
		cube.add_component(me::components::Shape3DComponent{ me::components::Shape3DComponent::Cube, me::Color::white });

		// Grid of colorfull balls
		//for (int x = 0; x < 10; ++x) {
		//	for (int y = 0; y < 10; ++y) {
		//		for (int z = 0; z < 10; ++z) {
		//			auto e = me::get_registry().create_entity();

		//			me::components::TransformComponent t;
		//			t.position = { x * 2.5f - 10.0f, y * 2.5f, z * 2.5f - 10.0f };
		//			t.scale = { 1.0f, 1.0f, 1.0f };
		//			t.is_dirty = true;
		//			me::get_registry().add_component(e, t);

		//			me::components::Shape3DComponent s;
		//			s.type = me::components::Shape3DComponent::Sphere;
		//			s.color = me::Color{ (unsigned char)(x * 25), (unsigned char)(y * 25), (unsigned char)(z * 25), 255 };
		//			me::get_registry().add_component(e, s);
		//		}
		//	}
		//}
	}

	bool EditorApp::save_scene() {
		// An untitled scene (e.g. a fresh project with no scenes) has no path yet —
		// route Save to Save As so the user names the file instead of failing.
		if (m_CurrentScenePath.empty()) { open_save_as_modal(); return false; }
		me::scene_manager::save(m_CurrentScenePath);
		save_animation_sidecar();
		m_SceneDirty = false;
		return true;
	}


	void EditorApp::open_save_as_modal() {
		m_ShowSaveAsModal = true;
		// Seed the name field with the current scene's filename (minus extension).
		std::string name = m_CurrentScenePath;
		if (auto pos = name.find_last_of('/'); pos != std::string::npos) name = name.substr(pos + 1);
		if (auto pos = name.rfind(".json"); pos != std::string::npos) name = name.substr(0, pos);
		if (name.empty()) name = "my_new_scene";
		snprintf(m_NewSceneInput, sizeof(m_NewSceneInput), "%s", name.c_str());
	}

	// Edit and Render each keep their own dock layout; crossing between them
	// stashes the layout being left IN MEMORY (the session keeps your
	// arrangement, but disk only changes via Save Layout) and brings in the
	// other mode's layout: session stash first, then the user's saved file,
	// then the default shipped in assets/layouts/.
	void EditorApp::switch_mode_layout(SceneState from, SceneState to) {
		if (!m_AutoModeLayouts) return;
		bool from_render = (from == SceneState::Render);
		bool to_render = (to == SceneState::Render);
		if (from_render == to_render) return; // same layout family — nothing to swap

		size_t len = 0;
		const char* ini = ImGui::SaveIniSettingsToMemory(&len);
		(from_render ? m_LayoutMemRender : m_LayoutMemEdit).assign(ini, len);

		std::string& stash = to_render ? m_LayoutMemRender : m_LayoutMemEdit;
		if (!stash.empty()) {
			m_PendingLayoutLoadMem = stash;
			return;
		}

		const char* user_file = to_render ? "root://layout_render.ini" : "root://layout_edit.ini";
		const char* shipped = to_render ? "root://assets/layouts/layout_render.ini" : "root://assets/layouts/layout_edit.ini";
		m_PendingLayoutLoad = std::filesystem::exists(me::vfs::resolve(user_file)) ? user_file : shipped;
	}

	void EditorApp::save_current_layout() {
		m_PendingLayoutSave = (m_SceneState == SceneState::Render)
			? "root://layout_render.ini" : "root://layout_edit.ini";
	}

	void EditorApp::reset_layout_to_default() {
		bool render = (m_SceneState == SceneState::Render);
		(render ? m_LayoutMemRender : m_LayoutMemEdit).clear(); // drop the session stash
		m_PendingLayoutLoad = render
			? "root://assets/layouts/layout_render.ini" : "root://assets/layouts/layout_edit.ini";
	}

	// Exports the current arrangement as the app's shipped default (used by
	// fresh installs that have no saved layout yet). Written into the runtime
	// assets folder; copy assets/layouts/ back into editor/assets/ in the repo
	// to make it part of the source tree.
	void EditorApp::save_layout_as_default() {
		std::filesystem::path dir = me::vfs::resolve("root://assets/layouts");
		std::error_code ec;
		std::filesystem::create_directories(dir, ec);

		bool render = (m_SceneState == SceneState::Render);
		std::filesystem::path file = dir / (render ? "layout_render.ini" : "layout_edit.ini");
		ImGui::SaveIniSettingsToDisk(file.string().c_str());

		// Ship the panel-visibility defaults along with the dock layout.
		nlohmann::json pj;
		auto write_panels = [](const PanelSet& p) {
			return nlohmann::json{
				{"hierarchy", p.hierarchy}, {"inspector", p.inspector},
				{"browser", p.browser}, {"console", p.console},
				{"animation", p.animation}, {"raytracer_settings", p.raytracer_settings}
			};
			};
		pj["panels_edit"] = write_panels(m_PanelsEdit);
		pj["panels_render"] = write_panels(m_PanelsRender);
		std::ofstream ofs(dir / "default_panels.json");
		if (ofs) ofs << pj.dump(4);

		me::logger::info("Saved shipped default (" + std::string(render ? "Render" : "Edit") + ") to: " + file.string());
		me::logger::info("To ship it with the app, copy assets/layouts/ into editor/assets/ in the repo.");
	}

	void EditorApp::apply_frame_pacing() {
		if (m_SceneState == SceneState::Render) {
			me::set_target_fps(0); // uncapped — accumulate samples as fast as possible
		} else {
			int refresh = GetMonitorRefreshRate(GetCurrentMonitor());
			me::set_target_fps(refresh > 0 ? refresh : 60);
		}
	}

	void EditorApp::open_scene(const std::string& vfs_path) {
		// Leave play/render mode first — swapping the scene mid-simulation would
		// mix the old simulation state into the new scene.
		if (m_SceneState == SceneState::Play) {
			on_stop();
		} else if (m_SceneState == SceneState::Render) {
			m_Raytracer.on_stop();
			m_SceneState = SceneState::Edit;
			apply_frame_pacing();
			switch_mode_layout(SceneState::Render, SceneState::Edit);
		}
		me::scene_manager::load(vfs_path); // SceneLoadedEvent does the bookkeeping
		me::logger::info("Opened scene: " + vfs_path);
	}

	void EditorApp::request_exit() {
		if (m_SceneDirty) {
			m_PendingAction = PendingAction::Exit;
			m_ShowUnsavedModal = true;
		} else {
			me::close_application();
		}
	}

	void EditorApp::request_new_scene() {
		if (m_SceneDirty) {
			m_PendingAction = PendingAction::NewScene;
			m_ShowUnsavedModal = true;
		} else {
			m_ShowNewSceneModal = true;
			snprintf(m_NewSceneInput, sizeof(m_NewSceneInput), "%s", "my_new_scene");
		}
	}

	void EditorApp::request_open_scene(const std::string& vfs_path) {
		if (m_SceneDirty) {
			m_PendingAction = PendingAction::OpenScene;
			m_PendingScenePath = vfs_path;
			m_ShowUnsavedModal = true;
		} else {
			open_scene(vfs_path);
		}
	}

	void EditorApp::perform_pending_action() {
		PendingAction action = m_PendingAction;
		m_PendingAction = PendingAction::None;

		switch (action) {
		case PendingAction::Exit:
			me::close_application();
			break;
		case PendingAction::NewScene:
			m_ShowNewSceneModal = true;
			snprintf(m_NewSceneInput, sizeof(m_NewSceneInput), "%s", "my_new_scene");
			break;
		case PendingAction::OpenScene:
			open_scene(m_PendingScenePath);
			break;
		default:
			break;
		}
	}

	// Shows "<scene>[*]" in the OS window title; the star marks unsaved changes.
	void EditorApp::update_window_title() {
		std::string title = "mini-engine-raylib";
		if (m_IsProjectLoaded && !m_CurrentScenePath.empty()) {
			std::string scene = m_CurrentScenePath;
			if (auto pos = scene.find_last_of('/'); pos != std::string::npos) scene = scene.substr(pos + 1);
			title += " - " + scene;
			if (m_SceneDirty) title += " *";
			if (m_SceneState == SceneState::Play)        title += "  [PLAY]";
			else if (m_SceneState == SceneState::Render) title += "  [RENDER]";
		}
		if (title != m_LastWindowTitle) {
			SetWindowTitle(title.c_str());
			m_LastWindowTitle = title;
		}
	}

	// Mirrors on_render's play-camera pick: an active CameraComponent that also
	// has a Transform can drive the play view. When none exists, on_render falls
	// back to the editor fly-cam, so we keep that camera drivable during Play.
	bool EditorApp::has_active_scene_camera() {
		auto& reg = me::get_registry();
		for (auto [e, cam] : reg.view<me::components::CameraComponent>()) {
			if (cam.active && reg.try_get_component<me::components::TransformComponent>(e))
				return true;
		}
		return false;
	}

	void EditorApp::on_play() {
		m_SceneState = SceneState::Play;
		me::logger::info("Mode set to: PLAYING");
		me::scene_manager::save("game://scenes/.temp_play.json");

		// Start the physics simulation and load the bodies
		me::physics::on_play(me::get_registry());

		// RT Play: bring the path tracer up at the viewport resolution (scaled),
		// exactly like entering Render mode does.
		if (m_RaytracePlayMode) {
			float raw_w = m_ViewportPanel.get_bounds().x;
			float raw_h = m_ViewportPanel.get_bounds().y;
			if (raw_w <= 0 || raw_h <= 0) { raw_w = 1280.0f; raw_h = 720.0f; }
			int w = std::max(1, (int)(raw_w * m_Raytracer.resolution_scale));
			int h = std::max(1, (int)(raw_h * m_Raytracer.resolution_scale));
			m_Raytracer.on_start(w, h);
		}

		// Play starts UNfocused: the game receives no input and the cursor stays
		// free, so you can tweak panels first. Click the viewport to engage.
		m_PlaytestFocused = false;
		m_IsFlying = false; // never carry an editor fly-grab into play
		me::input::set_input_gate(false);

		me::set_playing(true);
		me::get_event_bus().publish<me::events::PlayStateChangedEvent>(true);
	}

	void EditorApp::on_stop() {
		SceneState prev = m_SceneState;
		m_SceneState = SceneState::Edit;
		apply_frame_pacing();
		switch_mode_layout(prev, SceneState::Edit); // no-op when stopping Play
		me::logger::info("Mode set to: EDIT");
		me::set_playing(false);
		me::get_event_bus().publish<me::events::PlayStateChangedEvent>(false);

		// Returning to the editor: re-open the input gate (edit-mode camera fly
		// needs it) and release the cursor a game script may have locked.
		m_PlaytestFocused = false;
		me::input::set_input_gate(true);
		me::input::unlock_cursor();

		// Destroy all live physics bodies
		me::physics::on_stop();

		// RT Play owned the raytracer for the session — free the VRAM/buffers.
		if (m_RaytracePlayMode) m_Raytracer.on_stop();

		// ==========================================
		// SILENCE ALL ACTIVE AUDIO
		// ==========================================
		auto& registry = me::get_registry();

		// Stop any looping music streams
		for (auto [e, music] : registry.view<me::components::BackgroundMusicComponent>()) {
			(void)e;
			me::audio::stop_music(music.stream);
		}

		// Stop any long-playing sound effects
		for (auto [e, sfx] : registry.view<me::components::AudioSourceComponent>()) {
			(void)e;
			me::audio::stop(sfx.clip);
		}

		me::scene_manager::load("game://scenes/.temp_play.json");
		me::fs::remove("game://scenes/.temp_play.json");

		// Reloading the scene re-creates every entity under new ids, so the
		// pre-play history would act on dead entities — drop it.
		m_CommandHistory.Clear();
	}

	// Packaging, Godot-style: the runtime executable was compiled when the
	// engine was built; exporting only copies files. Output layout:
	//   <out>/<Name>.exe        the renamed game runtime
	//   <out>/game_config.json  window settings + boot scene
	//   <out>/assets/shaders/   engine runtime shaders   (game mounts engine://)
	//   <out>/data/             this project's content   (game mounts game://)
	void EditorApp::export_game() {
		namespace fs = std::filesystem;
		std::error_code ec;

		fs::path exe_dir = GetApplicationDirectory();
#ifdef _WIN32
		fs::path runtime = exe_dir / "game.exe";
#else
		fs::path runtime = exe_dir / "game";
#endif
		if (!fs::exists(runtime)) {
			me::logger::error("Game runtime not found next to the editor (" + runtime.string() + ").");
			me::logger::error("Packaged install: restore the missing file. Source build: build the 'game' target.");
			return;
		}

		// A raytraced export needs a runtime that knows the renderer exists. The
		// runtime embeds a capability marker string; a game.exe from an older
		// build lacks it and would silently render raster — refuse instead.
		if (m_ExportGameRaytraced) {
			std::ifstream rt_check(runtime, std::ios::binary);
			std::string runtime_bytes((std::istreambuf_iterator<char>(rt_check)), std::istreambuf_iterator<char>());
			if (runtime_bytes.find("me-runtime-cap:raytraced;") == std::string::npos) {
				me::logger::error("Export aborted: the game runtime next to the editor is from an older build "
					"without the raytraced renderer - the exported game would silently fall back to raster.");
				me::logger::error("Rebuild the 'game' target with the same build as the editor, then export again. (" + runtime.string() + ")");
				return;
			}
		}

		std::string name = m_ExportGameName[0] ? std::string(m_ExportGameName) : "game";
		fs::path out = m_ExportGameDir;
		fs::create_directories(out, ec);
		if (!fs::exists(out)) {
			me::logger::error("Could not create the export folder: " + out.string());
			return;
		}

		// 1. The runtime, renamed to the game.
#ifdef _WIN32
		fs::path game_exe = out / (name + ".exe");
#else
		fs::path game_exe = out / name;
#endif
		fs::copy_file(runtime, game_exe, fs::copy_options::overwrite_existing, ec);
		if (ec) {
			me::logger::error("Export failed copying the runtime: " + ec.message());
			return;
		}

		// 2. Engine runtime shaders.
		fs::create_directories(out / "assets" / "shaders", ec);
		fs::copy(exe_dir / "assets" / "shaders", out / "assets" / "shaders",
			fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
		if (ec) me::logger::warn("Export: copying engine shaders reported: " + ec.message());

		// 3. The project's content. Start clean so deleted assets don't linger.
		fs::remove_all(out / "data", ec);
		ec.clear();
		fs::copy(m_ProjectPath / "assets", out / "data",
			fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
		if (ec) {
			me::logger::error("Export failed copying project assets: " + ec.message());
			return;
		}
		fs::remove(out / "data" / "scenes" / ".temp_play.json", ec); // editor-only scratch

		// 4. The game's boot config.
		nlohmann::json j;
		j["name"] = name;
		j["main_scene"] = m_ExportGameScene.empty() ? m_CurrentScenePath : m_ExportGameScene;
		j["width"] = std::max(320, m_ExportGameW);
		j["height"] = std::max(240, m_ExportGameH);
		j["vsync"] = m_ExportGameVsync;
		j["renderer"] = m_ExportGameRaytraced ? "raytraced" : "raster";
		if (m_ExportGameRaytraced) {
			// Snapshot of the Raytracer Settings the game will boot with.
			nlohmann::json rt;
			rt["resolution_scale"] = m_Raytracer.resolution_scale;
			rt["samples_per_frame"] = m_Raytracer.play_samples_per_frame;
			rt["bounces"] = m_Raytracer.max_bounces;
			rt["lock_noise_pattern"] = m_Raytracer.lock_noise_pattern;
			rt["exposure"] = m_Raytracer.exposure;
			rt["firefly_clamp"] = m_Raytracer.firefly_clamp;
			rt["ambient_strength"] = m_Raytracer.ambient_strength;
			rt["sky_horizon"] = { m_Raytracer.sky_horizon_color.x, m_Raytracer.sky_horizon_color.y, m_Raytracer.sky_horizon_color.z };
			rt["sky_zenith"] = { m_Raytracer.sky_zenith_color.x, m_Raytracer.sky_zenith_color.y, m_Raytracer.sky_zenith_color.z };
			rt["sky_intensity"] = m_Raytracer.sky_intensity;
			// Feature toggles — the game ships with exactly the look you set here.
			rt["soft_shadows"] = m_Raytracer.enable_soft_shadows;
			rt["reflections"] = m_Raytracer.enable_reflections;
			rt["refraction"] = m_Raytracer.enable_refraction;
			rt["indirect"] = m_Raytracer.enable_indirect;
			j["rt"] = rt;
		}
		std::ofstream ofs(out / "game_config.json");
		if (ofs) ofs << j.dump(4);

		me::logger::info("Game exported to: " + out.string());
		editor::utils::open_folder_dialog(out.string());
	}

	void EditorApp::load_engine_config() {
		// First run (no per-user config yet): adopt the shipped panel-visibility
		// defaults so a fresh install matches the author's setup.
		if (!std::filesystem::exists("engine_config.json")) {
			std::ifstream dfs("assets/layouts/default_panels.json");
			if (dfs.is_open()) {
				try {
					nlohmann::json dj;
					dfs >> dj;
					auto read_defaults = [](PanelSet& p, const nlohmann::json& pj) {
						p.hierarchy = pj.value("hierarchy", p.hierarchy);
						p.inspector = pj.value("inspector", p.inspector);
						p.browser = pj.value("browser", p.browser);
						p.console = pj.value("console", p.console);
						p.animation = pj.value("animation", p.animation);
						p.raytracer_settings = pj.value("raytracer_settings", p.raytracer_settings);
						};
					if (dj.contains("panels_edit"))   read_defaults(m_PanelsEdit, dj["panels_edit"]);
					if (dj.contains("panels_render")) read_defaults(m_PanelsRender, dj["panels_render"]);
				} catch (...) {}
			}
		}

		std::ifstream ifs("engine_config.json");
		if (ifs.is_open()) {
			nlohmann::json j;
			try {
				ifs >> j;
				if (j.contains("recent_projects")) {
					for (const auto& path : j["recent_projects"]) m_RecentProjects.push_back(path.get<std::string>());
					m_HubPanel.set_recent_projects(m_RecentProjects); // Pass to hub!
				}
				if (j.contains("last_scenes")) {
					for (auto& [project, scene] : j["last_scenes"].items())
						m_LastScenes[project] = scene.get<std::string>();
				}
				if (j.contains("ui_scale")) ImGui::GetIO().FontGlobalScale = j["ui_scale"].get<float>();
				if (j.contains("auto_mode_layouts")) m_AutoModeLayouts = j["auto_mode_layouts"].get<bool>();

				auto read_panels = [](PanelSet& p, const nlohmann::json& pj) {
					p.hierarchy = pj.value("hierarchy", p.hierarchy);
					p.inspector = pj.value("inspector", p.inspector);
					p.browser = pj.value("browser", p.browser);
					p.console = pj.value("console", p.console);
					p.animation = pj.value("animation", p.animation);
					p.raytracer_settings = pj.value("raytracer_settings", p.raytracer_settings);
					};
				if (j.contains("panels_edit"))   read_panels(m_PanelsEdit, j["panels_edit"]);
				if (j.contains("panels_render")) read_panels(m_PanelsRender, j["panels_render"]);
			} catch (...) {}
		}
	}

	void EditorApp::save_engine_config() {
		nlohmann::json j;
		j["recent_projects"] = m_RecentProjects;
		j["last_scenes"] = m_LastScenes;
		j["ui_scale"] = ImGui::GetIO().FontGlobalScale;
		j["auto_mode_layouts"] = m_AutoModeLayouts;

		auto write_panels = [](const PanelSet& p) {
			return nlohmann::json{
				{"hierarchy", p.hierarchy}, {"inspector", p.inspector},
				{"browser", p.browser}, {"console", p.console},
				{"animation", p.animation}, {"raytracer_settings", p.raytracer_settings}
			};
			};
		j["panels_edit"] = write_panels(m_PanelsEdit);
		j["panels_render"] = write_panels(m_PanelsRender);
		std::ofstream ofs("engine_config.json");
		ofs << j.dump(4);
	}

	void EditorApp::add_recent_project(const std::string& path) {
		auto it = std::remove(m_RecentProjects.begin(), m_RecentProjects.end(), path);
		m_RecentProjects.erase(it, m_RecentProjects.end());
		m_RecentProjects.insert(m_RecentProjects.begin(), path);
		if (m_RecentProjects.size() > 10) m_RecentProjects.pop_back();
		m_HubPanel.set_recent_projects(m_RecentProjects); // Update hub!
		save_engine_config();
	}

	// ====================================================================
	// UI DRAW HELPERS
	// ====================================================================

	void EditorApp::draw_menu_bar() {
		if (ImGui::BeginMainMenuBar()) {

			// --- File Menu ---
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("New Scene", "Ctrl+N")) request_new_scene();

				// Open Scene: every .json in the project's scenes folder.
				if (ImGui::BeginMenu("Open Scene")) {
					bool any = false;
					std::filesystem::path scenes_dir = m_ProjectPath / "assets" / "scenes";
					if (std::filesystem::exists(scenes_dir)) {
						for (const auto& entry : std::filesystem::directory_iterator(scenes_dir)) {
							if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
							std::string fname = entry.path().filename().string();
							if (fname == ".temp_play.json") continue;
							any = true;
							std::string vfs_path = "game://scenes/" + fname;
							bool is_current = (vfs_path == m_CurrentScenePath);
							if (ImGui::MenuItem(fname.c_str(), nullptr, is_current)) {
								request_open_scene(vfs_path);
							}
						}
					}
					if (!any) ImGui::TextDisabled("(no scenes found)");
					ImGui::EndMenu();
				}

				if (ImGui::MenuItem("Save Scene", "Ctrl+S")) save_scene();
				if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) open_save_as_modal();

				ImGui::Separator();
				if (ImGui::MenuItem("Export Game...")) {
					m_ShowExportGameModal = true;
					snprintf(m_ExportGameName, sizeof(m_ExportGameName), "%s", m_ProjectPath.filename().string().c_str());
					snprintf(m_ExportGameDir, sizeof(m_ExportGameDir), "%s", (m_ProjectPath / "export").string().c_str());
					// Default the game's boot scene to the one currently open; the
					// export modal lets the user pick a different one.
					m_ExportGameScene = m_CurrentScenePath;
				}

				ImGui::Separator();
				if (ImGui::MenuItem("Exit")) request_exit();
				ImGui::EndMenu();
			}

			// --- Edit Menu ---
			if (ImGui::BeginMenu("Edit")) {
				if (ImGui::MenuItem("Undo", "Ctrl+Z", false, m_CommandHistory.CanUndo())) m_CommandHistory.Undo();
				if (ImGui::MenuItem("Redo", "Ctrl+Y", false, m_CommandHistory.CanRedo())) m_CommandHistory.Redo();
				ImGui::Separator();
				bool has_selection = m_HierarchyPanel.get_selected_entity().is_valid();
				if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, has_selection)) duplicate_selected_entity();
				if (ImGui::MenuItem("Delete", "Del", false, has_selection)) delete_selected_entity();
				ImGui::EndMenu();
			}

			// --- Layout Menu ---
			if (ImGui::BeginMenu("Layout")) {

				// Explicit-save model: arrange freely, then save. The saved
				// layout is what comes back on the next start; unsaved changes
				// last only for this session.
				bool render_mode = (m_SceneState == SceneState::Render);
				if (ImGui::MenuItem(render_mode ? "Save Layout (Render)" : "Save Layout (Edit)")) {
					save_current_layout();
				}
				if (ImGui::MenuItem("Reset Layout to Default")) {
					reset_layout_to_default();
				}

				ImGui::Separator();
				// Edit and Render each remember their own panel arrangement and
				// swap automatically when switching modes (Play shares Edit's).
				if (ImGui::MenuItem("Per-Mode Layouts (Auto)", nullptr, &m_AutoModeLayouts)) {
					save_engine_config();
				}

				ImGui::Separator();
				// Exports the current arrangement (+ panel visibility) as the
				// default a fresh install starts with.
				if (ImGui::MenuItem("Save As Shipped Default")) {
					save_layout_as_default();
				}

				ImGui::Separator();
				ImGui::TextDisabled("UI Scale");
				ImGui::Separator();

				float current_scale = ImGui::GetIO().FontGlobalScale;
				float scales[] = { 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };
				const char* scale_labels[] = { "75%", "100% (Default)", "125%", "150%", "200%" };

				for (int i = 0; i < 5; ++i) {
					if (ImGui::MenuItem(scale_labels[i], "", current_scale == scales[i])) {
						ImGui::GetIO().FontGlobalScale = scales[i];
						save_engine_config();
					}
				}

				ImGui::EndMenu();
			}

			// --- View Menu ---
			if (ImGui::BeginMenu("View")) {

				// Lighting Toggle Checkbox
				bool lighting = me::render::is_lighting_enabled();
				if (ImGui::MenuItem("Lit Mode (Lighting)", nullptr, &lighting)) {
					me::render::set_lighting_enabled(lighting);
				}

				ImGui::MenuItem("Stats Overlay", nullptr, &m_ShowStats);
				ImGui::MenuItem("Controls", nullptr, &m_ShowControlsWindow);

				// Per-mode panel visibility — each mode remembers its own set, so
				// Edit can stay a scene-building workspace and Render a film one.
				ImGui::Separator();
				ImGui::TextDisabled(m_SceneState == SceneState::Render ? "Panels (Render mode)" : "Panels (Edit mode)");
				PanelSet& p = active_panels();
				bool changed = false;
				changed |= ImGui::MenuItem("Scene Hierarchy", nullptr, &p.hierarchy);
				changed |= ImGui::MenuItem("Inspector", nullptr, &p.inspector);
				changed |= ImGui::MenuItem("Content Browser", nullptr, &p.browser);
				changed |= ImGui::MenuItem("Console", nullptr, &p.console);
				changed |= ImGui::MenuItem("Animation", nullptr, &p.animation);
				if (m_SceneState == SceneState::Render)
					changed |= ImGui::MenuItem("Raytracer Settings", nullptr, &p.raytracer_settings);
				if (changed) save_engine_config();

				ImGui::EndMenu();
			}

			// --- Help Menu ---
			if (ImGui::BeginMenu("Help")) {
				ImGui::MenuItem("Controls", nullptr, &m_ShowControlsWindow);
				ImGui::Separator();
				if (ImGui::MenuItem("About Mini Engine Raylib")) m_ShowAboutModal = true;
				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}
	}

	void EditorApp::draw_toolbar() {
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
		ImGui::SetNextWindowBgAlpha(0.0f);

		// Create the toolbar window
		ImGui::Begin("##Toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		float button_size = 26.0f;

		// ==========================================
		// GIZMO CONTROLS (Left Aligned)
		// ==========================================
		bool can_edit = (m_SceneState == SceneState::Edit);
		if (!can_edit) ImGui::BeginDisabled();

		// Translate (T)
		ImGui::PushStyleColor(ImGuiCol_Button, m_GizmoType == ImGuizmo::TRANSLATE ? ImVec4(0.2f, 0.6f, 0.9f, 1.0f) : ImVec4(0.16f, 0.16f, 0.21f, 1.0f));
		if (ImGui::Button("T", ImVec2(button_size, button_size))) m_GizmoType = ImGuizmo::TRANSLATE;
		ImGui::PopStyleColor();
		ImGui::SameLine(0, 5);

		// Rotate (R)
		ImGui::PushStyleColor(ImGuiCol_Button, m_GizmoType == ImGuizmo::ROTATE ? ImVec4(0.2f, 0.6f, 0.9f, 1.0f) : ImVec4(0.16f, 0.16f, 0.21f, 1.0f));
		if (ImGui::Button("R", ImVec2(button_size, button_size))) m_GizmoType = ImGuizmo::ROTATE;
		ImGui::PopStyleColor();
		ImGui::SameLine(0, 5);

		// Scale (S)
		ImGui::PushStyleColor(ImGuiCol_Button, m_GizmoType == ImGuizmo::SCALE ? ImVec4(0.2f, 0.6f, 0.9f, 1.0f) : ImVec4(0.16f, 0.16f, 0.21f, 1.0f));
		if (ImGui::Button("S", ImVec2(button_size, button_size))) m_GizmoType = ImGuizmo::SCALE;
		ImGui::PopStyleColor();

		if (!can_edit) ImGui::EndDisabled();

		// ==========================================
		// MAIN STATE CONTROLS (Center Aligned)
		// ==========================================
		if (m_SceneState == SceneState::Edit) {

			// Layout: [ PLAY ] [ RENDER ]
			float total_width = 60.0f + 5.0f + 60.0f;
			ImGui::SameLine((ImGui::GetWindowContentRegionMax().x * 0.5f) - (total_width * 0.5f));

			// PLAY Button
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
			if (ImGui::Button("PLAY", ImVec2(60, button_size))) {
				m_SceneState = SceneState::Play;
				on_play();
				me::set_paused(false);
			}
			ImGui::PopStyleColor();

			ImGui::SameLine(0, 5);

			// RENDER Button
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.2f, 0.8f, 1.0f));
			if (ImGui::Button("RENDER", ImVec2(60, button_size))) {
				m_SceneState = SceneState::Render;
				apply_frame_pacing(); // uncap: every frame is a raytrace sample
				switch_mode_layout(SceneState::Edit, SceneState::Render);

				// Get the raw bounds of the viewport
				float raw_width = m_ViewportPanel.get_bounds().x;
				float raw_height = m_ViewportPanel.get_bounds().y;

				// Fallback in case the viewport isn't initialized properly yet
				if (raw_width <= 0 || raw_height <= 0) {
					raw_width = 1280.0f;
					raw_height = 720.0f;
				}

				// Apply the resolution scale
				int scaled_width = (int)(raw_width * m_Raytracer.resolution_scale);
				int scaled_height = (int)(raw_height * m_Raytracer.resolution_scale);

				// Safeguard against 0-pixel rendering
				if (scaled_width < 1) scaled_width = 1;
				if (scaled_height < 1) scaled_height = 1;

				m_Raytracer.on_start(scaled_width, scaled_height);
				m_Raytracer.reset_accumulation(&me::get_registry());
			}
			ImGui::PopStyleColor();

			// Path-traced play: the next PLAY renders through the raytracer.
			ImGui::SameLine(0, 10);
			ImGui::Checkbox("RT", &m_RaytracePlayMode);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Path trace Play mode (real-time, retro resolutions).\nUses the Raytracer Settings' resolution scale; tune samples/frame\nin the Raytracer Settings panel while playing.");

		} else if (m_SceneState == SceneState::Play) {

			// Layout: [ STOP ] [ PAUSE/RESUME ] [ STEP ]
			float total_width = 60.0f + 5.0f + 60.0f + 5.0f + 60.0f;
			ImGui::SameLine((ImGui::GetWindowContentRegionMax().x * 0.5f) - (total_width * 0.5f));

			// STOP Button
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
			if (ImGui::Button("STOP", ImVec2(60, button_size))) {
				on_stop();
				m_SceneState = SceneState::Edit; // Return to Edit mode
			}
			ImGui::PopStyleColor();

			ImGui::SameLine(0, 5);

			// PAUSE/RESUME Button
			bool is_paused = me::is_paused();
			ImGui::PushStyleColor(ImGuiCol_Button, is_paused ? ImVec4(0.2f, 0.6f, 0.9f, 1.0f) : ImVec4(0.8f, 0.6f, 0.1f, 1.0f));
			if (ImGui::Button(is_paused ? "RESUME" : "PAUSE", ImVec2(60, button_size))) {
				me::set_paused(!is_paused);
				me::get_event_bus().publish<me::events::PauseStateChangedEvent>(!is_paused);
			}
			ImGui::PopStyleColor();

			ImGui::SameLine(0, 5);

			// STEP Button
			if (!is_paused) ImGui::BeginDisabled();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
			if (ImGui::Button("STEP", ImVec2(60, button_size))) {
				me::step(1);
			}
			ImGui::PopStyleColor();
			if (!is_paused) ImGui::EndDisabled();

		} else if (m_SceneState == SceneState::Render) {

			// Layout: [ STOP RENDER ]
			float total_width = 100.0f;
			ImGui::SameLine((ImGui::GetWindowContentRegionMax().x * 0.5f) - (total_width * 0.5f));

			// STOP RENDER Button
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
			if (ImGui::Button("STOP RENDER", ImVec2(100, button_size))) {
				m_Raytracer.on_stop();           // Free the VRAM and vectors
				m_SceneState = SceneState::Edit; // Return to Edit mode safely
				apply_frame_pacing();            // re-cap to the monitor refresh
				switch_mode_layout(SceneState::Render, SceneState::Edit);
			}
			ImGui::PopStyleColor();
		}

		// Mode badge (right-aligned): mirrors the viewport frame color.
		const char* mode_label = "EDIT";
		ImVec4 mode_col = ImVec4(0.62f, 0.62f, 0.62f, 1.0f);
		if (m_SceneState == SceneState::Play) {
			// Tell the user how to hand input to the game vs. back to the editor.
			mode_label = m_PlaytestFocused ? "PLAYING  (Esc to release)" : "PLAYING  (click viewport)";
			mode_col = ImVec4(0.35f, 0.8f, 0.45f, 1.0f);
		}
		if (m_SceneState == SceneState::Render) { mode_label = "RENDERING"; mode_col = ImVec4(0.72f, 0.45f, 0.95f, 1.0f); }
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(mode_label).x - 14.0f);
		ImGui::TextColored(mode_col, "%s", mode_label);

		ImGui::End();
		ImGui::PopStyleVar(1);
	}

	void EditorApp::draw_overlays() {
		const ImGuiWindowFlags overlay_flags =
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoDocking;

		const ImGuiViewport* vp = ImGui::GetMainViewport();

		// Fly-speed toast: brief feedback while scrolling to tune the camera speed.
		if (m_FlySpeedToastTimer > 0.0f) {
			ImGui::SetNextWindowPos({ vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + 48.0f }, ImGuiCond_Always, { 0.5f, 0.0f });
			ImGui::SetNextWindowBgAlpha(0.65f);
			if (ImGui::Begin("##FlySpeedToast", nullptr, overlay_flags)) {
				ImGui::Text("Fly Speed: %.1f", m_EditorCamera.move_speed);
			}
			ImGui::End();
		}

		// Stats overlay (View > Stats Overlay): frame timing + scene/raytracer info.
		if (m_ShowStats) {
			ImGui::SetNextWindowPos({ vp->WorkPos.x + vp->WorkSize.x - 12.0f, vp->WorkPos.y + 48.0f }, ImGuiCond_Always, { 1.0f, 0.0f });
			ImGui::SetNextWindowBgAlpha(0.55f);
			if (ImGui::Begin("##StatsOverlay", nullptr, overlay_flags)) {
				ImGui::Text("FPS: %d (%.2f ms)", GetFPS(), GetFrameTime() * 1000.0f);
				int entity_count = 0;
				for (auto e : me::get_registry().view<me::components::TransformComponent>()) { (void)e; ++entity_count; }
				ImGui::Text("Entities: %d", entity_count);
				if (m_SceneState == SceneState::Render) {
					ImGui::Separator();
					ImGui::Text("Raytracer: %dx%d (%s)",
						m_Raytracer.get_width(), m_Raytracer.get_height(),
						m_Raytracer.current_backend == me::systems::RenderBackend::GPU ? "GPU" : "CPU");
					ImGui::Text("Samples: %d / %d", m_Raytracer.get_accumulated_frames(), m_Raytracer.preview_samples);
				}
			}
			ImGui::End();
		}
	}

	void EditorApp::draw_modals() {
		// Center modals on the screen
		ImGui::SetNextWindowPos(ImVec2(GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		// ==========================================
		// CREATE NEW SCENE MODAL
		// ==========================================
		if (m_ShowNewSceneModal) ImGui::OpenPopup("Create New Scene");
		if (ImGui::BeginPopupModal("Create New Scene", &m_ShowNewSceneModal, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Enter scene name:");
			ImGui::InputText("##SceneName", m_NewSceneInput, sizeof(m_NewSceneInput));
			ImGui::Dummy(ImVec2(0, 10));

			if (ImGui::Button("Create", ImVec2(120, 0))) {
				std::string filename = std::string(m_NewSceneInput);
				if (filename.find(".json") == std::string::npos) filename += ".json";
				m_CurrentScenePath = "game://scenes/" + filename;
				new_scene();
				save_scene();
				remember_last_scene();
				m_ShowNewSceneModal = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) { m_ShowNewSceneModal = false; ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}

		// ==========================================
		// SAVE SCENE AS MODAL
		// ==========================================
		if (m_ShowSaveAsModal) ImGui::OpenPopup("Save Scene As");
		if (ImGui::BeginPopupModal("Save Scene As", &m_ShowSaveAsModal, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Enter scene name:");
			ImGui::InputText("##SaveAsName", m_NewSceneInput, sizeof(m_NewSceneInput));
			ImGui::Dummy(ImVec2(0, 10));

			if (ImGui::Button("Save", ImVec2(120, 0))) {
				std::string filename = std::string(m_NewSceneInput);
				if (filename.find(".json") == std::string::npos) filename += ".json";
				m_CurrentScenePath = "game://scenes/" + filename;
				save_scene();
				remember_last_scene();
				me::logger::info("Saved scene as " + m_CurrentScenePath);
				m_ShowSaveAsModal = false;
				ImGui::CloseCurrentPopup();
				// If this Save As was the "Save" answer to an unsaved-changes prompt,
				// the queued action (exit / new / open) was waiting for a name.
				if (m_PendingAction != PendingAction::None) perform_pending_action();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				// Cancelling Save As also cancels any action that was waiting on it.
				m_PendingAction = PendingAction::None;
				m_ShowSaveAsModal = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		// ==========================================
		// EXPORT GAME MODAL
		// ==========================================
		if (m_ShowExportGameModal) ImGui::OpenPopup("Export Game");
		if (ImGui::BeginPopupModal("Export Game", &m_ShowExportGameModal, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::TextDisabled("Packages the prebuilt game runtime together with this project's");
			ImGui::TextDisabled("assets into a standalone folder. No compilation involved.");
			ImGui::Dummy(ImVec2(0, 6));

			ImGui::InputText("Game Name", m_ExportGameName, sizeof(m_ExportGameName));

			ImGui::InputText("Output Folder", m_ExportGameDir, sizeof(m_ExportGameDir));
			ImGui::SameLine();
			if (ImGui::Button("...")) {
				std::string picked = editor::utils::select_folder_dialog("Choose the export folder", m_ExportGameDir);
				if (!picked.empty()) snprintf(m_ExportGameDir, sizeof(m_ExportGameDir), "%s", picked.c_str());
			}

			// Which scene the game boots into.
			if (ImGui::BeginCombo("Main Scene", m_ExportGameScene.c_str())) {
				std::filesystem::path scenes_dir = m_ProjectPath / "assets" / "scenes";
				if (std::filesystem::exists(scenes_dir)) {
					for (const auto& entry : std::filesystem::directory_iterator(scenes_dir)) {
						if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
						std::string fname = entry.path().filename().string();
						if (fname == ".temp_play.json" || fname.ends_with(".anim.json")) continue;
						std::string vfs_path = "game://scenes/" + fname;
						if (ImGui::Selectable(fname.c_str(), vfs_path == m_ExportGameScene))
							m_ExportGameScene = vfs_path;
					}
				}
				ImGui::EndCombo();
			}

			ImGui::InputInt("Window Width", &m_ExportGameW);
			ImGui::InputInt("Window Height", &m_ExportGameH);
			ImGui::Checkbox("VSync", &m_ExportGameVsync);

			ImGui::Dummy(ImVec2(0, 6));
			ImGui::Checkbox("Raytraced renderer", &m_ExportGameRaytraced);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("The game renders through the real-time path tracer instead of the\nrasterizer — meant for retro/pixelated games (low internal resolution\nbuys the samples). Needs a GPU with compute shaders (OpenGL 4.3);\nfalls back to the (slow) CPU tracer without one.\n\nResolution scale, samples/frame, bounces, sky and exposure are\ncaptured from the current Raytracer Settings at export time.");
			if (m_ExportGameRaytraced) {
				ImGui::TextDisabled("internal resolution: %d x %d  (scale %.2f)",
					std::max(1, (int)(m_ExportGameW * m_Raytracer.resolution_scale)),
					std::max(1, (int)(m_ExportGameH * m_Raytracer.resolution_scale)),
					m_Raytracer.resolution_scale);
				ImGui::TextDisabled("%d samples/frame, %d bounces",
					m_Raytracer.play_samples_per_frame, m_Raytracer.max_bounces);
			}

			ImGui::Dummy(ImVec2(0, 10));
			if (ImGui::Button("Export", ImVec2(120, 0))) {
				export_game();
				m_ShowExportGameModal = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) { m_ShowExportGameModal = false; ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}

		// ==========================================
		// ABOUT MODAL + CONTROLS WINDOW (Help menu)
		// ==========================================
		if (m_ShowAboutModal) ImGui::OpenPopup("About Mini Engine Raylib");
		if (ImGui::BeginPopupModal("About Mini Engine Raylib", &m_ShowAboutModal, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Mini Engine Raylib %s", ME_VERSION_STRING);
			ImGui::TextDisabled("A small scene engine built on raylib.");
			ImGui::TextDisabled("Build a scene once: play it as a game, or render it as film.");
			ImGui::Dummy(ImVec2(0, 6));
			ImGui::Text("(c) 2025-2026 Vasco Alves - MIT License");
			ImGui::TextDisabled("Built on raylib, Dear ImGui, Jolt Physics, Lua/sol3,");
			ImGui::TextDisabled("nlohmann_json, ImGuizmo and mini-ecs.");
			ImGui::Dummy(ImVec2(0, 8));
			if (ImGui::Button("Close", ImVec2(120, 0))) { m_ShowAboutModal = false; ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}

		if (m_ShowControlsWindow) {
			ImGui::SetNextWindowSize(ImVec2(460, 0), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Controls", &m_ShowControlsWindow, ImGuiWindowFlags_AlwaysAutoResize)) {
				if (ImGui::BeginTable("##controls", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
					auto row = [](const char* action, const char* input) {
						ImGui::TableNextRow();
						ImGui::TableNextColumn(); ImGui::TextUnformatted(action);
						ImGui::TableNextColumn(); ImGui::TextDisabled("%s", input);
						};
					row("Fly camera", "Hold RMB + WASD, Q/E up/down");
					row("Camera sprint / precision", "Shift / Ctrl while flying");
					row("Tune fly speed", "Scroll while flying");
					row("Orbit camera", "RMB + Alt");
					row("Focus selected", "F");
					row("Gizmo: none/move/rotate/scale", "Q / W / R / S  (Ctrl = snap)");
					row("Select object", "Left-click in viewport");
					row("Rename entity", "F2 or double-click in hierarchy");
					row("Duplicate / delete", "Ctrl+D / Del");
					row("Undo / redo", "Ctrl+Z / Ctrl+Y or Ctrl+Shift+Z");
					row("Save / Save As / new scene", "Ctrl+S / Ctrl+Shift+S / Ctrl+N");
					row("Play / stop", "Ctrl+P or the toolbar");
					row("Fine-tune drag fields", "Hold Alt (10x finer), Ctrl+click to type");
					row("Timeline zoom", "Ctrl+scroll over the timeline");
					row("DoF focus (Render mode)", "Click an object in the viewport");
					ImGui::EndTable();
				}
			}
			ImGui::End();
		}

		// ==========================================
		// UNSAVED CHANGES GUARD (Exit / New Scene / Open Scene)
		// ==========================================
		if (m_ShowUnsavedModal) ImGui::OpenPopup("Unsaved Changes");
		if (ImGui::BeginPopupModal("Unsaved Changes", &m_ShowUnsavedModal, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("The current scene has unsaved changes.");
			ImGui::TextDisabled("%s", m_CurrentScenePath.c_str());
			ImGui::Dummy(ImVec2(0, 10));

			if (ImGui::Button("Save", ImVec2(110, 0))) {
				bool saved = save_scene();
				m_ShowUnsavedModal = false;
				ImGui::CloseCurrentPopup();
				// If the scene was untitled, save_scene() opened Save As instead;
				// the parked action runs once a name is chosen (see Save As modal).
				if (saved) perform_pending_action();
			}
			ImGui::SameLine();
			if (ImGui::Button("Don't Save", ImVec2(110, 0))) {
				m_ShowUnsavedModal = false;
				ImGui::CloseCurrentPopup();
				perform_pending_action();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(110, 0))) {
				m_PendingAction = PendingAction::None;
				m_ShowUnsavedModal = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		draw_offline_render_modals();
	}

	// ====================================================================
	// THEME
	// ====================================================================

	void EditorApp::apply_theme() {
		ImGuiStyle& style = ImGui::GetStyle();
		auto& colors = style.Colors;

		colors[ImGuiCol_WindowBg] = ImVec4{ 0.11f, 0.11f, 0.13f, 1.0f };
		colors[ImGuiCol_MenuBarBg] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
		colors[ImGuiCol_Border] = ImVec4{ 0.44f, 0.37f, 0.61f, 0.29f };
		colors[ImGuiCol_BorderShadow] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.24f };
		colors[ImGuiCol_Text] = ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f };
		colors[ImGuiCol_TextDisabled] = ImVec4{ 0.5f, 0.5f, 0.5f, 1.0f };
		colors[ImGuiCol_Header] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
		colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.24f, 0.24f, 0.32f, 1.0f };
		colors[ImGuiCol_HeaderActive] = ImVec4{ 0.20f, 0.20f, 0.27f, 1.0f };
		colors[ImGuiCol_Button] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.24f, 0.24f, 0.32f, 1.0f };
		colors[ImGuiCol_ButtonActive] = ImVec4{ 0.20f, 0.20f, 0.27f, 1.0f };
		colors[ImGuiCol_CheckMark] = ImVec4{ 0.74f, 0.58f, 0.98f, 1.0f };
		colors[ImGuiCol_SliderGrab] = ImVec4{ 0.44f, 0.37f, 0.61f, 0.54f };
		colors[ImGuiCol_SliderGrabActive] = ImVec4{ 0.74f, 0.58f, 0.98f, 0.54f };
		colors[ImGuiCol_Tab] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
		colors[ImGuiCol_TabHovered] = ImVec4{ 0.24f, 0.24f, 0.32f, 1.0f };
		colors[ImGuiCol_TabActive] = ImVec4{ 0.20f, 0.20f, 0.27f, 1.0f };
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.16f, 0.16f, 0.21f, 1.0f };

		style.WindowPadding = ImVec2(8.0f, 8.0f);
		style.FramePadding = ImVec2(5.0f, 3.0f);
		style.CellPadding = ImVec2(6.0f, 6.0f);
		style.ItemSpacing = ImVec2(6.0f, 6.0f);
		style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
		style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
		style.IndentSpacing = 25;
		style.ScrollbarSize = 15;
		style.GrabMinSize = 10;
		style.WindowBorderSize = 1;
		style.ChildBorderSize = 1;
		style.PopupBorderSize = 1;
		style.FrameBorderSize = 1;
		style.TabBorderSize = 1;
		style.WindowRounding = 4;
		style.ChildRounding = 4;
		style.FrameRounding = 3;
		style.PopupRounding = 4;
		style.ScrollbarRounding = 9;
		style.GrabRounding = 3;
		style.TabRounding = 4;
	}

} // namespace editor