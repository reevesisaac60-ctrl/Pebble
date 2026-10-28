#pragma once

#include <cmath>

// A three-component vector used for points, colors and directions.
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(float xIn, float yIn, float zIn) : x(xIn), y(yIn), z(zIn) {}

    constexpr Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }

    constexpr Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }

    constexpr Vec3 operator*(float scalar) const {
        return Vec3(x * scalar, y * scalar, z * scalar);
    }

    constexpr Vec3& operator+=(const Vec3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    constexpr float lengthSquared() const {
        return x * x + y * y + z * z;
    }

    constexpr Vec3 normalized() const {
        const float length = std::sqrt(lengthSquared());
        if (length == 0.0f) {
            return Vec3(0.0f, 0.0f, 0.0f);
        }
        return Vec3(x / length, y / length, z / length);
    }
};

constexpr Vec3 operator*(float scalar, const Vec3& vector) {
    return vector * scalar;
}

constexpr float dot(const Vec3& left, const Vec3& right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

constexpr Vec3 cross(const Vec3& left, const Vec3& right) {
    return Vec3(
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x
    );
}
