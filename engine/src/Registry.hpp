#pragma once

#include "Entity.hpp"
#include "Components.hpp"

#include <unordered_map>
#include <string>
#include <functional>

namespace me::detail {

	struct Registry {
		struct EntityRecord {
			bool alive = false;
			std::string name;
			me::components::Transform2D transform{};
		};

		std::unordered_map<me::EntityId, EntityRecord> entities;
		me::EntityId nextId = 1;
		std::unordered_map<std::string, me::EntityId> nameIndex;

		// ---- Component stores (1 map per type) ----
		std::unordered_map<me::EntityId, me::components::SpriteRenderer>   sprites;
		std::unordered_map<me::EntityId, me::components::Camera2D>         cameras;
		std::unordered_map<me::EntityId, me::components::Velocity2D>       velocities;
		std::unordered_map<me::EntityId, me::components::AabbCollider>     colliders;
		std::unordered_map<me::EntityId, me::components::CircleCollider>   circleColliders;

		std::unordered_map<me::EntityId, me::components::SpriteSheet>      animSheets;
		std::unordered_map<me::EntityId, me::components::AnimationPlayer>  animPlayers;

		inline EntityRecord* Lookup(me::EntityId e) {
			if (e == 0) return nullptr;
			auto it = entities.find(e);
			return (it == entities.end()) ? nullptr : &it->second;
		}

		inline void EraseAllForEntity(me::EntityId e) {
			sprites.erase(e);
			cameras.erase(e);
			velocities.erase(e);
			colliders.erase(e);
			circleColliders.erase(e);
		}

		inline void ForEachEntity(const std::function<void(me::EntityId)>& fn) const {
			for (const auto& kv : entities) fn(kv.first);
		}

		inline void ForEachSprite(const std::function<void(me::EntityId, const me::components::SpriteRenderer&)>& fn) const {
			for (const auto& kv : sprites) fn(kv.first, kv.second);
		}

		inline void ForEachCamera(const std::function<void(me::EntityId, const me::components::Camera2D&)>& fn) const {
			for (const auto& kv : cameras) fn(kv.first, kv.second);
		}

		inline void ForEachVelocity(const std::function<void(me::EntityId, const me::components::Velocity2D&)>& fn) const {
			for (const auto& kv : velocities) fn(kv.first, kv.second);
		}

		inline void ForEachCollider(const std::function<void(me::EntityId, const me::components::AabbCollider&)>& fn) const {
			for (const auto& kv : colliders) fn(kv.first, kv.second);
		}

		inline void ForEachCircleCollider(const std::function<void(me::EntityId, const me::components::CircleCollider&)>& fn) const {
			for (const auto& kv : circleColliders) fn(kv.first, kv.second);
		}
	};

	Registry& Reg();
}
