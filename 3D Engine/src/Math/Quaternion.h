#pragma once

#include <cmath>
#include "Matrix.h"

class Quaternion {
public:
    float x, y, z, w;

    Quaternion() noexcept : x(0), y(0), z(0), w(1) {}

    Quaternion(float _x, float _y, float _z, float _w) noexcept
        : x(_x), y(_y), z(_z), w(_w) {
    }

    static Quaternion fromAxisAngle(const Vector3& axis, float angle) noexcept {
        float half = angle * 0.5f;
        float s = std::sin(half);
        Vector3 a = axis.normalized();
        return Quaternion(a.x * s, a.y * s, a.z * s, std::cos(half));
    }

    static Quaternion fromEuler(const Vector3& euler) noexcept {
        float cx = std::cos(euler.x * 0.5f);
        float sx = std::sin(euler.x * 0.5f);
        float cy = std::cos(euler.y * 0.5f);
        float sy = std::sin(euler.y * 0.5f);
        float cz = std::cos(euler.z * 0.5f);
        float sz = std::sin(euler.z * 0.5f);

        // Z * Y * X
        return Quaternion(
            cx * sy * sz + sx * cy * cz,
            cx * cy * sz - sx * sy * cz,
            sx * cy * sz + cx * sy * cz,
            cx * cy * cz - sx * sy * sz
        );
    }

    Vector3 toEuler() const noexcept {
        Vector3 e;

        float sinY = -2.0f * (x * z - w * y);
        sinY = (sinY > 1.0f) ? 1.0f : (sinY < -1.0f) ? -1.0f : sinY;
        e.y = std::asin(sinY);

        e.x = std::atan2(2.0f * (y * z + w * x), w * w - x * x - y * y + z * z);

        e.z = std::atan2(2.0f * (x * y + w * z), w * w + x * x - y * y - z * z);
        return e;
    }

    Quaternion& normalize() noexcept {
        float len = std::sqrt(x * x + y * y + z * z + w * w);
        if (len > 0.0f) {
            float inv = 1.0f / len;
            x *= inv; y *= inv; z *= inv; w *= inv;
        }
        return *this;
    }

    Quaternion operator*(const Quaternion& r) const noexcept {
        return Quaternion(
            w * r.x + x * r.w + y * r.z - z * r.y,
            w * r.y - x * r.z + y * r.w + z * r.x,
            w * r.z + x * r.y - y * r.x + z * r.w,
            w * r.w - x * r.x - y * r.y - z * r.z
        );
    }

    Matrix toMatrix() const noexcept {
        Matrix m;
        float xx = x * x, yy = y * y, zz = z * z;
        float xy = x * y, xz = x * z, yz = y * z;
        float wx = w * x, wy = w * y, wz = w * z;

        m.mat[0][0] = 1 - 2 * (yy + zz);
        m.mat[0][1] = 2 * (xy + wz);
        m.mat[0][2] = 2 * (xz - wy);
        m.mat[0][3] = 0;

        m.mat[1][0] = 2 * (xy - wz);
        m.mat[1][1] = 1 - 2 * (xx + zz);
        m.mat[1][2] = 2 * (yz + wx);
        m.mat[1][3] = 0;

        m.mat[2][0] = 2 * (xz + wy);
        m.mat[2][1] = 2 * (yz - wx);
        m.mat[2][2] = 1 - 2 * (xx + yy);
        m.mat[2][3] = 0;

        m.mat[3][0] = 0;
        m.mat[3][1] = 0;
        m.mat[3][2] = 0;
        m.mat[3][3] = 1;
        return m;
    }
};
