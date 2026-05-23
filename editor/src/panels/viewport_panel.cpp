#include "editor/panels/viewport_panel.hpp"

#include <imgui.h>
#include <rlImGui.h>
#include <ImGuizmo.h>
#include <raymath.h>

#include <mini-engine-raylib/core/events.hpp>
#include <mini-engine-raylib/core/engine.hpp>

namespace editor {

	void ViewportPanel::on_start() {
		m_Texture = LoadRenderTexture((int)m_Bounds.x, (int)m_Bounds.y);
	}

	void ViewportPanel::on_shutdown() {
		UnloadRenderTexture(m_Texture);
	}

	void ViewportPanel::begin_render() {
		// Dynamic Viewport Resizing
		if (m_Texture.texture.width != (int)m_Bounds.x || m_Texture.texture.height != (int)m_Bounds.y) {
			if (m_Bounds.x > 0 && m_Bounds.y > 0) {
				UnloadRenderTexture(m_Texture);
				m_Texture = LoadRenderTexture((int)m_Bounds.x, (int)m_Bounds.y);
			}
		}
		BeginTextureMode(m_Texture);
	}

	void ViewportPanel::end_render() {
		EndTextureMode();
	}

	void ViewportPanel::on_imgui_render(me::components::TransformComponent& cam_transform, me::components::CameraComponent& camera, me::entity::entity_id selected, int gizmo_type) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Scene View");

		m_IsFocused = ImGui::IsWindowFocused() || ImGui::IsWindowHovered();

		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		m_Bounds = { viewportPanelSize.x, viewportPanelSize.y };
		ImVec2 viewportPos = ImGui::GetCursorScreenPos();

		rlImGuiImageRenderTexture(&m_Texture);

		// ==========================================
		// IMGUIZMO INTEGRATION
		// ==========================================
		if (selected != 0xFFFFFFFF && gizmo_type != -1) {
			auto* t = me::get_registry().try_get_component<me::components::TransformComponent>(selected);
			if (t) {
				ImGuizmo::SetOrthographic(false);
				ImGuizmo::SetDrawlist();
				ImGuizmo::SetRect(viewportPos.x, viewportPos.y, m_Bounds.x, m_Bounds.y);

				Matrix view = MatrixLookAt(
					{ cam_transform.position.x, cam_transform.position.y, cam_transform.position.z },
					{ camera.target.x, camera.target.y, camera.target.z },
					{ camera.up.x, camera.up.y, camera.up.z }
				);
				Matrix proj = MatrixPerspective(camera.fov * DEG2RAD, m_Bounds.x / m_Bounds.y, 0.01f, 1000.0f);

				Matrix viewGL = MatrixTranspose(view);
				Matrix projGL = MatrixTranspose(proj);

				float transform_matrix[16];
				float pos[3] = { t->position.x, t->position.y, t->position.z };
				float rot[3] = { t->rotation.x, t->rotation.y, t->rotation.z };
				float sca[3] = { t->scale.x, t->scale.y, t->scale.z };
				ImGuizmo::RecomposeMatrixFromComponents(pos, rot, sca, transform_matrix);

				bool snap_active = ImGui::GetIO().KeyCtrl;
				float snap_value = (gizmo_type == ImGuizmo::ROTATE) ? 45.0f : 0.5f;
				float snap[3] = { snap_value, snap_value, snap_value };

				ImGuizmo::Manipulate((float*)&viewGL, (float*)&projGL, (ImGuizmo::OPERATION)gizmo_type, ImGuizmo::LOCAL, transform_matrix, nullptr, snap_active ? snap : nullptr);

				if (ImGuizmo::IsUsing()) {
					float new_pos[3], new_rot[3], new_sca[3];
					ImGuizmo::DecomposeMatrixToComponents(transform_matrix, new_pos, new_rot, new_sca);
					t->position = { new_pos[0], new_pos[1], new_pos[2] };
					t->rotation = { new_rot[0], new_rot[1], new_rot[2] };
					t->scale = { new_sca[0], new_sca[1], new_sca[2] };
				}
			}
		}

		// ==========================================
		// 3D MOUSE PICKING (RAYCAST)
		// ==========================================
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !ImGuizmo::IsOver()) {
			ImVec2 mouse_pos = ImGui::GetMousePos();
			Vector2 rel_mouse = { mouse_pos.x - viewportPos.x, mouse_pos.y - viewportPos.y };

			if (rel_mouse.x >= 0 && rel_mouse.x <= m_Bounds.x && rel_mouse.y >= 0 && rel_mouse.y <= m_Bounds.y) {
				float nx = (2.0f * rel_mouse.x) / m_Bounds.x - 1.0f;
				float ny = 1.0f - (2.0f * rel_mouse.y) / m_Bounds.y;

				Matrix view = MatrixLookAt(
					{ cam_transform.position.x, cam_transform.position.y, cam_transform.position.z },
					{ camera.target.x, camera.target.y, camera.target.z },
					{ camera.up.x, camera.up.y, camera.up.z }
				);
				Matrix proj = MatrixPerspective(camera.fov * DEG2RAD, m_Bounds.x / m_Bounds.y, 0.01f, 1000.0f);
				Matrix viewProjInv = MatrixInvert(MatrixMultiply(view, proj));

				float wx = viewProjInv.m0 * nx + viewProjInv.m4 * ny + viewProjInv.m8 * 1.0f + viewProjInv.m12;
				float wy = viewProjInv.m1 * nx + viewProjInv.m5 * ny + viewProjInv.m9 * 1.0f + viewProjInv.m13;
				float wz = viewProjInv.m2 * nx + viewProjInv.m6 * ny + viewProjInv.m10 * 1.0f + viewProjInv.m14;
				float ww = viewProjInv.m3 * nx + viewProjInv.m7 * ny + viewProjInv.m11 * 1.0f + viewProjInv.m15;

				Ray ray;
				ray.position = { cam_transform.position.x, cam_transform.position.y, cam_transform.position.z };
				ray.direction = Vector3Normalize(Vector3Subtract({ wx / ww, wy / ww, wz / ww }, ray.position));

				float closest_dist = FLT_MAX;
				me::entity::entity_id hit_entity = 0xFFFFFFFF;
				auto& reg = me::get_registry();
				auto view_pool = reg.view<me::components::TransformComponent>();

				for (size_t i = 0; i < view_pool.size(); ++i) {
					auto e = view_pool.entity_map[i];
					auto* t = reg.try_get_component<me::components::TransformComponent>(e);

					bool is_clickable = reg.try_get_component<me::components::Shape3DComponent>(e) ||
						reg.try_get_component<me::components::Model3DComponent>(e) ||
						reg.try_get_component<me::components::LightComponent>(e) ||
						reg.try_get_component<me::components::DirectionalLightComponent>(e);

					if (t && is_clickable) {
						BoundingBox box = {
							{ t->position.x - (t->scale.x * 0.75f), t->position.y - (t->scale.y * 0.75f), t->position.z - (t->scale.z * 0.75f) },
							{ t->position.x + (t->scale.x * 0.75f), t->position.y + (t->scale.y * 0.75f), t->position.z + (t->scale.z * 0.75f) }
						};
						RayCollision col = GetRayCollisionBox(ray, box);
						if (col.hit && col.distance < closest_dist) {
							closest_dist = col.distance;
							hit_entity = e;
						}
					}
				}
				me::get_event_bus().publish<me::events::EntitySelectedEvent>(hit_entity);
			}
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}
}
