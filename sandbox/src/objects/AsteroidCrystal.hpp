#pragma once

#include "Asteroid.hpp"

class AsteroidCrystal : public Asteroid {
public:
	AsteroidCrystal() {
		resourceType = driftspace::ResourceType::CrystalShard;
		density = 0.6f;   // lighter, fragile
		baseYield = 6;    // yields more per hit
	}
	const char* GetTypeName() const override { return "AsteroidCrystal"; }

	void OnCreate(float x, float y, float scaleIn, int index = -1) {
		me::assets::TextureId tex =
			me::assets::LoadTexture("asteroids/asteroid_crystal.png");
		Asteroid::OnCreate(x, y, scaleIn, tex, index);
	}
};
