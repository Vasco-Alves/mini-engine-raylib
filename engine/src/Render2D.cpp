#include "Render2D.hpp"
#include "Assets.hpp"
#include "Entity.hpp"
#include "Components.hpp"
#include "DebugDraw.hpp"
#include "AssetsInternal.hpp"
#include "Registry.hpp"

#include <raylib.h>
#include <vector>
#include <algorithm>
#include <cmath>

namespace {
	me::EntityId s_ActiveCamera = 0;
	bool s_CameraBegun = false;

	inline ::Color ToRay(me::Color c) { return ::Color{ c.r, c.g, c.b, c.a }; }
}

namespace me::render2d {

	void DrawSprite(const SpriteDesc& s) {}

	void SetActiveCamera(me::EntityId e) {
		s_ActiveCamera = e;
	}

	void BeginCamera() {
		if (s_CameraBegun) return;

		if (s_ActiveCamera != 0) {
			auto& reg = me::detail::Reg();

			// 1. Get Camera Settings (Zoom, Rotation)
			auto* cam = reg.TryGetComponent<me::components::Camera2D>(s_ActiveCamera);

			// 2. Get Camera Position (Transform)
			auto* t = reg.TryGetComponent<me::components::Transform2D>(s_ActiveCamera);

			if (cam && t) {
				::Camera2D rc{};

				rc.target = { t->x, t->y };
				rc.offset = { (float)GetScreenWidth() * 0.5f, (float)GetScreenHeight() * 0.5f };
				rc.rotation = t->rotation;
				rc.zoom = cam->zoom <= 0.0f ? 1.0f : cam->zoom;

				BeginMode2D(rc);
				s_CameraBegun = true;
				return;
			}
		}
		s_CameraBegun = false;
	}

	void EndCamera() {
		if (s_CameraBegun) {
			EndMode2D();
			s_CameraBegun = false;
		}
	}

	// --- World Render ---
	struct DrawItem {
		int layer;
		me::EntityId e;
		const me::components::SpriteRenderer* sr;
		const me::components::Transform2D* tr;
	};

	void RenderWorld() {
		auto& reg = me::detail::Reg();
		auto* spritePool = reg.GetPool<me::components::SpriteRenderer>();

		std::vector<DrawItem> items;
		items.reserve(spritePool->data.size());

		for (auto& kv : spritePool->data) {
			me::EntityId e = kv.first;
			const auto& sr = kv.second;

			// Need Transform
			auto* t = reg.TryGetComponent<me::components::Transform2D>(e);
			if (t) {
				items.push_back({ sr.layer, e, &sr, t });
			}
		}

		std::stable_sort(items.begin(), items.end(),
			[](const DrawItem& a, const DrawItem& b) { return a.layer < b.layer; });

		// Draw
		for (const auto& it : items) {
			const ::Texture2D* tex = me::assets::ME_InternalGetTexture(it.sr->tex);
			if (!tex) continue;

			::Rectangle src{ 0, 0, (float)tex->width, (float)tex->height };

			// SpriteSheet support
			auto* ss = reg.TryGetComponent<me::components::SpriteSheet>(it.e);
			if (ss && ss->frameW > 0 && ss->frameH > 0) {
				int cols = ss->cols;
				if (cols <= 0) {
					const int step = ss->frameW + ss->spacing;
					cols = step > 0 ? (tex->width - ss->margin + ss->spacing) / step : 0;
					if (cols <= 0) cols = 1;
				}

				int frameIndex = ss->startIndex;
				auto* ap = reg.TryGetComponent<me::components::AnimationPlayer>(it.e);
				if (ap) frameIndex = ap->current;

				const int fx = frameIndex % cols;
				const int fy = frameIndex / cols;

				const int x = ss->margin + fx * (ss->frameW + ss->spacing);
				const int y = ss->margin + fy * (ss->frameH + ss->spacing);

				src.x = (float)x;
				src.y = (float)y;
				src.width = (float)ss->frameW;
				src.height = (float)ss->frameH;
			}

			if (it.sr->flipX) src.width = -src.width;
			if (it.sr->flipY) src.height = -src.height;

			const float frameW = std::fabs(src.width) * it.tr->sx;
			const float frameH = std::fabs(src.height) * it.tr->sy;

			::Rectangle dst{ it.tr->x, it.tr->y, frameW, frameH };
			::Vector2 origin{ frameW * 0.5f, frameH * 0.5f };
			::Color   tint{ it.sr->tint.r, it.sr->tint.g, it.sr->tint.b, it.sr->tint.a };

			DrawTexturePro(*tex, src, dst, origin, it.tr->rotation, tint);
		}
	}

	void RenderWorldWithCamera(me::EntityId camera, int x, int y, int w, int h) {
	}

	void ClearWorld(const me::Color& clearColor) {
		ClearBackground(ToRay(clearColor));
	}
}