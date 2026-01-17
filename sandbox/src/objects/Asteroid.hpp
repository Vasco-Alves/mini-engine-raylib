#pragma once

#include "Assets.hpp"
#include "Components.hpp"
#include "GameObject.hpp"
#include "../Resource.hpp"

#include <string>
#include <algorithm>
#include <cstdlib>
#include <cmath>

class Asteroid : public GameObject {
protected:
	// Visual / motion
	float scale = 1.0f;
	float rotationSpeed = 0.0f;           // deg/sec, random per asteroid
	me::assets::TextureId texture{};

	// Material / physics
	float density = 1.0f;   // relative density (Basalt = 1 by default)
	float baseMass = 1000.0f; // mass at scale=1, density=1
	int   baseYield = 3;      // base resource units at scale=1, density=1

	// HP
	float maxHp = 20.0f;
	float hp = 20.0f;
	bool  alive = true;

	// What this asteroid drops
	driftspace::ResourceType resourceType = driftspace::ResourceType::BasaltOre;

	static float Rand01() {
		return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	}

public:
	virtual ~Asteroid() = default;
	virtual const char* GetTypeName() const { return "Asteroid"; }

	// ---- Query helpers ----
	driftspace::ResourceType GetResourceType() const { return resourceType; }
	float GetDensity() const { return density; }
	float GetHp() const { return hp; }
	bool  IsDead() const { return !alive; }

	// Total yield scales with density & size
	int GetResourceYield() const {
		float scaled = static_cast<float>(baseYield) * density * (scale * scale);
		int y = static_cast<int>(scaled + 0.5f);
		return (y < 1) ? 1 : y;
	}

	// Returns true if asteroid entity no longer exists
	bool IsDestroyed() const {
		return !alive || !entity.IsValid();
	}

	// Apply mining or impact damage.
	// Returns true if asteroid was destroyed by this hit.
	// Outputs number of resources dropped immediately and on final destruction.
	bool ApplyDamage(float amount, int& dropsOnHit, int& dropsOnDeath) {
		dropsOnHit = 0;
		dropsOnDeath = 0;
		if (!alive) return false;

		hp -= amount;
		if (hp <= 0.0f) {
			hp = 0.0f;
			alive = false;

			// Guaranteed batch of resources on death, based on density & scale.
			dropsOnDeath = GetResourceYield();

			// Destroy entity visually/physically
			if (entity.IsValid())
				entity.Destroy();

			return true;
		} else {
			// Small random chance to eject a single resource on hit
			if (Rand01() < 0.25f)
				dropsOnHit = 1;
		}

		return false;
	}

	// Base GameObject requirement (not used directly)
	void OnCreate() override {}

	// x, y = asteroid CENTER in world space
	// scaleIn = visual scale
	virtual void OnCreate(float x, float y, float scaleIn,
		me::assets::TextureId tex,
		int index = -1) {
		scale = scaleIn;
		texture = tex;

		std::string name = std::string(GetTypeName());
		if (index >= 0)
			name += "_" + std::to_string(index);

		entity = me::Entity::Create(name.c_str());

		// --- Transform (center-based) ---
		me::components::Transform2D t{};
		t.x = x;
		t.y = y;
		t.sx = scale;
		t.sy = scale;
		entity.Add(t);

		// --- SpriteRenderer ---

		me::math::Vec2 sz = me::assets::TextureSize(tex);
		const float width = sz.x * scale;
		const float height = sz.y * scale;

		me::components::SpriteRenderer sr{};
		sr.tex = tex;
		sr.layer = 0;
		entity.Add(sr);

		// rotate around sprite center
		//sr.originX = width * 0.5f;
		//sr.originY = height * 0.5f;

		// --- Circle Collider (centered on Transform) ---
		me::components::CircleCollider cc{};
		cc.radius = 0.5f * std::min(width, height);
		cc.ox = 0.0f;  // center at t.x
		cc.oy = 0.0f;  // center at t.y
		cc.solid = true;
		entity.Add(cc);

		// --- Random spin speed (-10 .. +10 deg/sec) ---
		rotationSpeed = Rand01() * 20.0f - 10.0f;

		// --- Velocity / mass ---
		float angle = Rand01() * 6.2831853f;  // 0..2π
		float speed = 5.0f + Rand01() * 15.0f;

		me::components::Velocity2D v{};
		v.vx = std::cos(angle) * speed;
		v.vy = std::sin(angle) * speed;
		v.mass = baseMass * density * (scale * scale);
		entity.Add(v);

		// --- HP ---
		const float baseHpPerSize = 25.0f;
		maxHp = baseHpPerSize * density * (0.4f + scale);
		hp = maxHp;
	}


	void OnUpdate(float dt) override {
		if (!alive) return;

		me::components::Transform2D t{};
		if (entity.Get(t)) {
			t.rotation += rotationSpeed * dt;
			entity.Add(t);
		}
	}
};
