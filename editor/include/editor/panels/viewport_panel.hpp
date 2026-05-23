#pragma once

#include <raylib.h>
#include <mini-ecs/registry.hpp>
#include <mini-engine-raylib/ecs/components.hpp>

namespace editor {

	class ViewportPanel {
	public:
		void on_start();
		void on_shutdown();

		// Wraps the Raylib texture rendering phase
		void begin_render();
		void end_render();

		// Draws the ImGui window and handles 3D math
		void on_imgui_render(me::components::TransformComponent& cam_transform, me::components::CameraComponent& camera, me::entity::entity_id selected_entity, int gizmo_type);

		bool is_focused() const { return m_IsFocused; }
		Vector2 get_bounds() const { return m_Bounds; }

	private:
		RenderTexture2D m_Texture;
		Vector2 m_Bounds = { 1920.0f, 1080.0f };
		bool m_IsFocused = false;
	};

}