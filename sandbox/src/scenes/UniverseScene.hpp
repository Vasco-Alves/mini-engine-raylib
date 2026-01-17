#pragma once

#include "GameScene.hpp"
#include "SceneManager.hpp"
#include "Render2D.hpp"
#include "CameraFollow.hpp"
#include "Assets.hpp"
#include "Input.hpp"
#include "Entity.hpp"
#include "Components.hpp"
#include "Math.hpp"

#include "../objects/Player.hpp"
#include "../objects/AsteroidBasalt.hpp"
#include "../objects/LaserProjectile.hpp"
#include "../objects/ResourcePickup.hpp"

#include "../generators/AsteroidGenerator.hpp"

#include "../utils/RNG.hpp"

#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>

// Small helpers
namespace {
	struct AABB {
		float x, y, w, h;
	};

	struct CircleShape {
		float cx, cy, r;
	};

	inline AABB MakeAabb(const me::components::Transform2D& t,
		const me::components::AabbCollider& c) {
		// Center of the collider in world space
		float cx = t.x + c.ox;
		float cy = t.y + c.oy;

		// Convert to top-left for AABB math
		float x = cx - c.w * 0.5f;
		float y = cy - c.h * 0.5f;

		return AABB{ x, y, c.w, c.h };
	}

	inline CircleShape MakeCircle(const me::components::Transform2D& t,
		const me::components::CircleCollider& c) {
		float cx = t.x + c.ox;
		float cy = t.y + c.oy;
		return CircleShape{ cx, cy, c.radius };
	}

	inline bool OverlapCircleAabb(const CircleShape& c, const AABB& b) {
		// Closest point on box to circle center
		float closestX = std::fmax(b.x, std::fmin(c.cx, b.x + b.w));
		float closestY = std::fmax(b.y, std::fmin(c.cy, b.y + b.h));

		float dx = c.cx - closestX;
		float dy = c.cy - closestY;
		float dist2 = dx * dx + dy * dy;
		float r2 = c.r * c.r;

		return dist2 <= r2;
	}

	inline bool OverlapAabbAabbSimple(const AABB& a, const AABB& b) {
		return (a.x < b.x + b.w) &&
			(a.x + a.w > b.x) &&
			(a.y < b.y + b.h) &&
			(a.y + a.h > b.y);
	}
}


class UniverseScene : public me::scene::GameScene {
public:
	UniverseScene() = default;

	const char* GetName() const override { return "Universe"; }
	const char* GetFile() const override { return "universe.json"; }

	int GetBasalt()  const { return invBasalt; }
	int GetIron()    const { return invIron; }
	int GetCrystal() const { return invCrystal; }

	void OnEnter() override {
		std::cout << "Entered Universe\n";

		// -- Camera --
		camera = me::FindEntity("Camera");
		if (!camera.IsValid()) {
			camera = me::Entity::Create("Camera");

			me::components::Transform2D t{};
			t.x = 0.0f; t.y = 0.0f;
			camera.Add(t);

			me::components::Camera2D c{};
			c.x = 0.0f;
			c.y = 0.0f;
			c.zoom = 1.0f;
			c.rotation = 0.0f;
			c.active = true;
			camera.Add(c);
		}
		me::render2d::SetActiveCamera(camera.Id());

		// -- Base --
		base = me::FindEntity("Base");
		if (!base.IsValid()) {
			base = me::Entity::Create("Base");

			me::components::Transform2D t{};
			t.x = 0.0f; t.y = 0.0f;
			t.sx = 0.5f; t.sy = 0.5f;
			base.Add(t);

			auto tex = me::assets::LoadTexture("base_station.png");
			me::components::SpriteRenderer sr{};
			sr.tex = tex;
			sr.layer = 0;
			base.Add(sr);
		}

		// -- Player --
		player.OnCreate();

		// Camera follows player
		if (player.GetEntity().IsValid()) {
			me::camera::FollowParams fp;
			fp.stiffness = 8.0f;
			fp.snapOnAttach = true;
			me::camera::Attach(camera.Id(), player.GetEntity().Id(), fp);
		}

		// -- Asteroids --
		// 1) Configure the generator
		driftspace::gen::AsteroidGeneratorConfig cfg;
		cfg.count = 80;        // how many asteroids
		cfg.innerRadius = 2500.0f;   // min distance from base
		cfg.outerRadius = 9000.0f;   // max distance from base
		cfg.minSeparation = 800.0f;    // minimum distance between asteroids
		cfg.minScale = 0.6f;      // minimum size
		cfg.maxScale = 2.2f;      // maximum size

		driftspace::gen::AsteroidGenerator generator(cfg);

		// 2) Get base position (center of belt)
		me::math::Vec2 basePos{ 0.0f, 0.0f };

		me::components::Transform2D baseTransform{};
		if (base.Get(baseTransform)) {
			basePos.x = baseTransform.x;
			basePos.y = baseTransform.y;
		}

		// 3) Generate asteroids around the base
		asteroids.clear();
		asteroids.reserve(cfg.count);

		// Pass a non-zero seed if you want a deterministic layout per save slot.
		// For now, 0 = random seed.
		generator.Generate(asteroids, basePos, /*seed=*/0);
	}

	void OnUpdate(float dt) override {
		// 1) Update player
		player.OnUpdate(dt);

		// Update cooldown
		if (laserCooldown > 0.0f)
			laserCooldown -= dt;

		// 2) Firing: hold to keep spawning lasers
		if (me::input::ActionDown("Mine") && laserCooldown <= 0.0f) {
			me::components::Transform2D pt{};
			me::components::Velocity2D  pv{};

			if (player.GetEntity().Get(pt) && player.GetEntity().Get(pv)) {
				float rad = pt.rotation * 3.14159265f / 180.0f;
				float fx = std::sin(rad);
				float fy = -std::cos(rad); // forward

				float spawnDist = 40.0f;
				float px = pt.x + fx * spawnDist;
				float py = pt.y + fy * spawnDist;

				LaserProjectile proj;
				proj.SetOwner(player.GetEntity());
				proj.Fire(px, py, fx, fy, "Laser");
				projectiles.push_back(std::move(proj));

				// reset cooldown
				laserCooldown = laserFireInterval;
			}
		}

		// 3) Update projectiles
		for (size_t i = 0; i < projectiles.size(); ) {
			auto& p = projectiles[i];
			if (p.IsAlive()) {
				p.OnUpdate(dt);
				++i;
			} else {
				// Remove dead ones
				projectiles.erase(projectiles.begin() + i);
			}
		}

		// 4) Update asteroids
		for (auto& a : asteroids) a.OnUpdate(dt);

		// Remove dead asteroids
		for (std::size_t i = 0; i < asteroids.size(); ) {
			if (asteroids[i].IsDestroyed()) {
				if (asteroids[i].GetEntity().IsValid())
					asteroids[i].GetEntity().Destroy();
				asteroids.erase(asteroids.begin() + static_cast<long>(i));
			} else {
				++i;
			}
		}

		// 5) Check projectile-asteroid collisions
		for (auto& proj : projectiles) {
			if (!proj.IsAlive())
				continue;

			// Projectile needs Transform2D + AabbCollider
			me::components::Transform2D pt{};
			me::components::AabbCollider pc{};

			if (!proj.GetEntity().Get(pt) || !proj.GetEntity().Get(pc))
				continue;

			AABB projBox = MakeAabb(pt, pc);

			for (auto& ast : asteroids) {
				if (ast.IsDead())
					continue;

				// Asteroids use CircleCollider in Asteroid::OnCreate
				me::components::Transform2D at{};
				me::components::CircleCollider ac{};

				if (!ast.GetEntity().Get(at) || !ast.GetEntity().Get(ac))
					continue;

				CircleShape astCircle = MakeCircle(at, ac);

				if (!OverlapCircleAabb(astCircle, projBox))
					continue;

				// --- We have a hit ---

				int dropsOnHit = 0;
				int dropsOnDeath = 0;
				ast.ApplyDamage(proj.GetDamage(), dropsOnHit, dropsOnDeath);

				// Spawn physical resource pickups instead of raw inventory
				SpawnResourcesFromHit(ast, at, ac, pt, dropsOnHit, dropsOnDeath);

				// Kill projectile so it doesn't multi-hit
				proj.Kill();
				break;
			}
		}

		// 6) Update resource pickups
		for (std::size_t i = 0; i < pickups.size(); ) {
			auto& p = pickups[i];
			if (!p.IsDead()) {
				p.OnUpdate(dt);
				++i;
			} else {
				pickups.erase(pickups.begin() + i);
			}
		}

		// Player collects pickups when touching them
		me::components::Transform2D playerT{};
		me::components::AabbCollider playerC{};
		if (player.GetEntity().Get(playerT) && player.GetEntity().Get(playerC)) {
			AABB playerBox = MakeAabb(playerT, playerC);

			for (std::size_t i = 0; i < pickups.size(); ) {
				auto& p = pickups[i];
				if (p.IsDead()) {
					pickups.erase(pickups.begin() + i);
					continue;
				}

				me::components::Transform2D rt{};
				me::components::AabbCollider rc{};
				if (!p.GetEntity().Get(rt) || !p.GetEntity().Get(rc)) {
					++i;
					continue;
				}

				AABB resBox = MakeAabb(rt, rc);

				if (OverlapAabbAabbSimple(playerBox, resBox)) {
					// Add to inventory based on pickup type/amount
					switch (p.type) {
					case driftspace::ResourceType::BasaltOre:
						invBasalt += p.amount;
						break;
					case driftspace::ResourceType::IronOre:
						invIron += p.amount;
						break;
					case driftspace::ResourceType::CrystalShard:
						invCrystal += p.amount;
						break;
					default:
						break;
					}

					p.Kill();
					pickups.erase(pickups.begin() + i);
				} else {
					++i;
				}
			}
		}

		// 7) Camera update (zooming)
		CameraUpdate(dt);
	}

	void OnExit() override {
		std::cout << "Leaving Universe\n";

		for (auto& a : asteroids) {
			if (a.GetEntity().IsValid())
				a.GetEntity().Destroy();
		}
		asteroids.clear();
	}

private:
	driftspace::Player player;
	std::vector<AsteroidBasalt> asteroids;
	std::vector<LaserProjectile> projectiles;
	std::vector<ResourcePickup> pickups;

	me::Entity base;
	me::Entity camera;

	// Simple inventory
	int invBasalt = 0;
	int invIron = 0;
	int invCrystal = 0;

	float laserCooldown = 0.0f;   // time until next shot allowed
	float laserFireInterval = 1.0f; // seconds between shots while holding "Mine"

	void CameraUpdate(float dt) {
		// Camera zoom
		if (camera.IsValid()) {
			float dz = me::input::AxisValue("ZoomCam");
			if (dz != 0.0f) {
				const float MinZoom = 0.4f;
				const float MaxZoom = 3.0f;

				me::components::Camera2D c{};
				if (camera.Get(c)) {
					c.zoom = std::clamp(c.zoom + dz, MinZoom, MaxZoom);
					camera.Add(c);
				}
			}
		}
	}

	void SpawnResourcesFromHit(
		const Asteroid& asteroid,
		const me::components::Transform2D& at,
		const me::components::CircleCollider& ac,
		const me::components::Transform2D& pt,
		int dropsOnHit,
		int dropsOnDeath) {
		int total = dropsOnHit + dropsOnDeath;
		if (total <= 0)
			return;

		// What type of resource does this asteroid drop?
		const driftspace::ResourceType resType = asteroid.GetResourceType();

		// Direction from asteroid center to impact point (approx hit normal)
		float nx = pt.x - at.x;
		float ny = pt.y - at.y;
		float len2 = nx * nx + ny * ny;
		if (len2 < 0.0001f) {
			// Fallback: random direction
			float angle = driftspace::utils::RandRange(0.0f, 2.0f * me::math::Pi);
			nx = std::cos(angle);
			ny = std::sin(angle);
		} else {
			float invLen = 1.0f / std::sqrt(len2);
			nx *= invLen;
			ny *= invLen;
		}

		for (int i = 0; i < total; ++i) {
			// Slight angular jitter so they don't all fly in exactly the same line
			float jitter = driftspace::utils::RandRange(-0.5f, 0.5f); // ±0.5 rad
			float cs = std::cos(jitter);
			float sn = std::sin(jitter);

			float jx = nx * cs - ny * sn;
			float jy = nx * sn + ny * cs;

			// Spawn point: near surface of asteroid, along normal
			float spawnDist = ac.radius * driftspace::utils::RandRange(0.9f, 1.2f);
			float px = at.x + jx * spawnDist;
			float py = at.y + jy * spawnDist;

			// Ejection speed: small floating chunks
			float speed = driftspace::utils::RandRange(60.0f, 140.0f);
			float vx = jx * speed;
			float vy = jy * speed;

			ResourcePickup pickup;
			// 1 unit per pickup – you can later pack "amount" for big drops
			pickup.Spawn(resType, 1, px, py, vx, vy);
			pickups.push_back(std::move(pickup));
		}
	}

};
