#include "application/animation_renderer.h"
#include "domain/shapes/transformed_shape.h"
#include "domain/shapes/sphere.h"
#include "domain/shapes/box.h"
#include "domain/shapes/plane.h"
#include "domain/shapes/cylinder.h"
#include "core/matrix4x4.h"
#include <iomanip>
#include <sstream>

namespace nwave {

namespace {

PhysicsBodyDesc map_shape_to_body_desc(const Shape* shape, const PhysicsProperties& props) {
    PhysicsBodyDesc desc;
    desc.properties = props;

    if (auto* sphere = dynamic_cast<const Sphere*>(shape)) {
        desc.shape_type = PhysicsShapeType::SPHERE;
        double r = sphere->radius();
        desc.dimensions = Vec3(r, r, r);
        desc.position = sphere->center();
    } else if (auto* box = dynamic_cast<const Box*>(shape)) {
        desc.shape_type = PhysicsShapeType::BOX;
        auto bmin = box->box_min();
        auto bmax = box->box_max();
        double hx = (bmax.x() - bmin.x()) / 2.0;
        double hy = (bmax.y() - bmin.y()) / 2.0;
        double hz = (bmax.z() - bmin.z()) / 2.0;
        desc.dimensions = Vec3(hx, hy, hz);
        desc.position = Point3((bmin.x() + bmax.x()) / 2.0,
                               (bmin.y() + bmax.y()) / 2.0,
                               (bmin.z() + bmax.z()) / 2.0);
    } else if (auto* plane = dynamic_cast<const Plane*>(shape)) {
        desc.shape_type = PhysicsShapeType::PLANE;
        desc.position = plane->point();
        desc.dimensions = plane->normal();
    } else if (auto* cyl = dynamic_cast<const Cylinder*>(shape)) {
        desc.shape_type = PhysicsShapeType::CYLINDER;
        desc.dimensions = Vec3(cyl->radius(), cyl->height() / 2.0, cyl->radius());
        desc.position = Point3(cyl->center().x(),
                               cyl->center().y() + cyl->height() / 2.0,
                               cyl->center().z());
    } else {
        // Default: treat as box with unit dimensions
        desc.shape_type = PhysicsShapeType::BOX;
        desc.dimensions = Vec3(0.5, 0.5, 0.5);
    }

    return desc;
}

std::string frame_filename(const std::string& output_dir, int frame_index) {
    std::ostringstream oss;
    oss << output_dir << "frame_" << std::setw(4) << std::setfill('0') << frame_index << ".ppm";
    return oss.str();
}

} // namespace

AnimationRenderer::AnimationRenderer(const AnimationConfig& config,
                                     const Scene& scene,
                                     const std::vector<PhysicsProperties>& shape_physics,
                                     std::unique_ptr<PhysicsSimulator> physics,
                                     const Camera& camera,
                                     WriteCallback write_callback,
                                     ProgressReporter* progress)
    : config_(config)
    , scene_(scene)
    , shape_physics_(shape_physics)
    , physics_(std::move(physics))
    , camera_(camera)
    , write_callback_(std::move(write_callback))
    , progress_(progress) {}

int AnimationRenderer::render() {
    const auto& original_shapes = scene_.shapes();
    int shape_count = static_cast<int>(original_shapes.size());

    // Add all shapes as physics bodies and wrap dynamic ones in TransformedShape
    std::vector<int> body_ids(shape_count);
    std::vector<std::shared_ptr<TransformedShape>> transformed_shapes(shape_count);
    std::vector<bool> is_dynamic(shape_count, false);

    for (int i = 0; i < shape_count; ++i) {
        PhysicsProperties props;
        if (i < static_cast<int>(shape_physics_.size())) {
            props = shape_physics_[i];
        }

        PhysicsBodyDesc desc = map_shape_to_body_desc(original_shapes[i].get(), props);
        body_ids[i] = physics_->add_body(desc);

        if (props.body_type == BodyType::DYNAMIC || props.body_type == BodyType::KINEMATIC) {
            is_dynamic[i] = true;
            transformed_shapes[i] = std::make_shared<TransformedShape>(
                original_shapes[i], Matrix4x4::identity());
        }
    }

    // Build the animation scene: replace dynamic shapes with their TransformedShape wrappers
    Scene anim_scene;
    for (int i = 0; i < shape_count; ++i) {
        if (is_dynamic[i]) {
            anim_scene.add_shape(transformed_shapes[i]);
        } else {
            anim_scene.add_shape(original_shapes[i]);
        }
    }

    // Copy lights from original scene
    for (const auto& light : scene_.lights()) {
        anim_scene.add_light(light);
    }

    int total_frames = config_.total_frames();
    int steps_per_frame = config_.steps_per_frame();
    double physics_dt = config_.physics_timestep;

    if (progress_) {
        progress_->start(total_frames);
    }

    for (int frame = 0; frame < total_frames; ++frame) {
        // Step physics
        for (int step = 0; step < steps_per_frame; ++step) {
            physics_->step(physics_dt);
        }

        // Update transforms for dynamic shapes
        for (int i = 0; i < shape_count; ++i) {
            if (is_dynamic[i]) {
                PhysicsTransform phys_transform = physics_->get_transform(body_ids[i]);
                Matrix4x4 mat = Matrix4x4::from_translation_rotation(
                    phys_transform.position, phys_transform.rotation);
                transformed_shapes[i]->set_transform(mat);
            }
        }

        // Invoke write callback
        std::string filename = frame_filename(config_.output_directory, frame);
        write_callback_(filename, anim_scene, camera_, camera_.image_width(), 1);

        if (progress_) {
            progress_->frame_complete(frame + 1);
        }
    }

    if (progress_) {
        progress_->finish();
    }

    return total_frames;
}

} // namespace nwave
