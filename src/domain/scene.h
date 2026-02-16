#ifndef NWAVE_DOMAIN_SCENE_H
#define NWAVE_DOMAIN_SCENE_H

#include "core/ray.h"
#include "domain/hit_record.h"
#include "domain/shapes/shape.h"
#include "domain/lights/light.h"
#include <memory>
#include <vector>

namespace nwave {

class Scene {
public:
    void add_shape(std::shared_ptr<Shape> shape);
    void add_light(std::shared_ptr<Light> light);

    bool hit(const Ray& ray, double t_min, double t_max, HitRecord& rec) const;

    const std::vector<std::shared_ptr<Shape>>& shapes() const { return shapes_; }
    const std::vector<std::shared_ptr<Light>>& lights() const { return lights_; }

private:
    std::vector<std::shared_ptr<Shape>> shapes_;
    std::vector<std::shared_ptr<Light>> lights_;
};

} // namespace nwave

#endif // NWAVE_DOMAIN_SCENE_H
