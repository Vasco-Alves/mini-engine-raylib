#pragma once

#include <mini-engine-raylib/scene/scene.hpp>

namespace sandbox {

	class TestScene : public me::Scene {
	public:
		const char* get_name() const override { return "Level3D"; }

		void on_register() override;
		void on_enter() override;
		void on_exit() override;
		void on_update(float dt) override;
		void on_resize(int width, int height) override;

	};

} // namespace sandbox