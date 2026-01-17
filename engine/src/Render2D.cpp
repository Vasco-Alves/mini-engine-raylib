#include "Render2D.hpp"
#include "Assets.hpp"
#include "Entity.hpp"
#include "Components.hpp"
#include "DebugDraw.hpp"
#include "AssetsInternal.hpp"
#include "ComponentsInternal.hpp"
#include "Registry.hpp"

#include <raylib.h>

#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>

namespace {

	me::EntityId s_ActiveCamera = 0;
	bool s_CameraBegun = false;

	inline ::Color ToRay(me::Color c) { return ::Color{ c.r, c.g, c.b, c.a }; }

} // anonymous namespace

namespace me::render2d {

	// ------------------------------------------------------------
	// Camera control
	// ------------------------------------------------------------
	void SetActiveCamera(me::EntityId e) {
		s_ActiveCamera = e;

		// Keep Camera2D::active flag in sync (optional but nice)
		std::vector<me::EntityId> cams;
		me::detail::ForEachCamera(
			[&](me::EntityId camE, const me::components::Camera2D&) {
				cams.push_back(camE);
			});

		for (me::EntityId camE : cams) {
			me::components::Camera2D cam{};
			if (!me::GetComponent(camE, cam)) continue;
			cam.active = (camE == e);
			me::AddComponent(camE, cam);
		}
	}

	void BeginCamera() {
		if (s_CameraBegun) return; // already begun this frame

		if (s_ActiveCamera != 0) {
			me::components::Camera2D cam{};
			if (me::GetComponent(s_ActiveCamera, cam)) {
				::Camera2D rc{};
				rc.target = { cam.x, cam.y }; // world center to look at
				rc.offset = { (float)GetScreenWidth() * 0.5f,
							  (float)GetScreenHeight() * 0.5f };
				rc.rotation = cam.rotation;
				rc.zoom = cam.zoom <= 0.0f ? 1.0f : cam.zoom;

				BeginMode2D(rc);
				s_CameraBegun = true;
				return;
			}
		}

		// No valid camera: render in screen space.
		s_CameraBegun = false;
	}

	void EndCamera() {
		if (s_CameraBegun) {
			EndMode2D();
			s_CameraBegun = false;
		}
	}

	// ------------------------------------------------------------
	// Immediate sprite draw (ad-hoc / debug)
	// ------------------------------------------------------------
	void DrawSprite(const SpriteDesc& s) {
		const ::Texture2D* tex = me::assets::ME_InternalGetTexture(s.tex);
		if (!tex) return;

		::Rectangle src{ 0, 0, (float)tex->width, (float)tex->height };
		if (s.flipX) src.width = -src.width;
		if (s.flipY) src.height = -src.height;

		const float frameW = std::fabs(src.width) * s.scaleX;
		const float frameH = std::fabs(src.height) * s.scaleY;

		// Treat s.x / s.y as the **center** in world space.
		// For raylib, dst.x/dst.y are top-left, so we subtract half-size.
		::Rectangle dst{
			s.x - frameW * 0.5f,
			s.y - frameH * 0.5f,
			frameW,
			frameH
		};

		// Pivot at the visual center of the quad
		::Vector2 origin{ frameW * 0.5f, frameH * 0.5f };

		DrawTexturePro(*tex, src, dst, origin, s.rotation, ToRay(s.tint));
	}

	// ------------------------------------------------------------
// World render pass (SpriteRenderer + Transform2D)
// Assumes camera (if any) is already begun with BeginCamera().
// ------------------------------------------------------------
	struct DrawItem {
		int                            layer;
		me::EntityId                   e;
		me::components::SpriteRenderer sr;
		me::components::Transform2D    tr;
	};

	void RenderWorld() {
		// Gather items to draw
		std::vector<DrawItem> items;
		items.reserve(128);

		me::detail::ForEachSprite(
			[&](me::EntityId e, const me::components::SpriteRenderer& sr) {
				me::components::Transform2D t{};
				if (me::GetComponent(e, t)) {
					items.push_back(DrawItem{ sr.layer, e, sr, t });
				}
			});

		std::stable_sort(items.begin(), items.end(),
			[](const DrawItem& a, const DrawItem& b) { return a.layer < b.layer; });

		// Draw them
		for (const auto& it : items) {
			const ::Texture2D* tex = me::assets::ME_InternalGetTexture(it.sr.tex);
			if (!tex) continue;

			// --- Source rectangle, including SpriteSheet support ---
			::Rectangle src{ 0, 0, (float)tex->width, (float)tex->height };

			me::components::SpriteSheet ss{};
			if (me::GetComponent(it.e, ss) && ss.frameW > 0 && ss.frameH > 0) {
				int cols = ss.cols;
				if (cols <= 0) {
					const int step = ss.frameW + ss.spacing;
					cols = step > 0 ? (tex->width - ss.margin + ss.spacing) / step : 0;
					if (cols <= 0) cols = 1;
				}

				int frameIndex = ss.startIndex;
				me::components::AnimationPlayer ap{};
				if (me::GetComponent(it.e, ap)) frameIndex = ap.current;

				const int fx = frameIndex % cols;
				const int fy = frameIndex / cols;

				const int x = ss.margin + fx * (ss.frameW + ss.spacing);
				const int y = ss.margin + fy * (ss.frameH + ss.spacing);

				src.x = (float)x;
				src.y = (float)y;
				src.width = (float)ss.frameW;
				src.height = (float)ss.frameH;
			}

			if (it.sr.flipX) src.width = -src.width;
			if (it.sr.flipY) src.height = -src.height;

			const float frameW = std::fabs(src.width) * it.tr.sx;
			const float frameH = std::fabs(src.height) * it.tr.sy;

			// Transform2D.x/y = sprite center in world-space
			::Rectangle dst{
				it.tr.x,
				it.tr.y,
				frameW,
				frameH
			};

			// Pivot at visual center (we ignore sr.originX/Y now to keep it simple)
			::Vector2 origin{ frameW * 0.5f, frameH * 0.5f };
			::Color   tint{ it.sr.tint.r, it.sr.tint.g, it.sr.tint.b, it.sr.tint.a };

			DrawTexturePro(*tex, src, dst, origin, it.tr.rotation, tint);
		}
	}


	void RenderWorldWithCamera(me::EntityId camera, int x, int y, int w, int h) {
		struct DrawItemLocal {
			int                            layer;
			me::EntityId                   e;
			me::components::SpriteRenderer sr;
			me::components::Transform2D    tr;
		};

		std::vector<DrawItemLocal> items;
		items.reserve(128);

		me::detail::ForEachSprite([&](me::EntityId e, const me::components::SpriteRenderer& sr) {
			me::components::Transform2D tr{};
			if (me::GetComponent(e, tr)) {
				items.push_back(DrawItemLocal{ sr.layer, e, sr, tr });
			}
			});

		std::stable_sort(
			items.begin(), items.end(),
			[](const DrawItemLocal& a, const DrawItemLocal& b) { return a.layer < b.layer; });

		BeginScissorMode(x, y, w, h);

		bool cameraBegun = false;
		if (camera != 0) {
			me::components::Camera2D cam{};
			if (me::GetComponent(camera, cam)) {
				::Camera2D rc{};
				rc.target = { cam.x, cam.y };
				rc.offset = { (float)x + (float)w * 0.5f,
							  (float)y + (float)h * 0.5f };
				rc.rotation = cam.rotation;
				rc.zoom = cam.zoom <= 0.0f ? 1.0f : cam.zoom;

				BeginMode2D(rc);
				cameraBegun = true;
			}
		}

		for (const auto& it : items) {
			const ::Texture2D* tex = me::assets::ME_InternalGetTexture(it.sr.tex);
			if (!tex) continue;

			::Rectangle src{ 0, 0, (float)tex->width, (float)tex->height };
			if (it.sr.flipX) src.width = -src.width;
			if (it.sr.flipY) src.height = -src.height;

			const float frameW = std::fabs(src.width) * it.tr.sx;
			const float frameH = std::fabs(src.height) * it.tr.sy;

			::Rectangle dst{
				it.tr.x - frameW * 0.5f,
				it.tr.y - frameH * 0.5f,
				frameW,
				frameH
			};

			::Vector2 origin{ frameW * 0.5f, frameH * 0.5f };
			::Color   tint{ it.sr.tint.r, it.sr.tint.g, it.sr.tint.b, it.sr.tint.a };

			DrawTexturePro(*tex, src, dst, origin, it.tr.rotation, tint);
		}

		if (cameraBegun) EndMode2D();
		EndScissorMode();
	}

	void ClearWorld(const me::Color& clearColor) {
		ClearBackground(ToRay(clearColor));
	}

} // namespace me::render2d
