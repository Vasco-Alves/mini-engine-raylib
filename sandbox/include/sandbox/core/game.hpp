#pragma once

#include "sandbox/scene/testscene.hpp"

#include <mini-engine-raylib/core/application.hpp>

#include <memory>

namespace sandbox {

	class SandboxGame : public me::Application {
	public:
		void on_start() override;
		void on_resize(int width, int height) override;
		void on_update(float dt) override;
		void on_render() override;
		void on_shutdown() override;

	private:
		TestScene m_test_scene;
	};

} // namespace sandbox