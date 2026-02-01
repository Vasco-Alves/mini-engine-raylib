#include "GameApp.hpp"
#include "SceneManager.hpp"
#include "GameScene.hpp"
#include "Components.hpp"
#include "Entity.hpp"
#include "Input.hpp"
#include "Render2D.hpp"
#include "Physics2D.hpp"   
#include "DebugDraw.hpp"   
#include "Color.hpp"
#include "LifetimeSystem.hpp"
#include "Time.hpp"
#include "Registry.hpp" 

#include <iostream>
#include <vector>
#include <cmath>

// --- Custom Game Components ---
struct Health {
	int current = 100;
	int max = 100;
};

struct Projectile {
	int damage = 10;
	me::EntityId owner = 0;
};

// A tag component to mark things that can be hit
struct Hittable {};

// --- Custom Game System ---
void CombatSystem_Update(float dt) {
	auto& reg = me::detail::Reg();

	// 1. Get Physics Collisions from this frame
	const auto& collisions = me::physics::GetCollisions();

	// 2. Iterate collisions to check for Projectile vs Hittable
	for (const auto& col : collisions) {
		me::EntityId a = col.a;
		me::EntityId b = col.b;

		// We don't know if 'a' is the bullet or 'b' is the bullet.
		// Helper lambda to check:
		auto TryHit = [&](me::EntityId bullet, me::EntityId victim) {

			// Check if 'bullet' is actually a projectile
			auto* proj = reg.TryGetComponent<Projectile>(bullet);
			if (!proj)
				return false;

			// Check if 'victim' is hittable
			if (!reg.HasComponent<Hittable>(victim))
				return false;

			// Don't hit itself
			if (proj->owner == victim)
				return false;

			// --- HIT CONFIRMED ---

			// Apply Damage
			if (auto* hp = reg.TryGetComponent<Health>(victim)) {
				hp->current -= proj->damage;
				std::cout << "Hit! HP: " << hp->current << "\n";
				if (hp->current <= 0) {
					me::DestroyEntity(victim);
					std::cout << "Enemy Destroyed!\n";
				}
			}

			// Destroy Bullet
			me::DestroyEntity(bullet);
			return true;
			};

		// Try both combinations: A hits B, or B hits A
		if (TryHit(a, b)) continue;
		TryHit(b, a);
	}
}


// --- A Simple Game Scene ---
class LevelOne : public me::scene::GameScene {
public:
	void OnRegister() override {
		me::input::BindDigitalAxis("Horizontal", me::input::Key::A, me::input::Key::D);
		me::input::BindDigitalAxis("Vertical", me::input::Key::W, me::input::Key::S);
		me::input::BindAction("Shoot", me::input::Key::Space);
	}

	void OnEnter() override {
		std::cout << "Entering Level One...\n";

		auto cam = me::Entity::Create("MainCamera");
		cam.Add(me::components::Transform2D{ 0, 0 });
		cam.Add(me::components::Camera2D{ 1.0f, true });
		me::render2d::SetActiveCamera(cam.Id());

		// Player
		auto player = me::Entity::Create("Player");
		player.Add(me::components::Transform2D{ 100, 100 });
		player.Add(me::components::SpriteRenderer{ {}, me::Color::Blue });
		player.Add(me::components::Velocity2D{});
		player.Add(me::components::AabbCollider{ 32, 32 });

		// Enemy (Target)
		auto enemy = me::Entity::Create("Enemy");
		enemy.Add(me::components::Transform2D{ 300, 100 });
		enemy.Add(me::components::SpriteRenderer{ {}, me::Color::Red });
		enemy.Add(me::components::AabbCollider{ 40, 40 });

		// Add our CUSTOM components!
		enemy.Add(Hittable{});
		enemy.Add(Health{ 50, 50 });
	}

	void OnUpdate(float dt) override {
		me::Entity player = me::FindEntity("Player");

		if (player.IsValid()) {
			auto* vel = player.TryGet<me::components::Velocity2D>();
			float speed = 200.0f;
			if (vel) {
				vel->vx = me::input::AxisValue("Horizontal") * speed;
				vel->vy = me::input::AxisValue("Vertical") * speed;
			}

			if (me::input::ActionPressed("Shoot")) {
				SpawnBullet(player.TryGet<me::components::Transform2D>());
			}
		}
	}

	void SpawnBullet(me::components::Transform2D* parentTrans) {
		if (!parentTrans) return;

		auto bullet = me::Entity::Create("Bullet");
		bullet.Add(me::components::Transform2D{ parentTrans->x, parentTrans->y });
		bullet.Add(me::components::SpriteRenderer{ {}, me::Color::Yellow });
		bullet.Add(me::components::Velocity2D{ 500.0f, 0.0f });
		bullet.Add(me::components::CircleCollider{ 10.0f });
		bullet.Add(me::components::Lifetime{ 2.0f });

		// Custom projectile component
		bullet.Add(Projectile{ 25, me::FindEntity("Player").Id() });
	}

	const char* GetName() const override { return "Level1"; }
};

/// --- Main App ---
class SandboxApp : public me::GameApp {
public:
	void OnStart() override {
		me::input::BindAction("Quit", me::input::Key::Escape);
		me::scene::manager::Register(&level1);
		me::scene::manager::Load("Level1");

		me::physics::SetGravity(0, 0);
	}

	void OnUpdate(float dt) override {
		if (me::input::ActionPressed("Quit"))
			me::RequestQuit();

		me::scene::manager::Update(dt);
		me::physics::Update(dt);
		CombatSystem_Update(dt);
		me::lifetime::Update(dt);
	}

	void OnRender() override {
		me::render2d::ClearWorld(me::Color{ 40, 40, 40, 255 });
		me::render2d::BeginCamera();
		me::render2d::RenderWorld();
		me::dbg::DrawAllCollidersWorld();
		me::render2d::EndCamera();

		std::string fps(std::to_string(me::time::GetFPS()));
		me::dbg::Text(10, 10, fps, me::Color::Green);
	}

private:
	LevelOne level1;
};

int main() {
	SandboxApp app;
	me::Run(app, "MiniEngine Sandbox", 1280, 720, true);
}