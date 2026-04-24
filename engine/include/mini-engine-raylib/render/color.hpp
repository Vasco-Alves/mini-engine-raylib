#pragma once

#include <cstdint>

namespace me {

	struct Color {
		std::uint8_t r = 255;
		std::uint8_t g = 255;
		std::uint8_t b = 255;
		std::uint8_t a = 255;

		constexpr Color() = default;

		constexpr Color(std::uint8_t rr, std::uint8_t gg, std::uint8_t bb, std::uint8_t aa = 255) noexcept : r(rr), g(gg), b(bb), a(aa) {}

		// Returns 0xRRGGBBAA
		constexpr std::uint32_t to_hex() const noexcept {
			return (static_cast<std::uint32_t>(r) << 24) |
				(static_cast<std::uint32_t>(g) << 16) |
				(static_cast<std::uint32_t>(b) << 8) |
				static_cast<std::uint32_t>(a);
		}

		// Returns 0x00RRGGBB (Useful for web/debug, ignores Alpha)
		constexpr std::uint32_t to_hex_rgb() const noexcept {
			return (static_cast<std::uint32_t>(r) << 16) |
				(static_cast<std::uint32_t>(g) << 8) |
				static_cast<std::uint32_t>(b);
		}

		// 0xRRGGBBAA
		static constexpr Color from_hex_rgba(std::uint32_t hex) noexcept {
			return Color{
				static_cast<std::uint8_t>((hex >> 24) & 0xFF), // R
				static_cast<std::uint8_t>((hex >> 16) & 0xFF), // G
				static_cast<std::uint8_t>((hex >> 8) & 0xFF),  // B
				static_cast<std::uint8_t>(hex & 0xFF)          // A
			};
		}

		// 0xRRGGBB + optional alpha
		static constexpr Color from_hex_rgb(std::uint32_t hex, std::uint8_t alpha = 255) noexcept {
			return Color{
				static_cast<std::uint8_t>((hex >> 16) & 0xFF),
				static_cast<std::uint8_t>((hex >> 8) & 0xFF),
				static_cast<std::uint8_t>(hex & 0xFF),
				alpha
			};
		}

		// Declarations only — definitions will follow below
		static const Color transparent;
		static const Color white;
		static const Color black;
		static const Color gray;
		static const Color light_gray;
		static const Color dark_gray;
		static const Color red;
		static const Color green;
		static const Color blue;
		static const Color yellow;
		static const Color cyan;
		static const Color magenta;
		static const Color orange;
		static const Color purple;
		static const Color brown;
		static const Color pink;
	};

	// ---- Definitions of static members ----
	inline const Color Color::transparent{ 0,   0,   0,   0 };
	inline const Color Color::white{ 255, 255, 255, 255 };
	inline const Color Color::black{ 0,   0,   0,   255 };
	inline const Color Color::gray{ 128, 128, 128, 255 };
	inline const Color Color::light_gray{ 192, 192, 192, 255 };
	inline const Color Color::dark_gray{ 64,  64,  64,  255 };
	inline const Color Color::red{ 255, 0,   0,   255 };
	inline const Color Color::green{ 0,   255, 0,   255 };
	inline const Color Color::blue{ 0,   0,   255, 255 };
	inline const Color Color::yellow{ 255, 255, 0,   255 };
	inline const Color Color::cyan{ 0,   255, 255, 255 };
	inline const Color Color::magenta{ 255, 0,   255, 255 };
	inline const Color Color::orange{ 255, 165, 0,   255 };
	inline const Color Color::purple{ 128, 0,   128, 255 };
	inline const Color Color::brown{ 150, 75,  0,   255 };
	inline const Color Color::pink{ 255, 192, 203, 255 };

} // namespace me