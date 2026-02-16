#ifndef NWAVE_CORE_MATH_UTILS_H
#define NWAVE_CORE_MATH_UTILS_H

#include <cmath>
#include <limits>
#include <random>

namespace nwave {

inline constexpr double pi = 3.14159265358979323846;
inline constexpr double infinity = std::numeric_limits<double>::infinity();
inline constexpr double epsilon = 0.001;

inline constexpr double degrees_to_radians(double degrees) {
    return degrees * pi / 180.0;
}

inline constexpr double clamp(double val, double min, double max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

inline double random_double() {
    thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    return distribution(generator);
}

inline double random_double(double min, double max) {
    return min + (max - min) * random_double();
}

} // namespace nwave

#endif // NWAVE_CORE_MATH_UTILS_H
