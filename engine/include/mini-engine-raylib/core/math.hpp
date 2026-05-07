#pragma once

#include <cmath>

namespace me::math {

    struct Vec2 {
        float x = 0.0f, y = 0.0f;

        Vec2 operator+(const Vec2& v) const { return { x + v.x, y + v.y }; }
        Vec2 operator-(const Vec2& v) const { return { x - v.x, y - v.y }; }
        Vec2 operator*(float scalar) const { return { x * scalar, y * scalar }; }

        float length() const { return std::sqrt(x * x + y * y); }

        Vec2 normalize() const {
            float len = length();
            if (len > 0) return { x / len, y / len };
            return { 0.0f, 0.0f };
        }
    };

    static constexpr float pi = 3.14159265358979323846f;

} // namespace me::math