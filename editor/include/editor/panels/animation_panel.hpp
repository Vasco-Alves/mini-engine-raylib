#pragma once

#include "editor/core/scene_animation.hpp"
#include "editor/core/icommands.hpp"

#include <mini-ecs/entity.hpp>
#include <functional>

namespace editor {

	// Timeline panel: a visual track view of the animation. The ruler scrubs the
	// playhead, keyframes are draggable diamonds (click to select, right-click for
	// options, double-click to jump), keys are captured AT the playhead from the
	// scene's current state, and the selected key can be retimed / value-edited /
	// re-captured / deleted below the timeline. Ctrl+scroll zooms the timeline.
	// All keyframe edits go through the command history (undoable).
	class AnimationPanel {
	public:
		void on_imgui_render(SceneAnimation& anim,
			me::components::TransformComponent& cam_transform,
			me::components::CameraComponent& camera,
			me::systems::RaytracerSystem& raytracer,
			me::Entity selected_entity,
			bool in_render_mode,
			editor::CommandHistory& history,
			const std::function<void()>& request_render);

		// Advances preview playback; call once per frame with dt.
		void update_preview(float dt, SceneAnimation& anim,
			me::components::TransformComponent& cam_transform,
			me::components::CameraComponent& camera,
			me::systems::RaytracerSystem& raytracer,
			bool in_render_mode);

		bool is_previewing() const { return m_Previewing; }
		void stop_preview() { m_Previewing = false; }

	private:
		// Moves the whole animation (camera + environment + entity tracks) to t.
		void apply(SceneAnimation& anim, float t,
			me::components::TransformComponent& cam_transform,
			me::components::CameraComponent& camera,
			me::systems::RaytracerSystem& raytracer,
			bool in_render_mode);

		// --- Playback ---
		float m_Time = 0.0f;       // playhead (seconds)
		bool  m_Previewing = false;
		bool  m_Loop = true;

		// --- Track creation ---
		int m_NewTrackComp = 0;    // combo index for the add-track component

		// --- Timeline view ---
		float m_Zoom = 1.0f;       // 1 = fit; Ctrl+scroll zooms, Fit resets

		// --- Selection (which key is selected) ---
		// m_SelTrack: kSelNone, kSelCamera, kSelEnv, or an entity-track index.
		static constexpr int kSelNone = -2;
		static constexpr int kSelCamera = -1;
		static constexpr int kSelEnv = -3;
		int  m_SelTrack = kSelNone;
		int  m_SelKey = -1;

		// --- Drag state ---
		bool  m_DraggingKey = false;
		bool  m_DraggingPlayhead = false;
		float m_DragStartTime = 0.0f; // key time when the drag began (undo no-op check)

		// --- Undo snapshot for in-progress widget edits (drag fields etc.) ---
		SceneAnimation m_BeforeEdit;
		bool m_BeforeEditValid = false;
	};

} // namespace editor
