#pragma once
#include "third_party/HandmadeMath.h"
#include <cmath>

struct Vec4 {
    float x, y, z, w;
    Vec4() : x(0), y(0), z(0), w(1) {}
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
};

struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    Vec3(HMM_Vec3 v) : x(v.X), y(v.Y), z(v.Z) {}
    operator HMM_Vec3() const { HMM_Vec3 r; r.X = x; r.Y = y; r.Z = z; return r; }

    Vec3 operator+(const Vec3& v) const {
        HMM_Vec3 r = HMM_AddV3(*this, v);
        return Vec3(r.X, r.Y, r.Z);
    }
    Vec3 operator-(const Vec3& v) const {
        HMM_Vec3 r = HMM_SubV3(*this, v);
        return Vec3(r.X, r.Y, r.Z);
    }
    Vec3 operator*(float s) const {
        HMM_Vec3 r = HMM_MulV3F(*this, s);
        return Vec3(r.X, r.Y, r.Z);
    }
    Vec3 operator/(float s) const {
        HMM_Vec3 r = HMM_DivV3F(*this, s);
        return Vec3(r.X, r.Y, r.Z);
    }
};

inline Vec3 Normalize(const Vec3& v) {
    HMM_Vec3 r = HMM_NormV3(v);
    return Vec3(r.X, r.Y, r.Z);
}

inline Vec3 Cross(const Vec3& a, const Vec3& b) {
    HMM_Vec3 r = HMM_Cross(a, b);
    return Vec3(r.X, r.Y, r.Z);
}

inline float Dot(const Vec3& a, const Vec3& b) {
    return HMM_DotV3(a, b);
}

struct Mat4 {
    float m[16];

    Mat4() { for (int i = 0; i < 16; ++i) m[i] = 0; }
    Mat4(const HMM_Mat4& hmm) {
        for (int i = 0; i < 16; ++i)
            m[i] = hmm.Elements[i / 4][i % 4];
    }
    operator HMM_Mat4() const {
        HMM_Mat4 r = {};
        for (int i = 0; i < 16; ++i)
            r.Elements[i / 4][i % 4] = m[i];
        return r;
    }

    static Mat4 Identity() {
        HMM_Mat4 r = HMM_M4D(1.0f);
        return Mat4(r);
    }

    static Mat4 Translate(float tx, float ty, float tz) {
        HMM_Mat4 r = HMM_Translate(HMM_V3(tx, ty, tz));
        return Mat4(r);
    }

    static Mat4 Scale(float sx, float sy, float sz) {
        HMM_Mat4 r = HMM_Scale(HMM_V3(sx, sy, sz));
        return Mat4(r);
    }

    static Mat4 FromQuaternion(const Vec4& q) {
        // Normalize quaternion first
        float len = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
        if (len < 1e-7f) return Identity();
        float x = q.x / len, y = q.y / len, z = q.z / len, w = q.w / len;
        HMM_Mat4 r = HMM_M4D(1.0f);
        float x2 = x + x, y2 = y + y, z2 = z + z;
        float xx = x * x2, xy = x * y2, xz = x * z2;
        float yy = y * y2, yz = y * z2, zz = z * z2;
        float wx = w * x2, wy = w * y2, wz = w * z2;
        r.Elements[0][0] = 1.0f - (yy + zz);
        r.Elements[0][1] = xy + wz;
        r.Elements[0][2] = xz - wy;
        r.Elements[1][0] = xy - wz;
        r.Elements[1][1] = 1.0f - (xx + zz);
        r.Elements[1][2] = yz + wx;
        r.Elements[2][0] = xz + wy;
        r.Elements[2][1] = yz - wx;
        r.Elements[2][2] = 1.0f - (xx + yy);
        return Mat4(r);
    }

    Mat4 operator*(const Mat4& b) const {
        HMM_Mat4 r = HMM_MulM4(*this, b);
        return Mat4(r);
    }

    const float* Data() const { return m; }

    void SetOrtho(float left, float right, float bottom, float top, float nearZ, float farZ) {
        HMM_Mat4 r = HMM_Orthographic_RH_NO(left, right, bottom, top, nearZ, farZ);
        for (int i = 0; i < 16; ++i)
            m[i] = r.Elements[i / 4][i % 4];
    }
};

inline Mat4 Perspective(float fovDeg, float aspect, float nearZ, float farZ) {
    float fovRad = fovDeg * 3.14159265f / 180.0f;
    HMM_Mat4 r = HMM_Perspective_RH_NO(fovRad, aspect, nearZ, farZ);
    return Mat4(r);
}

inline Mat4 LookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    HMM_Mat4 r = HMM_LookAt_RH(eye, center, up);
    return Mat4(r);
}

inline Mat4 Translate(const Mat4& mat, const Vec3& v) {
    // HMM_Translate creates a translation matrix, multiply: T * mat
    HMM_Mat4 t = HMM_Translate(v);
    HMM_Mat4 r = HMM_MulM4(t, mat);
    return Mat4(r);
}
