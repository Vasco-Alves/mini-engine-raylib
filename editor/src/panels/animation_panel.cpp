#include "editor/panels/animation_panel.hpp"

#include <mini-engine-raylib/core/engine.hpp>

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace editor {

	namespace {
		// Components that are safe to animate: pure data, no native handles
		// (asset-backed components would re-acquire their handle every frame).
		constexpr const char* kAnimatable[] = {
			"Transform", "Material", "MeshRenderer", "Light", "DirectionalLight", "Camera"
		};

		// --- Timeline metrics & colors ---
		constexpr float kLabelW = 200.0f; // track-name column width
		constexpr float kRulerH = 22.0f;
		constexpr float kRowH = 26.0f;
		constexpr float kRowGap = 2.0f;   // breathing room between track rows
		constexpr float kKeyRadius = 5.0f;
		constexpr float kHitRadius = 8.0f;

		const ImU32 kColRulerBg = IM_COL32(26, 26, 30, 255);
		const ImU32 kColLabelBg = IM_COL32(22, 22, 26, 255);
		const ImU32 kColRowA = IM_COL32(33, 33, 38, 255);
		const ImU32 kColRowB = IM_COL32(38, 38, 44, 255);
		const ImU32 kColTick = IM_COL32(95, 95, 105, 255);
		const ImU32 kColTickText = IM_COL32(150, 150, 158, 255);
		const ImU32 kColPlayhead = IM_COL32(255, 180, 60, 255);
		const ImU32 kColCamKey = IM_COL32(255, 170, 70, 255);
		const ImU32 kColEnvKey = IM_COL32(110, 220, 170, 255);
		const ImU32 kColEntityKey = IM_COL32(120, 180, 255, 255);
		const ImU32 kColKeySelected = IM_COL32(255, 255, 255, 255);

		// Whole-animation snapshot command. Animation data is small, so undo
		// simply swaps full before/after copies (live entity ids ride along).
		class AnimationEditCommand : public ICommand {
		public:
			AnimationEditCommand(SceneAnimation* target, SceneAnimation before, SceneAnimation after)
				: m_Target(target), m_Before(std::move(before)), m_After(std::move(after)) {}
			void Execute() override { *m_Target = m_After; }
			void Undo() override { *m_Target = m_Before; }
		private:
			SceneAnimation* m_Target;
			SceneAnimation m_Before;
			SceneAnimation m_After;
		};

		// Smallest "nice" tick step that keeps labels at least min_px apart.
		float nice_tick_step(float pixels_per_second, float min_px = 64.0f) {
			const float steps[] = { 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 30.0f, 60.0f };
			for (float s : steps)
				if (s * pixels_per_second >= min_px) return s;
			return 120.0f;
		}

		void draw_diamond(ImDrawList* dl, ImVec2 c, float r, ImU32 fill, bool selected) {
			ImVec2 pts[4] = { {c.x, c.y - r}, {c.x + r, c.y}, {c.x, c.y + r}, {c.x - r, c.y} };
			dl->AddQuadFilled(pts[0], pts[1], pts[2], pts[3], fill);
			if (selected) {
				float r2 = r + 2.0f;
				ImVec2 o[4] = { {c.x, c.y - r2}, {c.x + r2, c.y}, {c.x, c.y + r2}, {c.x - r2, c.y} };
				dl->AddQuad(o[0], o[1], o[2], o[3], kColKeySelected, 1.5f);
			}
		}

		// Capture the camera + lens settings into a key at time t (replacing a
		// key already sitting at that time instead of stacking a duplicate).
		void capture_camera_key(SceneAnimation& anim, float t,
			const me::components::TransformComponent& cam_transform,
			const me::components::CameraComponent& camera,
			const me::systems::RaytracerSystem& raytracer) {
			CameraKeyframe k;
			k.time = t;
			k.position = cam_transform.position;
			k.target = camera.target;
			k.fov = camera.fov;
			k.exposure = raytracer.exposure;
			k.aperture = raytracer.aperture;
			k.focus_distance = raytracer.focus_distance;

			auto same = std::find_if(anim.keys.begin(), anim.keys.end(),
				[&](const CameraKeyframe& e) { return std::abs(e.time - t) < 0.001f; });
			if (same != anim.keys.end()) { k.easing = same->easing; *same = k; } else anim.keys.push_back(k);
			anim.sort_keys();
		}

		// Capture the current environment (sky + ambient) into a key at time t.
		void capture_env_key(SceneAnimation& anim, float t,
			const me::systems::RaytracerSystem& raytracer) {
			EnvironmentKeyframe k;
			k.time = t;
			k.ambient_strength = raytracer.ambient_strength;
			k.sky_horizon = raytracer.sky_horizon_color;
			k.sky_zenith = raytracer.sky_zenith_color;
			k.sky_intensity = raytracer.sky_intensity;

			auto same = std::find_if(anim.env_keys.begin(), anim.env_keys.end(),
				[&](const EnvironmentKeyframe& e) { return std::abs(e.time - t) < 0.001f; });
			if (same != anim.env_keys.end()) { k.easing = same->easing; *same = k; } else anim.env_keys.push_back(k);
			anim.sort_env_keys();
		}

		// Capture the tracked component's current state into a key at time t.
		void capture_entity_key(EntityTrack& tr, float t, me::Registry& reg) {
			const auto* meta = me::ecs::find(tr.component);
			if (!meta || tr.entity == me::entity::null || !reg.is_alive(tr.entity)) return;
			if (!meta->has(reg, tr.entity)) return;

			EntityKey k;
			k.time = t;
			k.value = meta->save(reg, tr.entity);

			auto same = std::find_if(tr.keys.begin(), tr.keys.end(),
				[&](const EntityKey& e) { return std::abs(e.time - t) < 0.001f; });
			if (same != tr.keys.end()) same->value = k.value;
			else tr.keys.push_back(std::move(k));
			tr.sort_keys();
		}
	}

	void AnimationPanel::apply(SceneAnimation& anim, float t,
		me::components::TransformComponent& cam_transform,
		me::components::CameraComponent& camera,
		me::systems::RaytracerSystem& raytracer,
		bool in_render_mode) {

		if (!anim.keys.empty())
			apply_keyframe(anim.evaluate(t), cam_transform, camera, raytracer);
		if (!anim.env_keys.empty())
			apply_environment(anim.evaluate_env(t), raytracer);
		apply_entity_tracks(anim, t, me::get_registry());

		// The path tracer accumulates against a frozen scene — moving the
		// playhead restarts the average. Entity tracks change geometry and
		// materials, so they additionally need fresh model matrices and a
		// BVH/SSBO rebuild before the next sample.
		if (in_render_mode) {
			if (anim.entity_tracks.empty()) {
				raytracer.reset_accumulation();
			} else {
				me::world_update(0.0f); // transforms only (not playing)
				raytracer.reset_accumulation(&me::get_registry());
			}
		}
	}

	void AnimationPanel::update_preview(float dt, SceneAnimation& anim,
		me::components::TransformComponent& cam_transform,
		me::components::CameraComponent& camera,
		me::systems::RaytracerSystem& raytracer,
		bool in_render_mode) {
		if (!m_Previewing) return;
		if (!anim.is_renderable()) { m_Previewing = false; return; }

		m_Time += dt;
		if (m_Time > anim.duration()) {
			if (m_Loop) m_Time = 0.0f;
			else { m_Time = anim.duration(); m_Previewing = false; }
		}
		apply(anim, m_Time, cam_transform, camera, raytracer, in_render_mode);
	}

	void AnimationPanel::on_imgui_render(SceneAnimation& anim,
		me::components::TransformComponent& cam_transform,
		me::components::CameraComponent& camera,
		me::systems::RaytracerSystem& raytracer,
		me::Entity selected_entity,
		bool in_render_mode,
		editor::CommandHistory& history,
		const std::function<void()>& request_render) {

		auto& reg = me::get_registry();

		// Pushes an undoable whole-animation edit: copy, mutate, commit.
		auto undoable = [&](auto&& mutate) {
			SceneAnimation before = anim;
			mutate();
			history.AddCommand(std::make_unique<AnimationEditCommand>(&anim, std::move(before), anim));
			};
		// Undo tracking for continuous widgets (drag fields): call right after.
		auto track_value_edit = [&]() {
			if (ImGui::IsItemActivated()) { m_BeforeEdit = anim; m_BeforeEditValid = true; }
			if (ImGui::IsItemDeactivatedAfterEdit() && m_BeforeEditValid) {
				history.AddCommand(std::make_unique<AnimationEditCommand>(&anim, m_BeforeEdit, anim));
				m_BeforeEditValid = false;
			}
			};

		ImGui::Begin("Animation");

		// ==================================================================
		// TRANSPORT ROW: play / loop / time / fit, render button (right)
		// ==================================================================
		bool can_play = anim.is_renderable();
		if (!can_play) ImGui::BeginDisabled();
		if (ImGui::Button(m_Previewing ? "Stop" : "Play", ImVec2(60, 0))) {
			m_Previewing = !m_Previewing;
			if (m_Previewing && m_Time >= anim.duration()) m_Time = 0.0f;
		}
		if (!can_play) ImGui::EndDisabled();
		if (in_render_mode && ImGui::IsItemHovered())
			ImGui::SetTooltip("Plays inside the path tracer: 1 noisy sample per frame.\nStop on a frame to let it accumulate and clean up.");

		ImGui::SameLine();
		ImGui::Checkbox("Loop", &m_Loop);

		ImGui::SameLine();
		ImGui::Text("%.2fs / %.2fs", m_Time, anim.duration());

		ImGui::SameLine();
		if (ImGui::SmallButton("Fit")) m_Zoom = 1.0f;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset timeline zoom (Ctrl+scroll over the timeline zooms)");

		// Render button, right-aligned.
		bool can_render = anim.is_renderable() && in_render_mode;
		float render_w = 170.0f;
		ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 10.0f,
			ImGui::GetWindowContentRegionMax().x - render_w));
		if (!can_render) ImGui::BeginDisabled();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.2f, 0.8f, 1.0f));
		if (ImGui::Button("Render Animation...", ImVec2(render_w, 0))) {
			m_Previewing = false;
			request_render();
		}
		ImGui::PopStyleColor();
		if (!can_render) ImGui::EndDisabled();

		// ==================================================================
		// ADD TRACK ROW
		// ==================================================================
		if (selected_entity.is_valid()) {
			std::vector<const char*> present;
			for (const char* name : kAnimatable) {
				const auto* meta = me::ecs::find(name);
				if (meta && meta->has(reg, selected_entity.get_id())) present.push_back(name);
			}

			std::string sel_name = "Entity";
			if (auto* tag = selected_entity.try_get_component<me::components::TagComponent>())
				if (!tag->name.empty()) sel_name = tag->name;

			if (!present.empty()) {
				if (m_NewTrackComp >= (int)present.size()) m_NewTrackComp = 0;

				ImGui::SetNextItemWidth(130.0f);
				ImGui::Combo("##trackcomp", &m_NewTrackComp, present.data(), (int)present.size());

				bool exists = false;
				for (const auto& tr : anim.entity_tracks)
					if (tr.entity == selected_entity.get_id() && tr.component == present[m_NewTrackComp]) { exists = true; break; }

				ImGui::SameLine();
				if (exists) ImGui::BeginDisabled();
				std::string add_label = "Add Track: " + sel_name;
				if (ImGui::Button(add_label.c_str())) {
					undoable([&] {
						EntityTrack tr;
						tr.entity = selected_entity.get_id();
						tr.entity_name = sel_name;
						tr.component = present[m_NewTrackComp];
						anim.entity_tracks.push_back(std::move(tr));
						});
				}
				if (exists) { ImGui::EndDisabled(); ImGui::SameLine(); ImGui::TextDisabled("(tracked)"); }
			} else {
				ImGui::TextDisabled("Selection has no animatable components.");
			}
		} else {
			ImGui::TextDisabled("Select an entity to add a component track. \"+\" captures a key at the playhead.");
		}

		// ==================================================================
		// THE TIMELINE
		// ==================================================================
		// Fixed rows: 0 = Camera, 1 = Environment; entity tracks follow.
		int track_count = 2 + (int)anim.entity_tracks.size();
		float body_h = kRulerH + track_count * kRowH;
		float child_h = std::min(body_h + 16.0f, 320.0f);

		ImGui::BeginChild("##timeline", ImVec2(0, child_h), true, ImGuiWindowFlags_HorizontalScrollbar);

		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 view_avail = ImGui::GetContentRegionAvail();
		float view_left = ImGui::GetWindowPos().x;
		float view_right = view_left + ImGui::GetWindowSize().x;
		float t_max = std::max(anim.duration() + 1.0f, 5.0f);

		// Zoom: 1 = the whole animation fits the visible width.
		float time_view_w = std::max(view_avail.x - kLabelW, 80.0f);
		float time_content_w = time_view_w * m_Zoom;
		float content_w = kLabelW + time_content_w;

		ImVec2 origin = ImGui::GetCursorScreenPos(); // scroll-adjusted content origin
		float area_x = origin.x + kLabelW;           // time area left (content coords)

		auto time_to_x = [&](float t) { return area_x + (t / t_max) * time_content_w; };
		auto x_to_time = [&](float x) { return std::clamp(((x - area_x) / time_content_w) * t_max, 0.0f, t_max); };

		// 1) A non-interactive Dummy establishes the scrollable content extent.
		ImGui::SetCursorScreenPos(origin);
		ImGui::Dummy(ImVec2(content_w, body_h));

		// 2) The interaction surface is pinned over the VISIBLE time area only —
		//    it never overlaps the label column, so the per-track buttons there
		//    receive their clicks (an overlapping button submitted first would
		//    otherwise claim every mouse-down).
		ImGui::SetCursorScreenPos({ view_left + kLabelW, origin.y });
		ImGui::InvisibleButton("##timearea", ImVec2(std::max(time_view_w, 10.0f), body_h));
		bool area_active = ImGui::IsItemActive();
		ImVec2 mouse = ImGui::GetMousePos();

		// Ctrl+scroll = zoom.
		if (ImGui::IsItemHovered() && ImGui::GetIO().KeyCtrl) {
			float wheel = ImGui::GetIO().MouseWheel;
			if (wheel != 0.0f) m_Zoom = std::clamp(m_Zoom * (1.0f + 0.15f * wheel), 1.0f, 20.0f);
		}

		// --- Backgrounds (pinned horizontally, scrolled vertically) ---
		dl->AddRectFilled({ view_left, origin.y }, { view_right, origin.y + kRulerH }, kColRulerBg);
		for (int r = 0; r < track_count; ++r) {
			float ry = origin.y + kRulerH + r * kRowH;
			dl->AddRectFilled({ view_left + kLabelW, ry + kRowGap }, { view_right, ry + kRowH - kRowGap },
				(r & 1) ? kColRowB : kColRowA);
		}

		// --- Scrolled time content, clipped so it never draws under the labels ---
		dl->PushClipRect({ view_left + kLabelW, origin.y }, { view_right, origin.y + body_h }, true);

		float step = nice_tick_step(time_content_w / t_max);
		for (float t = 0.0f; t <= t_max + 0.0001f; t += step) {
			float x = time_to_x(t);
			dl->AddLine({ x, origin.y + kRulerH * 0.45f }, { x, origin.y + body_h }, kColTick, 1.0f);
			char buf[16];
			snprintf(buf, sizeof(buf), "%.4gs", t);
			dl->AddText({ x + 3.0f, origin.y + 2.0f }, kColTickText, buf);
		}

		// Keys.
		for (int i = 0; i < (int)anim.keys.size(); ++i) {
			ImVec2 c{ time_to_x(anim.keys[i].time), origin.y + kRulerH + kRowH * 0.5f };
			draw_diamond(dl, c, kKeyRadius, kColCamKey, m_SelTrack == kSelCamera && m_SelKey == i);
		}
		for (int i = 0; i < (int)anim.env_keys.size(); ++i) {
			ImVec2 c{ time_to_x(anim.env_keys[i].time), origin.y + kRulerH + kRowH * 1.5f };
			draw_diamond(dl, c, kKeyRadius, kColEnvKey, m_SelTrack == kSelEnv && m_SelKey == i);
		}
		for (int ti = 0; ti < (int)anim.entity_tracks.size(); ++ti) {
			for (int i = 0; i < (int)anim.entity_tracks[ti].keys.size(); ++i) {
				ImVec2 c{ time_to_x(anim.entity_tracks[ti].keys[i].time),
						  origin.y + kRulerH + (2 + ti) * kRowH + kRowH * 0.5f };
				draw_diamond(dl, c, kKeyRadius, kColEntityKey, m_SelTrack == ti && m_SelKey == i);
			}
		}

		// Playhead.
		{
			float px = time_to_x(std::clamp(m_Time, 0.0f, t_max));
			dl->AddLine({ px, origin.y }, { px, origin.y + body_h }, kColPlayhead, 2.0f);
			dl->AddTriangleFilled({ px - 5.0f, origin.y }, { px + 5.0f, origin.y }, { px, origin.y + 9.0f }, kColPlayhead);

			// Keep the playhead visible while previewing.
			if (m_Previewing) {
				float rel = kLabelW + (m_Time / t_max) * time_content_w; // content coords
				float sx = ImGui::GetScrollX();
				if (rel < sx + kLabelW + 10.0f || rel > sx + view_avail.x - 20.0f)
					ImGui::SetScrollX(std::max(0.0f, rel - kLabelW - time_view_w * 0.3f));
			}
		}

		dl->PopClipRect();

		// --- Hit testing (track ids: kSelCamera, kSelEnv, or entity index) ---
		auto hit_test = [&](int& out_track, int& out_key) -> bool {
			auto test = [&](int row, float key_time, int track, int key) {
				ImVec2 c{ time_to_x(key_time), origin.y + kRulerH + row * kRowH + kRowH * 0.5f };
				float dx = mouse.x - c.x, dy = mouse.y - c.y;
				if (dx * dx + dy * dy <= kHitRadius * kHitRadius) { out_track = track; out_key = key; return true; }
				return false;
				};
			for (int i = 0; i < (int)anim.keys.size(); ++i)
				if (test(0, anim.keys[i].time, kSelCamera, i)) return true;
			for (int i = 0; i < (int)anim.env_keys.size(); ++i)
				if (test(1, anim.env_keys[i].time, kSelEnv, i)) return true;
			for (int ti = 0; ti < (int)anim.entity_tracks.size(); ++ti)
				for (int i = 0; i < (int)anim.entity_tracks[ti].keys.size(); ++i)
					if (test(2 + ti, anim.entity_tracks[ti].keys[i].time, ti, i)) return true;
			return false;
			};

		// Accessors for the selected key (nullptr when invalid).
		auto sel_time_ptr = [&]() -> float* {
			if (m_SelTrack == kSelCamera && m_SelKey >= 0 && m_SelKey < (int)anim.keys.size()) return &anim.keys[m_SelKey].time;
			if (m_SelTrack == kSelEnv && m_SelKey >= 0 && m_SelKey < (int)anim.env_keys.size()) return &anim.env_keys[m_SelKey].time;
			if (m_SelTrack >= 0 && m_SelTrack < (int)anim.entity_tracks.size()
				&& m_SelKey >= 0 && m_SelKey < (int)anim.entity_tracks[m_SelTrack].keys.size())
				return &anim.entity_tracks[m_SelTrack].keys[m_SelKey].time;
			return nullptr;
			};
		auto sel_easing_ptr = [&]() -> int* {
			if (m_SelTrack == kSelCamera && m_SelKey >= 0 && m_SelKey < (int)anim.keys.size()) return &anim.keys[m_SelKey].easing;
			if (m_SelTrack == kSelEnv && m_SelKey >= 0 && m_SelKey < (int)anim.env_keys.size()) return &anim.env_keys[m_SelKey].easing;
			if (m_SelTrack >= 0 && m_SelTrack < (int)anim.entity_tracks.size()
				&& m_SelKey >= 0 && m_SelKey < (int)anim.entity_tracks[m_SelTrack].keys.size())
				return &anim.entity_tracks[m_SelTrack].keys[m_SelKey].easing;
			return nullptr;
			};
		auto resort_selected_track = [&]() {
			float* tp = sel_time_ptr();
			if (!tp) return;
			float sel_time = *tp;
			auto resort = [&](auto& keys) {
				std::stable_sort(keys.begin(), keys.end(),
					[](const auto& a, const auto& b) { return a.time < b.time; });
				for (int i = 0; i < (int)keys.size(); ++i)
					if (keys[i].time == sel_time) { m_SelKey = i; break; }
				};
			if (m_SelTrack == kSelCamera) resort(anim.keys);
			else if (m_SelTrack == kSelEnv) resort(anim.env_keys);
			else if (m_SelTrack >= 0 && m_SelTrack < (int)anim.entity_tracks.size()) resort(anim.entity_tracks[m_SelTrack].keys);
			};

		if (ImGui::IsItemActivated()) {
			int t, k;
			if (hit_test(t, k)) {
				m_SelTrack = t;
				m_SelKey = k;
				m_DraggingKey = true;
				float* tp = sel_time_ptr();
				m_DragStartTime = tp ? *tp : 0.0f;
				m_BeforeEdit = anim; // undo snapshot for the drag
				m_BeforeEditValid = true;
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					// Double-click: jump the playhead to the key.
					m_Time = m_DragStartTime;
					apply(anim, m_Time, cam_transform, camera, raytracer, in_render_mode);
					m_DraggingKey = false;
					m_BeforeEditValid = false;
				}
			} else {
				m_DraggingPlayhead = true; // the surface only covers the time area
			}
		}

		if (area_active && m_DraggingKey) {
			if (float* tp = sel_time_ptr()) *tp = x_to_time(mouse.x);
		}

		if (area_active && m_DraggingPlayhead) {
			float nt = x_to_time(mouse.x);
			if (std::abs(nt - m_Time) > 1e-5f) {
				m_Time = nt;
				if (!anim.empty()) apply(anim, m_Time, cam_transform, camera, raytracer, in_render_mode);
			}
		}

		if (ImGui::IsItemDeactivated()) {
			if (m_DraggingKey) {
				float* tp = sel_time_ptr();
				bool moved = tp && std::abs(*tp - m_DragStartTime) > 1e-5f;
				resort_selected_track();
				if (moved && m_BeforeEditValid) {
					history.AddCommand(std::make_unique<AnimationEditCommand>(&anim, m_BeforeEdit, anim));
				}
				m_BeforeEditValid = false;
			}
			m_DraggingKey = false;
			m_DraggingPlayhead = false;
		}

		// Right-click a key: select it and open its context menu.
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
			int t, k;
			if (hit_test(t, k)) {
				m_SelTrack = t;
				m_SelKey = k;
				ImGui::OpenPopup("##key_ctx");
			}
		}

		// --- Pinned track labels + per-track buttons (after the interaction
		//     surface so they win the overlap when the timeline is scrolled). ---
		dl->AddRectFilled({ view_left, origin.y + kRulerH }, { view_left + kLabelW, origin.y + body_h }, kColLabelBg);
		dl->AddRectFilled({ view_left, origin.y }, { view_left + kLabelW, origin.y + kRulerH }, kColRulerBg);

		// Track name, clipped to the label column (full name in a tooltip when
		// it doesn't fit). The clip keeps long names from spilling onto the
		// timeline; the right margin keeps clear of the row's +/x buttons.
		auto label_row = [&](int row, const char* text, bool dim, float button_w) {
			float y = origin.y + kRulerH + row * kRowH + 4.0f;
			float clip_right = view_left + kLabelW - button_w - 6.0f;
			bool truncated = ImGui::CalcTextSize(text).x > (clip_right - (view_left + 6.0f));

			ImGui::PushClipRect({ view_left, y - 4.0f }, { clip_right, y + kRowH - 4.0f }, true);
			ImGui::SetCursorScreenPos({ view_left + 6.0f, y });
			if (dim) ImGui::TextDisabled("%s", text);
			else     ImGui::Text("%s", text);
			ImGui::PopClipRect();

			if (truncated && ImGui::IsItemHovered() && ImGui::GetMousePos().x < clip_right)
				ImGui::SetTooltip("%s", text);
			};

		// Camera row.
		label_row(0, "Camera", anim.keys.empty(), 26.0f);
		ImGui::SetCursorScreenPos({ view_left + kLabelW - 26.0f, origin.y + kRulerH + 1.0f });
		ImGui::PushID("camrow");
		if (ImGui::SmallButton("+")) {
			undoable([&] { capture_camera_key(anim, m_Time, cam_transform, camera, raytracer); });
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Capture a camera + lens key at the playhead");
		ImGui::PopID();

		// Environment row.
		label_row(1, "Environment", anim.env_keys.empty(), 26.0f);
		ImGui::SetCursorScreenPos({ view_left + kLabelW - 26.0f, origin.y + kRulerH + kRowH + 1.0f });
		ImGui::PushID("envrow");
		if (ImGui::SmallButton("+")) {
			undoable([&] { capture_env_key(anim, m_Time, raytracer); });
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Capture the sky + ambient (Raytracer Settings > Environment) at the playhead");
		ImGui::PopID();

		// Entity track rows.
		int delete_track = -1;
		for (int ti = 0; ti < (int)anim.entity_tracks.size(); ++ti) {
			auto& tr = anim.entity_tracks[ti];

			std::string display = tr.entity_name;
			bool alive = (tr.entity != me::entity::null) && reg.is_alive(tr.entity);
			if (alive) {
				if (auto* tag = reg.try_get_component<me::components::TagComponent>(tr.entity))
					if (!tag->name.empty()) display = tag->name;
			} else {
				display += " (missing)";
			}
			display += " · " + tr.component;

			label_row(2 + ti, display.c_str(), !alive, 46.0f); // room for + and x

			ImGui::PushID(100 + ti);
			ImGui::SetCursorScreenPos({ view_left + kLabelW - 46.0f, origin.y + kRulerH + (2 + ti) * kRowH + 1.0f });
			if (!alive) ImGui::BeginDisabled();
			if (ImGui::SmallButton("+")) {
				undoable([&] { capture_entity_key(tr, m_Time, reg); });
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Capture this component's current state at the playhead");
			if (!alive) ImGui::EndDisabled();
			ImGui::SameLine(0.0f, 2.0f);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
			if (ImGui::SmallButton("x")) delete_track = ti;
			ImGui::PopStyleColor();
			ImGui::PopID();
		}

		// --- Key context menu ---
		if (ImGui::BeginPopup("##key_ctx")) {
			int* easing = sel_easing_ptr();
			if (!easing) {
				ImGui::CloseCurrentPopup();
			} else {
				if (ImGui::MenuItem("Update From Current Scene")) {
					float kt = *sel_time_ptr();
					undoable([&] {
						if (m_SelTrack == kSelCamera) capture_camera_key(anim, kt, cam_transform, camera, raytracer);
						else if (m_SelTrack == kSelEnv) capture_env_key(anim, kt, raytracer);
						else capture_entity_key(anim.entity_tracks[m_SelTrack], kt, reg);
						});
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Linear", nullptr, *easing == 0)) undoable([&] { *sel_easing_ptr() = 0; });
				if (ImGui::MenuItem("Smooth", nullptr, *easing == 1)) undoable([&] { *sel_easing_ptr() = 1; });
				ImGui::Separator();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
				if (ImGui::MenuItem("Delete Key")) {
					undoable([&] {
						if (m_SelTrack == kSelCamera) anim.keys.erase(anim.keys.begin() + m_SelKey);
						else if (m_SelTrack == kSelEnv) anim.env_keys.erase(anim.env_keys.begin() + m_SelKey);
						else anim.entity_tracks[m_SelTrack].keys.erase(anim.entity_tracks[m_SelTrack].keys.begin() + m_SelKey);
						});
					m_SelTrack = kSelNone; m_SelKey = -1;
				}
				ImGui::PopStyleColor();
			}
			ImGui::EndPopup();
		}

		ImGui::EndChild();

		if (delete_track >= 0) {
			undoable([&] { anim.entity_tracks.erase(anim.entity_tracks.begin() + delete_track); });
			if (m_SelTrack == delete_track) { m_SelTrack = kSelNone; m_SelKey = -1; }
			else if (m_SelTrack > delete_track) m_SelTrack--;
		}

		// ==================================================================
		// SELECTED KEY EDITOR
		// ==================================================================
		float* time_ptr = sel_time_ptr();
		int* easing_ptr = sel_easing_ptr();

		if (time_ptr && easing_ptr) {
			const char* what = (m_SelTrack == kSelCamera) ? "Camera key:"
				: (m_SelTrack == kSelEnv) ? "Environment key:" : "Key:";
			ImGui::AlignTextToFramePadding();
			ImGui::Text("%s", what);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(90.0f);
			ImGui::DragFloat("##seltime", time_ptr, 0.05f, 0.0f, 3600.0f, "%.2fs");
			if (ImGui::IsItemActivated()) { m_BeforeEdit = anim; m_BeforeEditValid = true; }
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				// Keys must stay time-ordered BEFORE the undo state is captured.
				resort_selected_track();
				if (m_BeforeEditValid) {
					history.AddCommand(std::make_unique<AnimationEditCommand>(&anim, m_BeforeEdit, anim));
					m_BeforeEditValid = false;
				}
			}

			ImGui::SameLine();
			ImGui::SetNextItemWidth(90.0f);
			const char* easings[] = { "Linear", "Smooth" };
			int easing_before = *easing_ptr;
			if (ImGui::Combo("##seleasing", easing_ptr, easings, 2)) {
				int chosen = *easing_ptr;
				*easing_ptr = easing_before; // revert so the undo snapshot is pre-change
				undoable([&] { *sel_easing_ptr() = chosen; });
			}

			ImGui::SameLine();
			if (ImGui::Button("Go To")) {
				m_Time = *time_ptr;
				apply(anim, m_Time, cam_transform, camera, raytracer, in_render_mode);
			}

			ImGui::SameLine();
			if (ImGui::Button("Update From Scene")) {
				float kt = *time_ptr;
				undoable([&] {
					if (m_SelTrack == kSelCamera) capture_camera_key(anim, kt, cam_transform, camera, raytracer);
					else if (m_SelTrack == kSelEnv) capture_env_key(anim, kt, raytracer);
					else if (m_SelTrack >= 0) capture_entity_key(anim.entity_tracks[m_SelTrack], kt, reg);
					});
			}

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.2f, 0.2f, 1.0f));
			if (ImGui::Button("Delete")) {
				undoable([&] {
					if (m_SelTrack == kSelCamera) anim.keys.erase(anim.keys.begin() + m_SelKey);
					else if (m_SelTrack == kSelEnv) anim.env_keys.erase(anim.env_keys.begin() + m_SelKey);
					else if (m_SelTrack >= 0) anim.entity_tracks[m_SelTrack].keys.erase(anim.entity_tracks[m_SelTrack].keys.begin() + m_SelKey);
					});
				m_SelTrack = kSelNone; m_SelKey = -1;
			}
			ImGui::PopStyleColor();

			// --- Direct value editing for camera / environment keys ---
			if (m_SelTrack == kSelCamera && m_SelKey >= 0 && m_SelKey < (int)anim.keys.size()) {
				if (ImGui::TreeNode("Key Values")) {
					auto& k = anim.keys[m_SelKey];
					ImGui::DragFloat3("Position", &k.position.x, 0.1f);      track_value_edit();
					ImGui::DragFloat3("Look-at Target", &k.target.x, 0.1f);  track_value_edit();
					ImGui::DragFloat("FOV", &k.fov, 0.5f, 1.0f, 179.0f);     track_value_edit();
					ImGui::DragFloat("Exposure", &k.exposure, 0.02f, 0.0f, 3.0f);            track_value_edit();
					ImGui::DragFloat("Aperture", &k.aperture, 0.005f, 0.0f, 0.5f, "%.3f");   track_value_edit();
					ImGui::DragFloat("Focus Distance", &k.focus_distance, 0.1f, 0.1f, 100.0f); track_value_edit();
					ImGui::TreePop();
				}
			} else if (m_SelTrack == kSelEnv && m_SelKey >= 0 && m_SelKey < (int)anim.env_keys.size()) {
				if (ImGui::TreeNode("Key Values")) {
					auto& k = anim.env_keys[m_SelKey];
					ImGui::DragFloat("Ambient Fill", &k.ambient_strength, 0.005f, 0.0f, 1.0f, "%.3f"); track_value_edit();
					ImGui::ColorEdit3("Sky Horizon", &k.sky_horizon.x);  track_value_edit();
					ImGui::ColorEdit3("Sky Zenith", &k.sky_zenith.x);    track_value_edit();
					ImGui::DragFloat("Sky Intensity", &k.sky_intensity, 0.02f, 0.0f, 2.0f);  track_value_edit();
					ImGui::TreePop();
				}
			} else if (m_SelTrack >= 0) {
				ImGui::TextDisabled("Entity keys hold a full component snapshot - use \"Update From Scene\" to change them.");
			}
		} else {
			ImGui::TextDisabled("Click a key to select it - drag to retime, double-click to jump, right-click for options.");
		}

		if (!anim.is_renderable())
			ImGui::TextDisabled("Add at least 2 keys to a track to animate.");
		else if (!in_render_mode)
			ImGui::TextDisabled("Enter RENDER mode to render the animation.");

		ImGui::End();
	}

} // namespace editor
