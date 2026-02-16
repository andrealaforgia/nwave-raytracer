#ifndef NWAVE_CORE_VEC3_H
#define NWAVE_CORE_VEC3_H

#include "math_utils.h"
#include <cmath>

namespace nwave {

class Vec3 {
public:
    double e[3];

    constexpr Vec3() : e{0.0, 0.0, 0.0} {}
    constexpr Vec3(double x, double y, double z) : e{x, y, z} {}

    constexpr double x() const { return e[0]; }
    constexpr double y() const { return e[1]; }
    constexpr double z() const { return e[2]; }

    constexpr double r() const { return e[0]; }
    constexpr double g() const { return e[1]; }
    constexpr double b() const { return e[2]; }

    constexpr double operator[](int i) const { return e[i]; }
    constexpr double& operator[](int i) { return e[i]; }

    constexpr Vec3 operator-() const {
        return Vec3(-e[0], -e[1], -e[2]);
    }

    constexpr Vec3& operator+=(const Vec3& v) {
        e[0] += v.e[0];
        e[1] += v.e[1];
        e[2] += v.e[2];
        return *this;
    }

    constexpr Vec3& operator-=(const Vec3& v) {
        e[0] -= v.e[0];
        e[1] -= v.e[1];
        e[2] -= v.e[2];
        return *this;
    }

    constexpr Vec3& operator*=(double t) {
        e[0] *= t;
        e[1] *= t;
        e[2] *= t;
        return *this;
    }

    constexpr Vec3& operator/=(double t) {
        return *this *= (1.0 / t);
    }

    double length() const {
        return std::sqrt(length_squared());
    }

    constexpr double length_squared() const {
        return e[0] * e[0] + e[1] * e[1] + e[2] * e[2];
    }

    bool near_zero() const {
        constexpr double s = 1e-8;
        return (std::fabs(e[0]) < s) && (std::fabs(e[1]) < s) && (std::fabs(e[2]) < s);
    }

    static Vec3 random() {
        return Vec3(random_double(), random_double(), random_double());
    }

    static Vec3 random(double min, double max) {
        return Vec3(random_double(min, max), random_double(min, max), random_double(min, max));
    }
};

// Type aliases
using Point3 = Vec3;
using Color3 = Vec3;

// Binary operators

inline constexpr Vec3 operator+(const Vec3& a, const Vec3& b) {
    return Vec3(a.e[0] + b.e[0], a.e[1] + b.e[1], a.e[2] + b.e[2]);
}

inline constexpr Vec3 operator-(const Vec3& a, const Vec3& b) {
    return Vec3(a.e[0] - b.e[0], a.e[1] - b.e[1], a.e[2] - b.e[2]);
}

inline constexpr Vec3 operator*(const Vec3& a, const Vec3& b) {
    return Vec3(a.e[0] * b.e[0], a.e[1] * b.e[1], a.e[2] * b.e[2]);
}

inline constexpr Vec3 operator*(double t, const Vec3& v) {
    return Vec3(t * v.e[0], t * v.e[1], t * v.e[2]);
}

inline constexpr Vec3 operator*(const Vec3& v, double t) {
    return t * v;
}

inline constexpr Vec3 operator/(const Vec3& v, double t) {
    return (1.0 / t) * v;
}

inline constexpr Vec3 operator/(const Vec3& a, const Vec3& b) {
    return Vec3(a.e[0] / b.e[0], a.e[1] / b.e[1], a.e[2] / b.e[2]);
}

// Geometric functions

inline constexpr double dot(const Vec3& a, const Vec3& b) {
    return a.e[0] * b.e[0] + a.e[1] * b.e[1] + a.e[2] * b.e[2];
}

inline constexpr Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(
        a.e[1] * b.e[2] - a.e[2] * b.e[1],
        a.e[2] * b.e[0] - a.e[0] * b.e[2],
        a.e[0] * b.e[1] - a.e[1] * b.e[0]
    );
}

inline Vec3 normalize(const Vec3& v) {
    return v / v.length();
}

// Reflection

inline Vec3 reflect(const Vec3& v, const Vec3& n) {
    return v - 2 * dot(v, n) * n;
}

// Refraction

inline Vec3 refract(const Vec3& uv, const Vec3& n, double etai_over_etat) {
    auto cos_theta = std::fmin(dot(-uv, n), 1.0);
    Vec3 r_out_perp = etai_over_etat * (uv + cos_theta * n);
    Vec3 r_out_parallel = -std::sqrt(std::fabs(1.0 - r_out_perp.length_squared())) * n;
    return r_out_perp + r_out_parallel;
}

// Random vector generation

inline Vec3 random_in_unit_sphere() {
    while (true) {
        Vec3 p = Vec3::random(-1.0, 1.0);
        if (p.length_squared() < 1.0)
            return p;
    }
}

inline Vec3 random_unit_vector() {
    return normalize(random_in_unit_sphere());
}

inline Vec3 random_in_unit_disk() {
    while (true) {
        Vec3 p(random_double(-1.0, 1.0), random_double(-1.0, 1.0), 0.0);
        if (p.length_squared() < 1.0)
            return p;
    }
}

} // namespace nwave

#endif // NWAVE_CORE_VEC3_H
