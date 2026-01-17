// ResourcePickup.hpp
#pragma once

#include "GameObject.hpp"
#include "Entity.hpp"
#include "Components.hpp"
#include "Assets.hpp"
#include "../Resource.hpp"

#include <cmath>

class ResourcePickup : public GameObject {
public:
	driftspace::ResourceType type = driftspace::ResourceType::BasaltOre;
	int   amount = 1;

	float lifetime = 10.0f; // seconds before despawn
	float age = 0.0f;
	bool  alive = false;

	void OnCreate() override {}

	void OnUpdate(float dt) override {
		if (!alive) return;
		age += dt;
		if (age >= lifetime) {
			Kill();
		}
	}

	void Spawn(driftspace::ResourceType t, int amt,
		float x, float y,
		float vx, float vy) {
		type = t;
		amount = amt;
		lifetime = 10.0f;
		age = 0.0f;
		alive = true;

		entity = me::Entity::Create("ResourcePickup");

		// Transform
		me::components::Transform2D tr{};
		tr.x = x;
		tr.y = y;
		tr.sx = 0.2f;
		tr.sy = 0.2f;
		entity.Add(tr);

		// Sprite – per-type texture (placeholder)
		me::components::SpriteRenderer sr{};
		sr.layer = 1;

		// You can switch by type later
		me::assets::TextureId tex = me::assets::LoadTexture("resources/basalt.png");
		sr.tex = tex;

		me::math::Vec2 sz = me::assets::TextureSize(tex);
		sr.originX = sz.x * 0.5f;
		sr.originY = sz.y * 0.5f;
		entity.Add(sr);

		// Collider (small circle-ish AABB)
		me::components::AabbCollider c{};
		c.w = sz.x;
		c.h = sz.y;
		c.ox = 0.0f;
		c.oy = 0.0f;
		c.solid = false; // we’ll do pickup checks in game code
		entity.Add(c);

		// Velocity
		me::components::Velocity2D v{};
		v.vx = vx;
		v.vy = vy;
		v.mass = 0.0f;
		entity.Add(v);
	}

	void Kill() {
		if (!alive) return;
		alive = false;
		if (entity.IsValid())
			entity.Destroy();
	}

	bool IsDead() const { return !alive; }
};
