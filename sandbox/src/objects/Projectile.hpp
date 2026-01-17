#pragma once

#include "Entity.hpp"
#include "Components.hpp"
#include "Assets.hpp"
#include "GameObject.hpp"

#include <cmath>
#include <string>

// Generic config for any projectile type
struct ProjectileConfig {
	float speed = 1400.0f;  // units per second
	float lifetime = 1.0f;     // seconds before auto-despawn
	float damage = 1.0f;     // damage units (your game decides what that means)

	// Collider size (simple box)
	float width = 16.0f;
	float height = 16.0f;

	// How much to rotate the sprite relative to "forward"
	float rotationOffsetDeg = 0.0f;
};

class Projectile : public GameObject {
public:
	Projectile() = default;

	explicit Projectile(const ProjectileConfig& cfg) : config(cfg) {}

	void SetConfig(const ProjectileConfig& cfg) { config = cfg; }

	bool IsAlive() const { return alive; }

	float GetDamage() const { return config.damage; }

	// Who fired this (to avoid self-collisions later if needed)
	void SetOwner(const me::Entity& e) { owner = e; }

	me::Entity GetOwner() const { return owner; }

	// Base GameObject requirement (not used directly)
	void OnCreate() override {}

	// Spawn & fire the projectile.
	//
	//  x, y   : world-space spawn position (CENTER of projectile)
	//  dirX,Y : direction in world-space; will be normalized internally
	//  name   : optional entity name (for debugging)
	void Fire(float x, float y, float dirX, float dirY, const char* name = "Projectile") {
		if (std::fabs(dirX) < 0.0001f && std::fabs(dirY) < 0.0001f) {
			// Default to "up" if direction is zero
			dirX = 0.0f;
			dirY = -1.0f;
		}

		// Normalize direction
		float len = std::sqrt(dirX * dirX + dirY * dirY);
		dirX /= len;
		dirY /= len;
		this->dirX = dirX;
		this->dirY = dirY;

		// Create entity
		entity = me::Entity::Create(name);

		// --- Transform ---
		me::components::Transform2D t{};
		t.x = x;
		t.y = y;

		// Solve rad from dir: dirX = sin(rad), dirY = -cos(rad) => rad = atan2(dirX, -dirY)
		float rad = std::atan2(dirX, -dirY);
		float baseDeg = rad * 180.0f / me::math::Pi;

		// Apply projectile-specific sprite offset
		t.rotation = baseDeg + config.rotationOffsetDeg;

		entity.Add(t);

		// --- Sprite ---
		me::components::SpriteRenderer s{};
		s.tex = GetTexture();
		s.tint = me::Color::White;
		s.layer = 2;         // in front of asteroids (you can tweak)
		s.originX = 0.5f;    // center-based
		s.originY = 0.5f;
		entity.Add(s);

		// --- Velocity ---
		me::components::Velocity2D v{};
		v.vx = dirX * config.speed;
		v.vy = dirY * config.speed;
		v.mass = 0.1f; // projectiles are tiny, let engine auto mass or treat as light
		entity.Add(v);

		// --- Collider (non-solid "trigger") ---
		me::components::AabbCollider col{};
		col.w = config.width;
		col.h = config.height;
		col.ox = -config.width * 0.5f; // center collider around Transform2D.x/y
		col.oy = -config.height * 0.5f;
		col.solid = false; // IMPORTANT: do not push things, but still detectable
		SetupCollider(col);
		entity.Add(col);

		age = 0.0f;
		alive = true;
	}

	// Generic movement + lifetime
	void OnUpdate(float dt) override {
		if (!alive) return;

		age += dt;
		if (age >= config.lifetime) {
			Kill();
			return;
		}

		// Movement is handled by physics via Velocity2D,
		// so we generally don't need to manually move here.
		// You can add trail / visual effects here later.

		// If you want projectiles that don't use physics, you could
		// directly update Transform2D here using dirX/dirY.
	}

	// To be called when a collision is detected by the scene / physics system
	virtual void OnHitEntity(const me::Entity& other) {
		// Default behavior: just disappear
		Kill();
	}

	void Kill() {
		if (!alive) return;
		alive = false;
		if (entity.IsValid())
			entity.Destroy();
	}

protected:
	// Derived classes must provide a texture
	virtual me::assets::TextureId GetTexture() const = 0;

	// Derived classes can tweak collider size/flags if needed.
	// Default implementation does nothing (uses ProjectileConfig values).
	virtual void SetupCollider(me::components::AabbCollider& /*col*/) const {}

protected:
	ProjectileConfig config{};
	bool   alive = false;
	float  age = 0.0f;

	// Normalized direction
	float dirX = 0.0f;
	float dirY = -1.0f;

	// Who fired the projectile (optional use)
	me::Entity owner{};
};
