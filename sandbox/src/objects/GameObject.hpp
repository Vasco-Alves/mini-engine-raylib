#pragma once

#include "Entity.hpp"

class GameObject {
public:
	virtual ~GameObject() = default;

	virtual void OnCreate() = 0; // Called once to create all components

	virtual void OnUpdate(float dt) {} // Optional per-frame update

	me::Entity GetEntity() const { return entity; }

protected:
	me::Entity entity;
};
