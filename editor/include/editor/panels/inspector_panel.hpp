#pragma once
#include <mini-ecs/registry.hpp>

namespace editor {

	class InspectorPanel {
	public:
		InspectorPanel() = default;
		void on_imgui_render(me::Registry* context, me::entity::entity_id selected_entity);

	private:
		void draw_tag(me::Registry* context, me::entity::entity_id entity);
		void draw_transform(me::Registry* context, me::entity::entity_id entity);
		void draw_shape3d(me::Registry* context, me::entity::entity_id entity);
		void draw_model3d(me::Registry* context, me::entity::entity_id entity);
		void draw_light(me::Registry* context, me::entity::entity_id entity);
		void draw_directional_light(me::Registry* context, me::entity::entity_id entity);
		void draw_rigidbody(me::Registry* context, me::entity::entity_id entity);
		void draw_box_collider(me::Registry* context, me::entity::entity_id entity);
		void draw_sphere_collider(me::Registry* context, me::entity::entity_id entity);
		void draw_audio_source(me::Registry* context, me::entity::entity_id entity);
		void draw_audio_listener(me::Registry* context, me::entity::entity_id entity);
		void draw_background_music(me::Registry* context, me::entity::entity_id entity);
		void draw_script(me::Registry* context, me::entity::entity_id entity);
		void draw_add_component_menu(me::Registry* context, me::entity::entity_id entity);
	};

}