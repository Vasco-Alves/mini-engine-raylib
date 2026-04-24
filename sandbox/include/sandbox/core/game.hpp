#pragma once

#include <mini-engine-raylib/core/application.hpp>
#include "sandbox/scene/testscene.hpp"

#include <memory>

namespace sandbox {

	class SandboxGame : public me::Application {
	public:
		void on_start(int width, int height) override;
		void on_update(float dt) override;
		void on_render() override;

	private:
		TestScene m_test_scene;
	};

} // namespace sandbox