#pragma once

#include "Projectile.hpp"
#include "Assets.hpp"

// Simple laser bolt projectile
class LaserProjectile : public Projectile {
public:
	LaserProjectile() {
		ProjectileConfig cfg;
		cfg.speed = 1600.0f;
		cfg.lifetime = 0.8f;
		cfg.damage = 1.0f;
		cfg.width = 12.0f;
		cfg.height = 32.0f;
		cfg.rotationOffsetDeg = -90.0f; // Sprite is facing right by default
		SetConfig(cfg);

		// Ensure texture is loaded once (lazy static)
		EnsureTextureLoaded();
	}

protected:
	me::assets::TextureId GetTexture() const override {
		return s_texLaser;
	}

	void SetupCollider(me::components::AabbCollider& col) const override {
		// You can narrow the collider a bit if you want
		col.w = config.width * 0.6f;
		col.h = config.height;
		col.ox = -col.w * 0.5f;
		col.oy = -col.h * 0.5f;
		col.solid = false; // still a trigger
	}

private:
	static void EnsureTextureLoaded() {
		if (!me::assets::IsTextureValid(s_texLaser))
			s_texLaser = me::assets::LoadTexture("projectiles/laser.png");
	}

	static me::assets::TextureId s_texLaser;
};

inline me::assets::TextureId LaserProjectile::s_texLaser{};
