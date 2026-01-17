#pragma once

#include "GameScene.hpp"
#include "SceneManager.hpp"
#include "Input.hpp"

#include <iostream>

class BaseScene : public me::scene::GameScene {
public:
	const char* GetName() const override { return "Base"; }
	const char* GetFile() const override { return "base.json"; }

	void OnEnter() override {
		std::cout << "Entered Base\n";
		// You can find entities from JSON or spawn extra stuff here
	}

	void OnUpdate(float /*dt*/) override {
		if (me::input::ActionPressed("Launch")) {
			me::scene::manager::Load("Space");
		}
	}
};
