#include "domain/scene.h"

namespace nwave {

void Scene::add_shape(std::shared_ptr<Shape> shape) {
    shapes_.push_back(std::move(shape));
}

void Scene::add_light(std::shared_ptr<Light> light) {
    lights_.push_back(std::move(light));
}

bool Scene::hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const {
    HitRecord temp_rec;
    bool hit_anything = false;
    double closest_so_far = t_max;

    for (const auto& shape : shapes_) {
        if (shape->hit(ray, t_min, closest_so_far, temp_rec)) {
            hit_anything = true;
            closest_so_far = temp_rec.t;
            rec = temp_rec;
        }
    }

    return hit_anything;
}

} // namespace nwave
