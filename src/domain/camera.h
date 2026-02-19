#ifndef NWAVE_DOMAIN_CAMERA_H
#define NWAVE_DOMAIN_CAMERA_H

#include "core/vec3.h"
#include "core/ray.h"

namespace nwave {

class Camera {
public:
    Camera(Point3 lookfrom, Point3 lookat, Vec3 vup,
           double vfov, double aspect_ratio, int image_width);

    Ray generate_ray(int px, int py) const;
    Ray generate_ray_random(int px, int py) const;

    int image_width() const { return image_width_; }
    int image_height() const { return image_height_; }
    const Point3& lookfrom() const { return lookfrom_; }
    const Point3& lookat() const { return lookat_; }
    const Vec3& vup() const { return vup_; }
    double vfov() const { return vfov_; }
    double aspect_ratio() const { return aspect_ratio_; }
    const Point3& pixel00_loc() const { return pixel00_loc_; }
    const Vec3& pixel_delta_u() const { return pixel_delta_u_; }
    const Vec3& pixel_delta_v() const { return pixel_delta_v_; }

private:
    Point3 lookfrom_;
    Point3 lookat_;
    Vec3 vup_;
    double vfov_;
    double aspect_ratio_;

    int image_width_;
    int image_height_;

    Vec3 u_, v_, w_;
    Point3 pixel00_loc_;
    Vec3 pixel_delta_u_;
    Vec3 pixel_delta_v_;
};

} // namespace nwave

#endif // NWAVE_DOMAIN_CAMERA_H
