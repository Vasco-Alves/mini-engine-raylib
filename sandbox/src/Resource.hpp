#pragma once
#include <cstdint>

namespace driftspace {

	enum class ResourceType : std::uint8_t {
		None = 0,
		BasaltOre,
		IronOre,
		CrystalShard,
	};

	inline const char* ToString(ResourceType t) {
		switch (t) {
		case ResourceType::BasaltOre:   return "Basalt";
		case ResourceType::IronOre:     return "Iron";
		case ResourceType::CrystalShard:return "Crystal";
		default:                        return "None";
		}
	}

} // namespace driftspace
