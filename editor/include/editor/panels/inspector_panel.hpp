#pragma once

#include <mini-ecs/entity.hpp>
#include "editor/core/icommands.hpp"

namespace editor {

	// Renders the Inspector window for the selected entity. The per-component
	// panels and the Add-Component menu are both driven by a single table in the
	// .cpp (inspector_components()), so a component's editor support lives in one
	// place instead of two hand-maintained lists.
	class InspectorPanel {
	public:
		InspectorPanel() = default;
		void on_imgui_render(me::Entity selected_entity, editor::CommandHistory& command_history);
	};

} // namespace editor
