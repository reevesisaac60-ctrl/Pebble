#pragma once

#include "vec3.hpp"

#include <cmath>

// A 4x4 matrix in column-major order, matching OpenGL's expectations:
// element (row, column) lives at m[column * 4 + row], so a matrix can be
// uploaded with glUniformMatrix4fv(..., GL_FALSE, ...) without transposition.
// Vectors are column vectors: transformed = M * point.
struct Mat4 {
    float m[16];

    constexpr Mat4() {
        setIdentity();
    }

    constexpr float& operator()(int row, int column) {
        return m[column * 4 + row];
    }

    constexpr float operator()(int row, int column) const {
        return m[column * 4 + row];
    }

    constexpr void setIdentity() {
        for (int i = 0; i < 16; ++i) {
            m[i] = 0.0f;
        }
        m[0] = 1.0f;
        m[5] = 1.0f;
        m[10] = 1.0f;
        m[15] = 1.0f;
    }

    constexpr const float* pointer() const {
        return m;
    }

    constexpr bool operator==(const Mat4& other) const {
        for (int i = 0; i < 16; ++i) {
            if (m[i] != other.m[i]) {
                return false;
            }
        }
        return true;
    }

    constexpr bool operator!=(const Mat4& other) const {
        return !(*this == other);
    }
};

// Matrix product. Because storage is column-major, left(row, k) reads
// left.m[k * 4 + row].
inline Mat4 operator*(const Mat4& left, const Mat4& right) {
    Mat4 result;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += left.m[k * 4 + row] * right.m[column * 4 + k];
            }
            result.m[column * 4 + row] = sum;
        }
    }
    return result;
}

// A perspective projection matrix looking down the negative Z axis, with a
// right-handed view space: eye-space objects at -Z are in front of the camera.
inline Mat4 perspective(float fovYRadians, float aspect, float nearPlane, float farPlane) {
    Mat4 result;
    result.setIdentity();

    const float f = 1.0f / std::tan(fovYRadians * 0.5f);

    result(0, 0) = f / aspect;
    result(1, 1) = f;
    result(2, 2) = (farPlane + nearPlane) / (nearPlane - farPlane);
    result(3, 2) = -1.0f;
    result(2, 3) = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
    result(3, 3) = 0.0f;

    return result;
}

// A view matrix that places the camera at eye looking toward center.
// Column-major, column-vector convention: the translation (camera position)
// lives in column 3 so that M * point correctly transforms world points into
// view space.
inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    const Vec3 forward = (center - eye).normalized();
    const Vec3 side = cross(forward, up).normalized();
    const Vec3 cameraUp = cross(side, forward).normalized();

    Mat4 result;
    result.setIdentity();

    // For column vectors, the rows of the view rotation are the camera basis
    // vectors. The matrix is stored column-major, but storage order does not
    // change which (row, column) each basis component belongs to.
    result(0, 0) = side.x;
    result(0, 1) = side.y;
    result(0, 2) = side.z;
    result(1, 0) = cameraUp.x;
    result(1, 1) = cameraUp.y;
    result(1, 2) = cameraUp.z;
    result(2, 0) = -forward.x;
    result(2, 1) = -forward.y;
    result(2, 2) = -forward.z;

    // Standard right-handed view translation: -(R * eye).
    result(0, 3) = -dot(side, eye);
    result(1, 3) = -dot(cameraUp, eye);
    result(2, 3) = dot(forward, eye);
    result(3, 0) = 0.0f;
    result(3, 1) = 0.0f;
    result(3, 2) = 0.0f;
    result(3, 3) = 1.0f;

    return result;
}

// A rotation matrix of angleRadians around the given axis (Rodrigues).
inline Mat4 rotate(float angleRadians, const Vec3& axis) {
    const Vec3 normalized = axis.normalized();
    const float c = std::cos(angleRadians);
    const float s = std::sin(angleRadians);
    const float oneMinusC = 1.0f - c;
    const float x = normalized.x;
    const float y = normalized.y;
    const float z = normalized.z;

    Mat4 result;
    result.setIdentity();

    result(0, 0) = c + x * x * oneMinusC;
    result(1, 0) = y * x * oneMinusC + z * s;
    result(2, 0) = z * x * oneMinusC - y * s;
    result(3, 0) = 0.0f;

    result(0, 1) = x * y * oneMinusC - z * s;
    result(1, 1) = c + y * y * oneMinusC;
    result(2, 1) = z * y * oneMinusC + x * s;
    result(3, 1) = 0.0f;

    result(0, 2) = x * z * oneMinusC + y * s;
    result(1, 2) = y * z * oneMinusC - x * s;
    result(2, 2) = c + z * z * oneMinusC;
    result(3, 2) = 0.0f;

    result(0, 3) = 0.0f;
    result(1, 3) = 0.0f;
    result(2, 3) = 0.0f;
    result(3, 3) = 1.0f;

    return result;
}

// A translation matrix.
inline Mat4 translate(const Vec3& position) {
    Mat4 result;
    result.setIdentity();

    result(0, 3) = position.x;
    result(1, 3) = position.y;
    result(2, 3) = position.z;
    result(3, 3) = 1.0f;

    return result;
}

inline Vec3 transformPoint(const Mat4& matrix, const Vec3& point) {
    const float x = matrix(0, 0) * point.x + matrix(0, 1) * point.y + matrix(0, 2) * point.z + matrix(0, 3);
    const float y = matrix(1, 0) * point.x + matrix(1, 1) * point.y + matrix(1, 2) * point.z + matrix(1, 3);
    const float z = matrix(2, 0) * point.x + matrix(2, 1) * point.y + matrix(2, 2) * point.z + matrix(2, 3);
    const float w = matrix(3, 0) * point.x + matrix(3, 1) * point.y + matrix(3, 2) * point.z + matrix(3, 3);
    if (w != 0.0f && w != 1.0f) {
        return Vec3(x / w, y / w, z / w);
    }
    return Vec3(x, y, z);
}
