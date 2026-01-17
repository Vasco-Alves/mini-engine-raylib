#pragma once

#include "Asteroid.hpp"

class AsteroidBasalt : public Asteroid {
public:
	AsteroidBasalt() {
		resourceType = driftspace::ResourceType::BasaltOre;
		density = 1.0f;
		baseYield = 3;
	}

	const char* GetTypeName() const override { return "AsteroidBasalt"; }

	void OnCreate(float x, float y, float scaleIn, int index = -1) {
		me::assets::TextureId tex = me::assets::LoadTexture("asteroids/asteroid_basalt.png");
		Asteroid::OnCreate(x, y, scaleIn, tex, index);
	}
};
