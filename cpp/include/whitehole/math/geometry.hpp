#pragma once

#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace whitehole::math {

struct Vec2f {
    float x{0.0F};
    float y{0.0F};

    [[nodiscard]] float length() const noexcept { return std::hypot(x, y); }
    [[nodiscard]] Vec2f normalized() const noexcept {
        const auto magnitude = length();
        return magnitude < 0.000001F ? *this : Vec2f{x / magnitude, y / magnitude};
    }
};

struct Vec3f {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};

    [[nodiscard]] float length() const noexcept { return std::sqrt(x * x + y * y + z * z); }
    [[nodiscard]] Vec3f normalized() const noexcept {
        const auto magnitude = length();
        return magnitude < 0.000001F ? *this : Vec3f{x / magnitude, y / magnitude, z / magnitude};
    }
    [[nodiscard]] static float dot(const Vec3f& left, const Vec3f& right) noexcept {
        return left.x * right.x + left.y * right.y + left.z * right.z;
    }
    [[nodiscard]] static Vec3f cross(const Vec3f& left, const Vec3f& right) noexcept {
        return {left.y * right.z - left.z * right.y,
                left.z * right.x - left.x * right.z,
                left.x * right.y - left.y * right.x};
    }
    [[nodiscard]] static Vec3f lerp(const Vec3f& from, const Vec3f& to, float amount) noexcept {
        return {from.x + (to.x - from.x) * amount,
                from.y + (to.y - from.y) * amount,
                from.z + (to.z - from.z) * amount};
    }
};

inline Vec3f operator+(const Vec3f& left, const Vec3f& right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

inline Vec3f operator-(const Vec3f& left, const Vec3f& right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

inline Vec3f operator*(const Vec3f& vector, float scale) noexcept {
    return {vector.x * scale, vector.y * scale, vector.z * scale};
}

struct Matrix4 {
    std::array<float, 16> values{1, 0, 0, 0,
                                 0, 1, 0, 0,
                                 0, 0, 1, 0,
                                 0, 0, 0, 1};

    [[nodiscard]] static Matrix4 scale(const Vec3f& scale) noexcept;
    [[nodiscard]] static Matrix4 translation(const Vec3f& translation) noexcept;
    [[nodiscard]] static Matrix4 rotationX(float radians) noexcept;
    [[nodiscard]] static Matrix4 rotationY(float radians) noexcept;
    [[nodiscard]] static Matrix4 rotationZ(float radians) noexcept;
    [[nodiscard]] Vec3f transformPoint(const Vec3f& point) const noexcept;
};

inline Matrix4 operator*(const Matrix4& left, const Matrix4& right) noexcept {
    Matrix4 result;
    result.values.fill(0.0F);
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            for (std::size_t index = 0; index < 4; ++index) {
                result.values[column * 4 + row] +=
                    left.values[index * 4 + row] * right.values[column * 4 + index];
            }
        }
    }
    return result;
}

inline Matrix4 Matrix4::scale(const Vec3f& scale) noexcept {
    Matrix4 result;
    result.values[0] = scale.x;
    result.values[5] = scale.y;
    result.values[10] = scale.z;
    return result;
}

inline Matrix4 Matrix4::translation(const Vec3f& translation) noexcept {
    Matrix4 result;
    result.values[12] = translation.x;
    result.values[13] = translation.y;
    result.values[14] = translation.z;
    return result;
}

inline Matrix4 Matrix4::rotationX(float radians) noexcept {
    Matrix4 result;
    const auto sine = std::sin(radians);
    const auto cosine = std::cos(radians);
    result.values[5] = cosine;
    result.values[6] = sine;
    result.values[9] = -sine;
    result.values[10] = cosine;
    return result;
}

inline Matrix4 Matrix4::rotationY(float radians) noexcept {
    Matrix4 result;
    const auto sine = std::sin(radians);
    const auto cosine = std::cos(radians);
    result.values[0] = cosine;
    result.values[2] = -sine;
    result.values[8] = sine;
    result.values[10] = cosine;
    return result;
}

inline Matrix4 Matrix4::rotationZ(float radians) noexcept {
    Matrix4 result;
    const auto sine = std::sin(radians);
    const auto cosine = std::cos(radians);
    result.values[0] = cosine;
    result.values[1] = sine;
    result.values[4] = -sine;
    result.values[5] = cosine;
    return result;
}

inline Vec3f Matrix4::transformPoint(const Vec3f& point) const noexcept {
    return {point.x * values[0] + point.y * values[4] + point.z * values[8] + values[12],
            point.x * values[1] + point.y * values[5] + point.z * values[9] + values[13],
            point.x * values[2] + point.y * values[6] + point.z * values[10] + values[14]};
}

} // namespace whitehole::math
