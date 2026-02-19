#include "application/animation_renderer.h"
#include "domain/shapes/transformed_shape.h"
#include "domain/shapes/deformable_mesh.h"
#include "domain/shapes/sphere.h"
#include "domain/shapes/box.h"
#include "domain/shapes/plane.h"
#include "domain/shapes/cylinder.h"
#include "domain/soft_body_mesh_data.h"
#include "core/matrix4x4.h"
#include <iomanip>
#include <sstream>
#include <cmath>

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
        auto box_min = box->box_min();
        auto box_max = box->box_max();
        desc.dimensions = Vec3(
            (box_max.x() - box_min.x()) / 2.0,
            (box_max.y() - box_min.y()) / 2.0,
            (box_max.z() - box_min.z()) / 2.0);
        desc.position = Point3(
            (box_min.x() + box_max.x()) / 2.0,
            (box_min.y() + box_max.y()) / 2.0,
            (box_min.z() + box_max.z()) / 2.0);
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

bool is_movable_body(BodyType body_type) {
    return body_type == BodyType::DYNAMIC || body_type == BodyType::KINEMATIC;
}

Matrix4x4 build_transform(const PhysicsTransform& t, bool translation_only) {
    if (translation_only) {
        return Matrix4x4::from_translation(t.position);
    }
    return Matrix4x4::from_translation_rotation(t.position, t.rotation);
}

} // namespace

AnimationRenderer::AnimationRenderer(const AnimationConfig& config,
                                     const Scene& scene,
                                     const std::vector<PhysicsProperties>& shape_physics,
                                     std::unique_ptr<PhysicsSimulator> physics,
                                     const Camera& camera,
                                     WriteCallback write_callback,
                                     ProgressReporter* progress,
                                     std::vector<SoftBodyDesc> soft_body_descs)
    : config_(config)
    , scene_(scene)
    , shape_physics_(shape_physics)
    , physics_(std::move(physics))
    , camera_(camera)
    , write_callback_(std::move(write_callback))
    , progress_(progress)
    , soft_body_descs_(std::move(soft_body_descs)) {}

int AnimationRenderer::render() {
    const auto& original_shapes = scene_.shapes();
    int shape_count = static_cast<int>(original_shapes.size());

    // Add all shapes as physics bodies and wrap dynamic ones in TransformedShape
    std::vector<int> body_ids(shape_count);
    std::vector<std::shared_ptr<TransformedShape>> transformed_shapes(shape_count);
    std::vector<bool> is_dynamic(shape_count, false);
    std::vector<bool> is_sphere(shape_count, false);

    for (int i = 0; i < shape_count; ++i) {
        PhysicsProperties props;
        if (i < static_cast<int>(shape_physics_.size())) {
            props = shape_physics_[i];
        }

        PhysicsBodyDesc desc = map_shape_to_body_desc(original_shapes[i].get(), props);
        body_ids[i] = physics_->add_body(desc);

        if (is_movable_body(props.body_type)) {
            is_dynamic[i] = true;
            is_sphere[i] = dynamic_cast<const Sphere*>(original_shapes[i].get()) != nullptr;
            transformed_shapes[i] = std::make_shared<TransformedShape>(
                original_shapes[i], Matrix4x4::identity());
        }
    }

    // Capture initial physics transforms so we can compute relative deltas.
    // The inner shapes are defined in world space, so the TransformedShape must
    // encode only the CHANGE from the initial position (identity at frame 0).
    // For spheres, rotation is mathematically invisible (rotational symmetry),
    // so we capture translation-only to avoid unnecessary computation.
    std::vector<Matrix4x4> initial_inv(shape_count);
    for (int i = 0; i < shape_count; ++i) {
        if (is_dynamic[i]) {
            PhysicsTransform init_t = physics_->get_transform(body_ids[i]);
            Matrix4x4 init_mat = build_transform(init_t, is_sphere[i]);
            initial_inv[i] = init_mat.inverse();
        }
    }

    // Setup soft bodies
    std::vector<std::shared_ptr<DeformableMesh>> soft_meshes;
    std::vector<int> soft_body_ids;
    for (const auto& sb_desc : soft_body_descs_) {
        int sb_id = physics_->add_soft_body(sb_desc);
        soft_body_ids.push_back(sb_id);

        SoftBodyMeshData initial_mesh = physics_->get_soft_body_mesh(sb_id);

        auto mesh = std::make_shared<DeformableMesh>(
            initial_mesh.face_indices, sb_desc.material);
        mesh->update_vertices(initial_mesh.vertices);

        soft_meshes.push_back(mesh);
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

    // Add soft body meshes to the scene
    for (const auto& mesh : soft_meshes) {
        anim_scene.add_shape(mesh);
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
        // Update transforms for dynamic shapes using relative delta.
        // For spheres, apply translation only (rotation is invisible due to symmetry).
        for (int i = 0; i < shape_count; ++i) {
            if (is_dynamic[i]) {
                PhysicsTransform phys_transform = physics_->get_transform(body_ids[i]);
                Matrix4x4 current = build_transform(phys_transform, is_sphere[i]);
                // Relative transform: current * inverse(initial)
                // At frame 0 this is identity, so shapes render at original positions
                Matrix4x4 relative = current * initial_inv[i];
                transformed_shapes[i]->set_transform(relative);
            }
        }

        // Update soft body meshes from physics
        for (size_t soft_idx = 0; soft_idx < soft_meshes.size(); ++soft_idx) {
            SoftBodyMeshData mesh_data = physics_->get_soft_body_mesh(soft_body_ids[soft_idx]);
            soft_meshes[soft_idx]->update_vertices(std::move(mesh_data.vertices));
        }

        // Animate camera: slow counterclockwise rotation + lower over duration
        double t = (total_frames > 1) ? static_cast<double>(frame) / (total_frames - 1) : 0.0;
        Point3 base_from = camera_.lookfrom();
        Point3 target = camera_.lookat();
        double dx = base_from.x() - target.x();
        double dz = base_from.z() - target.z();
        double radius = std::sqrt(dx * dx + dz * dz);
        double base_angle = std::atan2(dx, dz);
        double rotation_amount = M_PI / 3.0;  // 60 degrees counterclockwise
        double height_drop = 0.8;
        double angle = base_angle + t * rotation_amount;
        Point3 new_from(
            target.x() + radius * std::sin(angle),
            base_from.y() - t * height_drop,
            target.z() + radius * std::cos(angle));
        Camera frame_camera(new_from, target, camera_.vup(),
                            camera_.vfov(), camera_.aspect_ratio(),
                            camera_.image_width());

        // Render frame
        std::string filename = frame_filename(config_.output_directory, frame);
        write_callback_(filename, anim_scene, frame_camera, frame_camera.image_width(), 1);

        if (progress_) {
            progress_->frame_complete(frame + 1);
        }

        // Step physics AFTER rendering so frame 0 shows the undisturbed scene
        for (int step = 0; step < steps_per_frame; ++step) {
            physics_->step(physics_dt);
        }

        // Wake all sleeping bodies at the configured frame (ensures all blocks fall)
        if (frame == config_.wake_frame) {
            physics_->wake_all();
        }

        // Per-body wake: activate individual bodies at their specified wake frame
        for (int i = 0; i < shape_count; ++i) {
            if (i < static_cast<int>(shape_physics_.size()) &&
                shape_physics_[i].wake_frame.has_value() &&
                shape_physics_[i].wake_frame.value() == frame) {
                physics_->wake_body(body_ids[i]);
            }
        }
    }

    if (progress_) {
        progress_->finish();
    }

    return total_frames;
}

} // namespace nwave
