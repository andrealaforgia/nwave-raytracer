#include "domain/camera.h"
#include "core/math_utils.h"
#include <algorithm>
#include <cmath>

namespace nwave {

Camera::Camera(Point3 lookfrom, Point3 lookat, Vec3 vup,
               double vfov, double aspect_ratio, int image_width)
    : lookfrom_(lookfrom), lookat_(lookat), vup_(vup),
      vfov_(vfov), aspect_ratio_(aspect_ratio), image_width_(image_width) {

    image_height_ = std::max(1, static_cast<int>(image_width_ / aspect_ratio_));

    w_ = normalize(lookfrom_ - lookat_);
    u_ = normalize(cross(vup_, w_));
    v_ = cross(w_, u_);

    double theta = degrees_to_radians(vfov_);
    double h = std::tan(theta / 2.0);
    double focus_dist = (lookfrom_ - lookat_).length();
    double viewport_height = 2.0 * h * focus_dist;
    double viewport_width = viewport_height * (static_cast<double>(image_width_) / image_height_);

    Vec3 viewport_u = viewport_width * u_;
    Vec3 viewport_v = viewport_height * (-v_);

    pixel_delta_u_ = viewport_u / image_width_;
    pixel_delta_v_ = viewport_v / image_height_;

    Point3 viewport_upper_left = lookfrom_ - (focus_dist * w_)
                                 - viewport_u / 2.0 - viewport_v / 2.0;

    pixel00_loc_ = viewport_upper_left + 0.5 * (pixel_delta_u_ + pixel_delta_v_);
}

Ray Camera::generate_ray(int px, int py) const {
    Point3 pixel_center = pixel00_loc_
                          + (px * pixel_delta_u_)
                          + (py * pixel_delta_v_);
    Vec3 ray_direction = pixel_center - lookfrom_;
    return Ray(lookfrom_, ray_direction);
}

Ray Camera::generate_ray_random(int px, int py) const {
    auto offset_x = random_double() - 0.5;
    auto offset_y = random_double() - 0.5;
    Point3 pixel_sample = pixel00_loc_
                          + ((px + offset_x) * pixel_delta_u_)
                          + ((py + offset_y) * pixel_delta_v_);
    Vec3 ray_direction = pixel_sample - lookfrom_;
    return Ray(lookfrom_, ray_direction);
}

} // namespace nwave
