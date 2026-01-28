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

	// 1. Get Pools
	auto* projPool = reg.TryGetPool<Projectile>();
	auto* hitPool = reg.TryGetPool<Hittable>();

	if (!projPool || !hitPool) return;

	// Helper to calculate overlap
	auto Overlap = [](float cx, float cy, float r, float bx, float by, float bw, float bh) {
		float closestX = std::max(bx, std::min(cx, bx + bw));
		float closestY = std::max(by, std::min(cy, by + bh));
		float dx = cx - closestX;
		float dy = cy - closestY;
		return (dx * dx + dy * dy) <= (r * r);
		};

	// 2. Iterate Projectiles
	struct HitEvent { me::EntityId bullet; me::EntityId victim; };
	std::vector<HitEvent> hits;

	for (auto& pKv : projPool->data) {
		me::EntityId pId = pKv.first;
		const auto& proj = pKv.second;

		auto* pT = reg.TryGetComponent<me::components::Transform2D>(pId);
		// Assuming bullets are CircleColliders for simplicity here
		auto* pCol = reg.TryGetComponent<me::components::CircleCollider>(pId);

		if (!pT || !pCol) continue;

		// Check against Hittables
		for (auto& hKv : hitPool->data) {
			me::EntityId tId = hKv.first;
			if (tId == proj.owner) continue; // Don't hit owner

			auto* tT = reg.TryGetComponent<me::components::Transform2D>(tId);
			auto* tCol = reg.TryGetComponent<me::components::AabbCollider>(tId);

			if (!tT || !tCol) continue;

			// Simple Circle vs AABB check
			// (Bullet is circle, Wall/Enemy is AABB)
			float boxX = tT->x + tCol->ox - tCol->w * 0.5f;
			float boxY = tT->y + tCol->oy - tCol->h * 0.5f;

			if (Overlap(pT->x + pCol->ox, pT->y + pCol->oy, pCol->radius,
				boxX, boxY, tCol->w, tCol->h)) {
				hits.push_back({ pId, tId });
				break; // Bullet hits one thing and stops
			}
		}
	}

	// 3. Resolve Hits
	for (const auto& ev : hits) {
		if (!me::IsAlive(ev.bullet) || !me::IsAlive(ev.victim)) continue;

		auto* pData = reg.TryGetComponent<Projectile>(ev.bullet);
		if (pData) {
			if (auto* hp = reg.TryGetComponent<Health>(ev.victim)) {
				hp->current -= pData->damage;
				std::cout << "Hit! HP: " << hp->current << "\n";
				if (hp->current <= 0) {
					me::DestroyEntity(ev.victim);
					std::cout << "Enemy Destroyed!\n";
				}
			}
		}
		me::DestroyEntity(ev.bullet);
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
	}

	void OnUpdate(float dt) override {
		if (me::input::ActionPressed("Quit"))
			me::RequestQuit();

		me::scene::manager::Update(dt);
		me::physics2d::Update(dt);
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