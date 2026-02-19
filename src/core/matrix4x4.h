#ifndef NWAVE_CORE_MATRIX4X4_H
#define NWAVE_CORE_MATRIX4X4_H

#include "quaternion.h"
#include "vec3.h"
#include <cmath>

namespace nwave {

class Matrix4x4 {
public:
    double m[4][4];

    constexpr Matrix4x4() : m{} {}

    static constexpr Matrix4x4 identity() {
        Matrix4x4 result;
        result.m[0][0] = 1.0;
        result.m[1][1] = 1.0;
        result.m[2][2] = 1.0;
        result.m[3][3] = 1.0;
        return result;
    }

    static constexpr Matrix4x4 translation(double dx, double dy, double dz) {
        Matrix4x4 result = identity();
        result.m[0][3] = dx;
        result.m[1][3] = dy;
        result.m[2][3] = dz;
        return result;
    }

    static Matrix4x4 from_translation_rotation(const Vec3& pos, const Quaternion& quat) {
        auto rot = quat.to_matrix();
        Matrix4x4 result;
        // Upper-left 3x3 from quaternion rotation matrix (row-major)
        result.m[0][0] = rot[0]; result.m[0][1] = rot[1]; result.m[0][2] = rot[2];
        result.m[1][0] = rot[3]; result.m[1][1] = rot[4]; result.m[1][2] = rot[5];
        result.m[2][0] = rot[6]; result.m[2][1] = rot[7]; result.m[2][2] = rot[8];
        // Translation in last column
        result.m[0][3] = pos.x();
        result.m[1][3] = pos.y();
        result.m[2][3] = pos.z();
        // Bottom row
        result.m[3][0] = 0.0;
        result.m[3][1] = 0.0;
        result.m[3][2] = 0.0;
        result.m[3][3] = 1.0;
        return result;
    }

    constexpr Matrix4x4 operator*(const Matrix4x4& other) const {
        Matrix4x4 result;
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                double sum = 0.0;
                for (int k = 0; k < 4; ++k) {
                    sum += m[r][k] * other.m[k][c];
                }
                result.m[r][c] = sum;
            }
        }
        return result;
    }

    constexpr Matrix4x4 transpose() const {
        Matrix4x4 result;
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                result.m[r][c] = m[c][r];
            }
        }
        return result;
    }

    // 4x4 matrix inverse using cofactor expansion (adjugate / determinant method)
    Matrix4x4 inverse() const {
        // Compute cofactors using Laplace expansion
        double inv[16];
        const double* s = &m[0][0]; // flat access

        inv[0]  =  s[5]*s[10]*s[15] - s[5]*s[11]*s[14] - s[9]*s[6]*s[15]
                  + s[9]*s[7]*s[14]  + s[13]*s[6]*s[11]  - s[13]*s[7]*s[10];
        inv[4]  = -s[4]*s[10]*s[15] + s[4]*s[11]*s[14] + s[8]*s[6]*s[15]
                  - s[8]*s[7]*s[14]  - s[12]*s[6]*s[11]  + s[12]*s[7]*s[10];
        inv[8]  =  s[4]*s[9]*s[15]  - s[4]*s[11]*s[13] - s[8]*s[5]*s[15]
                  + s[8]*s[7]*s[13]  + s[12]*s[5]*s[11]  - s[12]*s[7]*s[9];
        inv[12] = -s[4]*s[9]*s[14]  + s[4]*s[10]*s[13] + s[8]*s[5]*s[14]
                  - s[8]*s[6]*s[13]  - s[12]*s[5]*s[10]  + s[12]*s[6]*s[9];

        inv[1]  = -s[1]*s[10]*s[15] + s[1]*s[11]*s[14] + s[9]*s[2]*s[15]
                  - s[9]*s[3]*s[14]  - s[13]*s[2]*s[11]  + s[13]*s[3]*s[10];
        inv[5]  =  s[0]*s[10]*s[15] - s[0]*s[11]*s[14] - s[8]*s[2]*s[15]
                  + s[8]*s[3]*s[14]  + s[12]*s[2]*s[11]  - s[12]*s[3]*s[10];
        inv[9]  = -s[0]*s[9]*s[15]  + s[0]*s[11]*s[13] + s[8]*s[1]*s[15]
                  - s[8]*s[3]*s[13]  - s[12]*s[1]*s[11]  + s[12]*s[3]*s[9];
        inv[13] =  s[0]*s[9]*s[14]  - s[0]*s[10]*s[13] - s[8]*s[1]*s[14]
                  + s[8]*s[2]*s[13]  + s[12]*s[1]*s[10]  - s[12]*s[2]*s[9];

        inv[2]  =  s[1]*s[6]*s[15]  - s[1]*s[7]*s[14]  - s[5]*s[2]*s[15]
                  + s[5]*s[3]*s[14]  + s[13]*s[2]*s[7]   - s[13]*s[3]*s[6];
        inv[6]  = -s[0]*s[6]*s[15]  + s[0]*s[7]*s[14]  + s[4]*s[2]*s[15]
                  - s[4]*s[3]*s[14]  - s[12]*s[2]*s[7]   + s[12]*s[3]*s[6];
        inv[10] =  s[0]*s[5]*s[15]  - s[0]*s[7]*s[13]  - s[4]*s[1]*s[15]
                  + s[4]*s[3]*s[13]  + s[12]*s[1]*s[7]   - s[12]*s[3]*s[5];
        inv[14] = -s[0]*s[5]*s[14]  + s[0]*s[6]*s[13]  + s[4]*s[1]*s[14]
                  - s[4]*s[2]*s[13]  - s[12]*s[1]*s[6]   + s[12]*s[2]*s[5];

        inv[3]  = -s[1]*s[6]*s[11]  + s[1]*s[7]*s[10]  + s[5]*s[2]*s[11]
                  - s[5]*s[3]*s[10]  - s[9]*s[2]*s[7]    + s[9]*s[3]*s[6];
        inv[7]  =  s[0]*s[6]*s[11]  - s[0]*s[7]*s[10]  - s[4]*s[2]*s[11]
                  + s[4]*s[3]*s[10]  + s[8]*s[2]*s[7]    - s[8]*s[3]*s[6];
        inv[11] = -s[0]*s[5]*s[11]  + s[0]*s[7]*s[9]   + s[4]*s[1]*s[11]
                  - s[4]*s[3]*s[9]   - s[8]*s[1]*s[7]    + s[8]*s[3]*s[5];
        inv[15] =  s[0]*s[5]*s[10]  - s[0]*s[6]*s[9]   - s[4]*s[1]*s[10]
                  + s[4]*s[2]*s[9]   + s[8]*s[1]*s[6]    - s[8]*s[2]*s[5];

        double det = s[0]*inv[0] + s[1]*inv[4] + s[2]*inv[8] + s[3]*inv[12];

        double inv_det = 1.0 / det;

        Matrix4x4 result;
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                result.m[r][c] = inv[r * 4 + c] * inv_det;
            }
        }
        return result;
    }

    Point3 transform_point(const Point3& p) const {
        double x = m[0][0]*p.x() + m[0][1]*p.y() + m[0][2]*p.z() + m[0][3];
        double y = m[1][0]*p.x() + m[1][1]*p.y() + m[1][2]*p.z() + m[1][3];
        double z = m[2][0]*p.x() + m[2][1]*p.y() + m[2][2]*p.z() + m[2][3];
        double w = m[3][0]*p.x() + m[3][1]*p.y() + m[3][2]*p.z() + m[3][3];
        if (w != 1.0 && w != 0.0) {
            return Point3(x / w, y / w, z / w);
        }
        return Point3(x, y, z);
    }

    Vec3 transform_vector(const Vec3& v) const {
        double x = m[0][0]*v.x() + m[0][1]*v.y() + m[0][2]*v.z();
        double y = m[1][0]*v.x() + m[1][1]*v.y() + m[1][2]*v.z();
        double z = m[2][0]*v.x() + m[2][1]*v.y() + m[2][2]*v.z();
        return Vec3(x, y, z);
    }

    Vec3 transform_normal(const Vec3& n) const {
        // Normal transformation requires inverse-transpose of upper-left 3x3
        Matrix4x4 inv = inverse();
        // Transpose of inverse: use columns of inv as rows
        double x = inv.m[0][0]*n.x() + inv.m[1][0]*n.y() + inv.m[2][0]*n.z();
        double y = inv.m[0][1]*n.x() + inv.m[1][1]*n.y() + inv.m[2][1]*n.z();
        double z = inv.m[0][2]*n.x() + inv.m[1][2]*n.y() + inv.m[2][2]*n.z();
        return Vec3(x, y, z);
    }
};

} // namespace nwave

#endif // NWAVE_CORE_MATRIX4X4_H
