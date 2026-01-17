#pragma once

#include "GameObject.hpp"
#include "Assets.hpp"
#include "Components.hpp"
#include "Input.hpp"
#include "Math.hpp"

#include <cmath>

namespace driftspace {

	struct PlayerConfig {
		float turnSpeedDeg = 180.0f;  // degrees per second
		float thrustAcceleration = 800.0f;  // units per second^2
		float brakeStrength = 4.0f;    // how quickly it slows when braking
		float maxSpeed = 900.0f;  // units per second
	};

	class Player : public GameObject {
	public:
		Player() = default;

		explicit Player(const PlayerConfig& cfg) : config(cfg) {}

		void OnCreate() override {
			entity = me::Entity::Create("Player");

			// -- Transform  --
			me::components::Transform2D t{};
			t.x = 300.0f;
			t.y = 0.0f;
			t.sx = 1.0f;
			t.sy = 1.0f;
			t.rotation = 90.0f;
			entity.Add(t);

			// -- SpriteRenderer --
			me::components::SpriteRenderer sr{};
			sr.tex = me::assets::LoadTexture("player_ship.png");
			sr.layer = 1;
			entity.Add(sr);

			// -- Collider --
			me::components::AabbCollider c{};
			c.w = 32.0f;
			c.h = 32.0f;
			c.ox = 0.0f; // center at t.x
			c.oy = 0.0f; // center at t.y
			c.solid = true;
			entity.Add(c);

			// -- Velocity2D --
			me::components::Velocity2D v{};
			v.vx = 0.0f;
			v.vy = 0.0f;
			v.mass = 50.0f;
			entity.Add(v);
		}

		void OnUpdate(float dt) override {
			me::components::Transform2D t{};
			me::components::Velocity2D v{};

			if (!entity.Get(t) || !entity.Get(v))
				return;

			// -- Parameters --
			const float turnSpeedDeg = config.turnSpeedDeg;
			const float thrustAcceleration = config.thrustAcceleration;
			const float brakeStrength = config.brakeStrength;
			const float maxSpeed = config.maxSpeed;

			// -- Inputs --
			float turnInput = me::input::AxisValue("MoveX");   // A/D or Left/Right
			float thrustAxis = me::input::AxisValue("MoveY");   // W/S or Up/Down
			float thrustInput = -thrustAxis; // In InputDefaults, MoveY is negative for W/Up -> make forward positive.

			// -- Rotate ship --
			t.rotation += turnInput * turnSpeedDeg * dt;

			// Forward vector (texture points up at rotation = 0)
			const float rad = t.rotation * me::math::Pi / 180.0f;
			const float fx = std::sin(rad);
			const float fy = -std::cos(rad);

			// -- Apply thrust along forward/back --
			if (std::fabs(thrustInput) > 0.0001f) {
				v.vx += fx * thrustAcceleration * thrustInput * dt;
				v.vy += fy * thrustAcceleration * thrustInput * dt;
			}

			// -- Brake --
			if (me::input::ActionDown("Brake")) {
				float factor = std::max(0.0f, 1.0f - brakeStrength * dt);
				v.vx *= factor;
				v.vy *= factor;

				const float stopThreshold = 1.0f;
				if (std::fabs(v.vx) < stopThreshold) v.vx = 0.0f;
				if (std::fabs(v.vy) < stopThreshold) v.vy = 0.0f;
			}

			// -- Clamp top speed --
			const float speedSq = v.vx * v.vx + v.vy * v.vy;
			const float maxSpeedSq = maxSpeed * maxSpeed;
			if (speedSq > maxSpeedSq) {
				float s = maxSpeed / std::sqrt(speedSq);
				v.vx *= s;
				v.vy *= s;
			}

			// Write back
			entity.Add(t);
			entity.Add(v);
		}

		void SetConfig(const PlayerConfig& cfg) {
			config = cfg;
		}

	private:
		PlayerConfig config{};
	};
} // namespace driftspace

