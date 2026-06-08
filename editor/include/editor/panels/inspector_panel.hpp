#pragma once

#include <mini-ecs/entity.hpp>
#include "editor/core/icommands.hpp"

namespace editor {

	class InspectorPanel {
	public:
		InspectorPanel() = default;
		void on_imgui_render(me::Entity selected_entity, editor::CommandHistory& command_history);

	private:
		void draw_tag(me::Entity entity, editor::CommandHistory& command_history);
		void draw_transform(me::Entity entity, editor::CommandHistory& command_history);
		void draw_shape3d(me::Entity entity, editor::CommandHistory& command_history);
		void draw_model3d(me::Entity entity, editor::CommandHistory& command_history);
		void draw_light(me::Entity entity, editor::CommandHistory& command_history);
		void draw_directional_light(me::Entity entity, editor::CommandHistory& command_history);
		void draw_rigidbody(me::Entity entity, editor::CommandHistory& command_history);
		void draw_box_collider(me::Entity entity, editor::CommandHistory& command_history);
		void draw_sphere_collider(me::Entity entity, editor::CommandHistory& command_history);
		void draw_audio_source(me::Entity entity, editor::CommandHistory& command_history);
		void draw_audio_listener(me::Entity entity, editor::CommandHistory& command_history);
		void draw_background_music(me::Entity entity, editor::CommandHistory& command_history);
		void draw_material(me::Entity entity, editor::CommandHistory& command_history);

		// Scripts and structural changes use custom or immediate logic, so they don't take the history object here yet
		void draw_script(me::Entity entity);
		void draw_add_component_menu(me::Entity entity);
	};

} // namespace editor
