#include "editor/panels/viewport_panel.hpp"
#include <imgui.h>
#include <rlImGui.h>
#include <ImGuizmo.h>
#include <raymath.h>
#include "editor/core/editor_app.hpp"
#include <mini-engine-raylib/core/events.hpp>
#include <mini-engine-raylib/core/engine.hpp>

namespace editor {

	void ViewportPanel::on_start() {
		m_Texture = LoadRenderTexture((int)m_Bounds.x, (int)m_Bounds.y);
	}

	void ViewportPanel::on_shutdown() const {
		UnloadRenderTexture(m_Texture);
	}

	void ViewportPanel::begin_render() {
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

	void ViewportPanel::on_imgui_render(me::components::TransformComponent& cam_transform, me::components::CameraComponent& camera, me::Entity selected_entity, int gizmo_type, editor::CommandHistory& command_history, Texture2D* raytraced_texture, bool is_render_mode) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Scene View");

		m_IsFocused = ImGui::IsWindowFocused();
		m_IsHovered = ImGui::IsWindowHovered();

		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		m_Bounds = { viewportPanelSize.x, viewportPanelSize.y };
		ImVec2 viewportPos = ImGui::GetCursorScreenPos();

		// ==========================================
		// RENDER OUTPUT TOGGLE
		// ==========================================
		if (is_render_mode && raytraced_texture && raytraced_texture->id != 0) {
			// Draw the Raytracer texture, stretched to perfectly fit the viewport bounds
			rlImGuiImageSize(raytraced_texture, (int)m_Bounds.x, (int)m_Bounds.y);
		} else {
			// Standard OpenGL Real-Time Viewport
			rlImGuiImageRenderTexture(&m_Texture);
		}

		// ==========================================
		// DISABLE EDITOR TOOLS DURING RENDER
		// ==========================================
		if (!is_render_mode) {

			// ==========================================
			// IMGUIZMO INTEGRATION
			// ==========================================
			if (selected_entity.is_valid() && gizmo_type != -1) {
				if (auto* t = selected_entity.try_get_component<me::components::TransformComponent>()) {
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

					Matrix worldGL = MatrixTranspose(t->model_matrix);
					float transform_matrix[16];
					for (int i = 0; i < 16; i++) transform_matrix[i] = ((float*)&worldGL)[i];

					bool snap_active = ImGui::GetIO().KeyCtrl;
					float snap_value = (gizmo_type == ImGuizmo::ROTATE) ? 45.0f : 0.5f;
					float snap[3] = { snap_value, snap_value, snap_value };

					// --- UNDO / REDO TRACKING ---
					static bool was_using_gizmo = false;
					static me::components::TransformComponent start_state;

					ImGuizmo::Manipulate((float*)&viewGL, (float*)&projGL, (ImGuizmo::OPERATION)gizmo_type, ImGuizmo::LOCAL, transform_matrix, nullptr, snap_active ? snap : nullptr);

					if (ImGuizmo::IsUsing()) {
						if (!was_using_gizmo) {
							// User JUST clicked the gizmo. Record the state!
							start_state = *t;
							was_using_gizmo = true;
						}

						Matrix modifiedWorldGL;
						for (int i = 0; i < 16; i++) ((float*)&modifiedWorldGL)[i] = transform_matrix[i];
						Matrix modifiedWorld = MatrixTranspose(modifiedWorldGL);

						Matrix finalLocalMat = modifiedWorld;
						if (t->parent != me::entity::null) {
							auto* parent_t = me::get_registry().try_get_component<me::components::TransformComponent>(t->parent);
							if (parent_t) {
								Matrix invParent = MatrixInvert(parent_t->model_matrix);
								finalLocalMat = MatrixMultiply(modifiedWorld, invParent);
							}
						}

						if (gizmo_type == ImGuizmo::TRANSLATE) {
							t->position = { finalLocalMat.m12, finalLocalMat.m13, finalLocalMat.m14 };
						} else if (gizmo_type == ImGuizmo::ROTATE) {
							Vector3 right = { finalLocalMat.m0, finalLocalMat.m4, finalLocalMat.m8 };
							Vector3 up = { finalLocalMat.m1, finalLocalMat.m5, finalLocalMat.m9 };
							Vector3 forward = { finalLocalMat.m2, finalLocalMat.m6, finalLocalMat.m10 };

							float sx = Vector3Length(right);
							float sy = Vector3Length(up);
							float sz = Vector3Length(forward);

							Matrix rotMat = finalLocalMat;
							if (sx > 0) { rotMat.m0 /= sx; rotMat.m4 /= sx; rotMat.m8 /= sx; }
							if (sy > 0) { rotMat.m1 /= sy; rotMat.m5 /= sy; rotMat.m9 /= sy; }
							if (sz > 0) { rotMat.m2 /= sz; rotMat.m6 /= sz; rotMat.m10 /= sz; }

							t->rotation_quat = QuaternionNormalize(QuaternionFromMatrix(rotMat));
							Vector3 euler = QuaternionToEuler(t->rotation_quat);
							t->rotation = { euler.x * RAD2DEG, euler.y * RAD2DEG, euler.z * RAD2DEG };

						} else if (gizmo_type == ImGuizmo::SCALE) {
							Vector3 right = { finalLocalMat.m0, finalLocalMat.m4, finalLocalMat.m8 };
							Vector3 up = { finalLocalMat.m1, finalLocalMat.m5, finalLocalMat.m9 };
							Vector3 forward = { finalLocalMat.m2, finalLocalMat.m6, finalLocalMat.m10 };

							float lx = Vector3Length(right);
							float ly = Vector3Length(up);
							float lz = Vector3Length(forward);

							t->scale = {
								lx > 0.0001f ? lx : t->scale.x,
								ly > 0.0001f ? ly : t->scale.y,
								lz > 0.0001f ? lz : t->scale.z
							};
							t->position = { finalLocalMat.m12, finalLocalMat.m13, finalLocalMat.m14 };
						}
					} else {
						if (was_using_gizmo) {
							// User JUST released the mouse. Push the command!
							was_using_gizmo = false;

							// Create and push the command to history
							auto cmd = std::make_unique<editor::ModifyComponentCommand<me::components::TransformComponent>>(
								selected_entity, start_state, *t
							);
							command_history.AddCommand(std::move(cmd));
						}
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
							Vector3 world_pos = { t->model_matrix.m12, t->model_matrix.m13, t->model_matrix.m14 };

							float world_scale_x = Vector3Length({ t->model_matrix.m0, t->model_matrix.m4, t->model_matrix.m8 });
							float world_scale_y = Vector3Length({ t->model_matrix.m1, t->model_matrix.m5, t->model_matrix.m9 });
							float world_scale_z = Vector3Length({ t->model_matrix.m2, t->model_matrix.m6, t->model_matrix.m10 });

							BoundingBox box = {
								{ world_pos.x - (world_scale_x * 0.75f), world_pos.y - (world_scale_y * 0.75f), world_pos.z - (world_scale_z * 0.75f) },
								{ world_pos.x + (world_scale_x * 0.75f), world_pos.y + (world_scale_y * 0.75f), world_pos.z + (world_scale_z * 0.75f) }
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

		} // <-- End of !is_render_mode block

		ImGui::End();
		ImGui::PopStyleVar();
	}
}