#pragma once

#include "Asteroid.hpp"

class AsteroidIron : public Asteroid {
public:
	AsteroidIron() {
		resourceType = driftspace::ResourceType::IronOre;
		density = 1.8f;    // heavier than basalt
		baseYield = 4;
	}
	const char* GetTypeName() const override { return "AsteroidIron"; }

	void OnCreate(float x, float y, float scaleIn, int index = -1) {
		me::assets::TextureId tex =
			me::assets::LoadTexture("asteroids/asteroid_iron.png");
		Asteroid::OnCreate(x, y, scaleIn, tex, index);
	}
};
