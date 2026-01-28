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
		constexpr std::uint32_t ToHex() const noexcept {
			return (static_cast<std::uint32_t>(r) << 24) |
				(static_cast<std::uint32_t>(g) << 16) |
				(static_cast<std::uint32_t>(b) << 8) |
				static_cast<std::uint32_t>(a);
		}

		// Returns 0x00RRGGBB (Useful for web/debug, ignores Alpha)
		constexpr std::uint32_t ToHexRGB() const noexcept {
			return (static_cast<std::uint32_t>(r) << 16) |
				(static_cast<std::uint32_t>(g) << 8) |
				static_cast<std::uint32_t>(b);
		}

		// 0xRRGGBBAA
		static constexpr Color FromHexRGBA(std::uint32_t hex) noexcept {
			return Color{
				static_cast<std::uint8_t>((hex >> 24) & 0xFF), // R
				static_cast<std::uint8_t>((hex >> 16) & 0xFF), // G
				static_cast<std::uint8_t>((hex >> 8) & 0xFF), // B
				static_cast<std::uint8_t>(hex & 0xFF)  // A
			};
		}

		// 0xRRGGBB + optional alpha
		static constexpr Color FromHexRGB(std::uint32_t hex, std::uint8_t alpha = 255) noexcept {
			return Color{
				static_cast<std::uint8_t>((hex >> 16) & 0xFF),
				static_cast<std::uint8_t>((hex >> 8) & 0xFF),
				static_cast<std::uint8_t>(hex & 0xFF),
				alpha
			};
		}

		// Declarations only — definitions will follow below
		static const Color Transparent;
		static const Color White;
		static const Color Black;
		static const Color Gray;
		static const Color LightGray;
		static const Color DarkGray;
		static const Color Red;
		static const Color Green;
		static const Color Blue;
		static const Color Yellow;
		static const Color Cyan;
		static const Color Magenta;
		static const Color Orange;
		static const Color Purple;
		static const Color Brown;
		static const Color Pink;
	};

	// ---- Definitions of static members ----
	inline const Color Color::Transparent{ 0,   0,   0,   0 };
	inline const Color Color::White{ 255, 255, 255, 255 };
	inline const Color Color::Black{ 0,   0,   0,   255 };
	inline const Color Color::Gray{ 128, 128, 128, 255 };
	inline const Color Color::LightGray{ 192, 192, 192, 255 };
	inline const Color Color::DarkGray{ 64,  64,  64,  255 };
	inline const Color Color::Red{ 255, 0,   0,   255 };
	inline const Color Color::Green{ 0,   255, 0,   255 };
	inline const Color Color::Blue{ 0,   0,   255, 255 };
	inline const Color Color::Yellow{ 255, 255, 0,   255 };
	inline const Color Color::Cyan{ 0,   255, 255, 255 };
	inline const Color Color::Magenta{ 255, 0,   255, 255 };
	inline const Color Color::Orange{ 255, 165, 0,   255 };
	inline const Color Color::Purple{ 128, 0,   128, 255 };
	inline const Color Color::Brown{ 150, 75,  0,   255 };
	inline const Color Color::Pink{ 255, 192, 203, 255 };

} // namespace me