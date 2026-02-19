#ifndef NWAVE_CORE_QUATERNION_H
#define NWAVE_CORE_QUATERNION_H

#include "math_utils.h"
#include "vec3.h"
#include <array>
#include <cmath>

namespace nwave {

class Quaternion {
public:
    double e[4]; // x, y, z, w

    constexpr Quaternion() : e{0.0, 0.0, 0.0, 1.0} {}
    constexpr Quaternion(double x, double y, double z, double w) : e{x, y, z, w} {}

    constexpr double x() const { return e[0]; }
    constexpr double y() const { return e[1]; }
    constexpr double z() const { return e[2]; }
    constexpr double w() const { return e[3]; }

    static constexpr Quaternion identity() {
        return Quaternion(0.0, 0.0, 0.0, 1.0);
    }

    static Quaternion from_axis_angle(const Vec3& axis, double angle_radians) {
        Vec3 n = normalize(axis);
        double half = angle_radians / 2.0;
        double s = std::sin(half);
        double c = std::cos(half);
        return Quaternion(n.x() * s, n.y() * s, n.z() * s, c);
    }

    constexpr Quaternion conjugate() const {
        return Quaternion(-e[0], -e[1], -e[2], e[3]);
    }

    Quaternion normalized() const {
        double len = std::sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2] + e[3] * e[3]);
        return Quaternion(e[0] / len, e[1] / len, e[2] / len, e[3] / len);
    }

    constexpr Quaternion operator*(const Quaternion& r) const {
        // Hamilton product: q * r
        return Quaternion(
            e[3] * r.e[0] + e[0] * r.e[3] + e[1] * r.e[2] - e[2] * r.e[1],
            e[3] * r.e[1] - e[0] * r.e[2] + e[1] * r.e[3] + e[2] * r.e[0],
            e[3] * r.e[2] + e[0] * r.e[1] - e[1] * r.e[0] + e[2] * r.e[3],
            e[3] * r.e[3] - e[0] * r.e[0] - e[1] * r.e[1] - e[2] * r.e[2]
        );
    }

    // Returns 9 rotation matrix components in row-major order:
    // [m0 m1 m2]
    // [m3 m4 m5]
    // [m6 m7 m8]
    std::array<double, 9> to_matrix() const {
        double xx = e[0] * e[0];
        double yy = e[1] * e[1];
        double zz = e[2] * e[2];
        double xy = e[0] * e[1];
        double xz = e[0] * e[2];
        double yz = e[1] * e[2];
        double wx = e[3] * e[0];
        double wy = e[3] * e[1];
        double wz = e[3] * e[2];

        return {{
            1.0 - 2.0 * (yy + zz),  2.0 * (xy - wz),        2.0 * (xz + wy),
            2.0 * (xy + wz),         1.0 - 2.0 * (xx + zz),  2.0 * (yz - wx),
            2.0 * (xz - wy),         2.0 * (yz + wx),         1.0 - 2.0 * (xx + yy)
        }};
    }

    static Quaternion slerp(const Quaternion& a, const Quaternion& b, double t) {
        double dot_val = a.e[0] * b.e[0] + a.e[1] * b.e[1]
                       + a.e[2] * b.e[2] + a.e[3] * b.e[3];

        // If dot is negative, negate one quaternion to take the shorter arc
        Quaternion b_adj = b;
        if (dot_val < 0.0) {
            b_adj = Quaternion(-b.e[0], -b.e[1], -b.e[2], -b.e[3]);
            dot_val = -dot_val;
        }

        // If quaternions are very close, use linear interpolation to avoid division by zero
        constexpr double threshold = 0.9995;
        if (dot_val > threshold) {
            Quaternion result(
                a.e[0] + t * (b_adj.e[0] - a.e[0]),
                a.e[1] + t * (b_adj.e[1] - a.e[1]),
                a.e[2] + t * (b_adj.e[2] - a.e[2]),
                a.e[3] + t * (b_adj.e[3] - a.e[3])
            );
            return result.normalized();
        }

        double theta = std::acos(dot_val);
        double sin_theta = std::sin(theta);
        double weight_a = std::sin((1.0 - t) * theta) / sin_theta;
        double weight_b = std::sin(t * theta) / sin_theta;

        return Quaternion(
            weight_a * a.e[0] + weight_b * b_adj.e[0],
            weight_a * a.e[1] + weight_b * b_adj.e[1],
            weight_a * a.e[2] + weight_b * b_adj.e[2],
            weight_a * a.e[3] + weight_b * b_adj.e[3]
        );
    }
};

} // namespace nwave

#endif // NWAVE_CORE_QUATERNION_H
