#include "application/animation_renderer.h"
#include "domain/shapes/transformed_shape.h"
#include "domain/shapes/deformable_mesh.h"
#include "domain/shapes/sphere.h"
#include "domain/shapes/box.h"
#include "domain/shapes/plane.h"
#include "domain/shapes/cylinder.h"
#include "domain/soft_body_mesh_data.h"
#include "domain/materials/image_texture.h"
#include "domain/materials/lambertian.h"
#include "domain/lights/directional_light.h"
#include "core/matrix4x4.h"
#include <iomanip>
#include <sstream>
#include <cmath>
#include <thread>
#include <chrono>
#include <iostream>

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

bool should_wake_at_frame(const PhysicsProperties& props, int frame) {
    return props.wake_frame.has_value() && props.wake_frame.value() == frame;
}

Matrix4x4 build_transform(const PhysicsTransform& t) {
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

    for (int i = 0; i < shape_count; ++i) {
        PhysicsProperties props;
        if (i < static_cast<int>(shape_physics_.size())) {
            props = shape_physics_[i];
        }

        PhysicsBodyDesc desc = map_shape_to_body_desc(original_shapes[i].get(), props);
        body_ids[i] = physics_->add_body(desc);

        if (is_movable_body(props.body_type)) {
            is_dynamic[i] = true;
        }
        // Wrap ALL shapes so the finale can animate them falling
        transformed_shapes[i] = std::make_shared<TransformedShape>(
            original_shapes[i], Matrix4x4::identity());
    }

    // Capture initial physics transforms so we can compute relative deltas.
    std::vector<Matrix4x4> initial_inv(shape_count);
    for (int i = 0; i < shape_count; ++i) {
        PhysicsTransform init_t = physics_->get_transform(body_ids[i]);
        Matrix4x4 init_mat = build_transform(init_t);
        initial_inv[i] = init_mat.inverse();
    }

    // Setup soft bodies (only immediate ones; deferred ones created at start_frame)
    std::vector<std::shared_ptr<DeformableMesh>> soft_meshes;
    std::vector<int> soft_body_ids;
    std::vector<bool> soft_body_created(soft_body_descs_.size(), false);
    for (size_t si = 0; si < soft_body_descs_.size(); ++si) {
        if (soft_body_descs_[si].start_frame > 0) {
            soft_body_ids.push_back(-1);
            soft_meshes.push_back(nullptr);
            continue;
        }
        int sb_id = physics_->add_soft_body(soft_body_descs_[si]);
        soft_body_ids.push_back(sb_id);

        SoftBodyMeshData initial_mesh = physics_->get_soft_body_mesh(sb_id);

        auto mesh = std::make_shared<DeformableMesh>(
            initial_mesh.face_indices, soft_body_descs_[si].material);
        mesh->update_vertices(initial_mesh.vertices);

        soft_meshes.push_back(mesh);
        soft_body_created[si] = true;
    }

    // Build the animation scene: all shapes use TransformedShape wrappers
    Scene anim_scene;
    for (int i = 0; i < shape_count; ++i) {
        anim_scene.add_shape(transformed_shapes[i]);
    }

    // Add immediate soft body meshes to the scene
    for (const auto& mesh : soft_meshes) {
        if (mesh) anim_scene.add_shape(mesh);
    }

    // Bowling phase: two directional lights at different heights + ambient
    // Scene centre is roughly (0, 0.3, 0.5)
    // High light: from upper-left, steep angle
    anim_scene.clear_lights();
    anim_scene.add_light(std::make_shared<DirectionalLight>(
        normalize(Vec3(0.4, -1.0, -0.6)), Color3(1.0, 0.97, 0.9), 1.5));
    // Low light: from right, shallow angle for long shadows
    anim_scene.add_light(std::make_shared<DirectionalLight>(
        normalize(Vec3(-0.5, -0.3, 0.4)), Color3(0.9, 0.95, 1.0), 1.0));

    // Identify sweeper body by name
    int sweeper_index = -1;
    bool sweeper_made_dynamic = false;
    for (int i = 0; i < static_cast<int>(shape_physics_.size()); ++i) {
        if (shape_physics_[i].name == "sweeper_prism") sweeper_index = i;
    }

    int total_frames = config_.total_frames();
    int steps_per_frame = config_.steps_per_frame();
    double physics_dt = config_.physics_timestep;

    // Determine finale frame range
    int finale_start = config_.finale.enabled ? config_.finale.start_frame : total_frames;
    int main_frames = std::min(total_frames, finale_start);

    if (progress_) {
        progress_->start(total_frames);
    }

    // Determine SPP from config (will be set via RenderSettings per frame)
    // The actual SPP comes from the command line via the write_callback closure,
    // but we create default settings here. The write_callback receives them.

    for (int frame = 0; frame < main_frames; ++frame) {
        // Update transforms for dynamic shapes using relative delta.
        for (int i = 0; i < shape_count; ++i) {
            if (is_dynamic[i]) {
                PhysicsTransform phys_transform = physics_->get_transform(body_ids[i]);
                Matrix4x4 current = build_transform(phys_transform);
                Matrix4x4 relative = current * initial_inv[i];
                transformed_shapes[i]->set_transform(relative);
            }
        }

        // Update soft body meshes from physics
        for (size_t soft_idx = 0; soft_idx < soft_meshes.size(); ++soft_idx) {
            if (!soft_meshes[soft_idx]) continue;
            SoftBodyMeshData mesh_data = physics_->get_soft_body_mesh(soft_body_ids[soft_idx]);
            soft_meshes[soft_idx]->update_vertices(std::move(mesh_data.vertices));
        }

        // Animate camera: two phases
        double t = (total_frames > 1) ? static_cast<double>(frame) / (total_frames - 1) : 0.0;
        Point3 base_from = camera_.lookfrom();
        Point3 target = camera_.lookat();
        double dx = base_from.x() - target.x();
        double dz = base_from.z() - target.z();
        double radius = std::sqrt(dx * dx + dz * dz);
        double base_angle = std::atan2(dx, dz);
        double height_drop = 0.8;

        int sweep_start = config_.wake_frame;
        double pre_sweep_rotation = M_PI / 12.0;
        double sweep_rotation = -M_PI / 3.0;

        double angle;
        if (frame < sweep_start || sweep_start <= 0) {
            double pre_t = (sweep_start > 0) ? static_cast<double>(frame) / sweep_start : t;
            angle = base_angle + pre_t * pre_sweep_rotation;
        } else {
            double sweep_t = static_cast<double>(frame - sweep_start) / (total_frames - 1 - sweep_start);
            angle = base_angle + pre_sweep_rotation + sweep_t * sweep_rotation;
        }

        Point3 new_from(
            target.x() + radius * std::sin(angle),
            base_from.y() - t * height_drop,
            target.z() + radius * std::cos(angle));
        Camera frame_camera(new_from, target, camera_.vup(),
                            camera_.vfov(), camera_.aspect_ratio(),
                            camera_.image_width());

        // Build default RenderSettings for main animation frames
        // Bowling phase uses bright ambient-only lighting (no directional/point lights)
        RenderSettings frame_settings;
        frame_settings.samples_per_pixel = 1; // placeholder; actual SPP set by CLI via main.cpp
        frame_settings.max_depth = 10;
        frame_settings.ambient_factor = 0.45f;
        // Light blue sky background
        frame_settings.background_top = Color3(0.35, 0.55, 0.85);
        frame_settings.background_bottom = Color3(0.65, 0.75, 0.90);

        // Render frame
        std::string filename = frame_filename(config_.output_directory, frame);
        write_callback_(filename, anim_scene, frame_camera, frame_camera.image_width(), frame_settings);

        if (progress_) {
            progress_->frame_complete(frame + 1);
        }

        // Inter-frame delay to prevent GPU thermal overload
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Step physics AFTER rendering so frame 0 shows the undisturbed scene
        for (int step = 0; step < steps_per_frame; ++step) {
            physics_->step(physics_dt);
        }

        // Wake sleeping bodies at the configured global frame
        if (frame == config_.wake_frame) {
            for (int i = 0; i < shape_count; ++i) {
                if (i >= static_cast<int>(shape_physics_.size()) ||
                    !shape_physics_[i].wake_frame.has_value()) {
                    physics_->wake_body(body_ids[i]);
                }
            }
        }

        // Per-body wake
        for (int i = 0; i < shape_count; ++i) {
            if (i < static_cast<int>(shape_physics_.size()) &&
                should_wake_at_frame(shape_physics_[i], frame)) {
                physics_->wake_body(body_ids[i]);
                const auto& vel = shape_physics_[i].initial_velocity;
                if (vel.x() != 0.0 || vel.y() != 0.0 || vel.z() != 0.0) {
                    physics_->set_linear_velocity(body_ids[i], vel);
                }
                const auto& ang_vel = shape_physics_[i].initial_angular_velocity;
                if (ang_vel.x() != 0.0 || ang_vel.y() != 0.0 || ang_vel.z() != 0.0) {
                    physics_->set_angular_velocity(body_ids[i], ang_vel);
                }
            }
        }

        // Sweeper fall-off
        if (sweeper_index >= 0 && !sweeper_made_dynamic) {
            PhysicsTransform sweeper_t = physics_->get_transform(body_ids[sweeper_index]);
            if (sweeper_t.position.z() < -4.0) {
                physics_->set_motion_type(body_ids[sweeper_index], BodyType::DYNAMIC);
                sweeper_made_dynamic = true;
            }
        }

        // Create deferred soft bodies at their start_frame
        for (size_t soft_idx = 0; soft_idx < soft_body_descs_.size(); ++soft_idx) {
            if (!soft_body_created[soft_idx] &&
                soft_body_descs_[soft_idx].start_frame == frame) {
                int sb_id = physics_->add_soft_body(soft_body_descs_[soft_idx]);
                soft_body_ids[soft_idx] = sb_id;

                SoftBodyMeshData initial_mesh = physics_->get_soft_body_mesh(sb_id);
                auto mesh = std::make_shared<DeformableMesh>(
                    initial_mesh.face_indices, soft_body_descs_[soft_idx].material);
                mesh->update_vertices(initial_mesh.vertices);

                soft_meshes[soft_idx] = mesh;
                anim_scene.add_shape(mesh);
                soft_body_created[soft_idx] = true;
            }
        }
    }

    // ===== EARTH FINALE =====
    if (config_.finale.enabled && finale_start < total_frames) {
        std::cerr << "[Finale] Starting Earth finale at frame " << finale_start
                  << " (" << (total_frames - finale_start) << " frames)\n";

        // Load Earth texture
        std::shared_ptr<Material> earth_material;
        if (!config_.finale.earth_texture_path.empty()) {
            earth_material = ImageTexture::load_from_file(config_.finale.earth_texture_path);
            if (!earth_material) {
                std::cerr << "[Finale] Warning: could not load Earth texture: "
                          << config_.finale.earth_texture_path << "\n";
                earth_material = std::make_shared<Lambertian>(Color3(0.2, 0.4, 0.8));
            }
        } else {
            earth_material = std::make_shared<Lambertian>(Color3(0.2, 0.4, 0.8));
        }

        // Moon material: load texture if available, fall back to grey lambertian
        std::shared_ptr<Material> moon_material;
        if (!config_.finale.moon_texture_path.empty()) {
            moon_material = ImageTexture::load_from_file(config_.finale.moon_texture_path);
            if (!moon_material) {
                std::cerr << "[Finale] Warning: could not load Moon texture: "
                          << config_.finale.moon_texture_path << "\n";
                moon_material = std::make_shared<Lambertian>(Color3(0.7, 0.7, 0.7));
            }
        } else {
            moon_material = std::make_shared<Lambertian>(Color3(0.7, 0.7, 0.7));
        }

        double earth_radius = config_.finale.earth_radius;
        double moon_radius = earth_radius * config_.finale.moon_radius_ratio;
        double orbit_radius = earth_radius * 4.0;
        double tilt_rad = config_.finale.earth_tilt_degrees * M_PI / 180.0;

        // Earth center position (slightly above ground)
        Point3 earth_center(0.0, earth_radius + 0.5, 0.0);

        // Create Earth sphere
        auto earth_sphere = std::make_shared<Sphere>(
            Point3(0, 0, 0), earth_radius, earth_material.get());
        auto earth_transformed = std::make_shared<TransformedShape>(
            earth_sphere, Matrix4x4::identity());

        // Create Moon sphere
        auto moon_sphere = std::make_shared<Sphere>(
            Point3(0, 0, 0), moon_radius, moon_material.get());
        auto moon_transformed = std::make_shared<TransformedShape>(
            moon_sphere, Matrix4x4::identity());

        // Add Earth and Moon to scene (alongside existing bowling objects initially)
        anim_scene.add_shape(earth_transformed);
        anim_scene.add_shape(moon_transformed);

        // Track which shapes are bowling objects (everything except Earth/Moon)
        // so we can animate them falling away
        int bowling_shape_count = static_cast<int>(anim_scene.shapes().size()) - 2;
        bool bowling_cleared = false;
        int fall_duration = 30; // 1 second at 30fps

        int finale_frames = total_frames - finale_start;

        // Camera starting position (from last main frame camera)
        Point3 cam_start = camera_.lookfrom();

        for (int frame = finale_start; frame < total_frames; ++frame) {
            int finale_idx = frame - finale_start;
            double finale_t = static_cast<double>(finale_idx) / std::max(1, finale_frames - 1);

            // Earth rotation: ~2.4 degrees per frame around tilted axis
            double earth_spin_angle = finale_idx * (2.0 * M_PI / finale_frames);

            // Combined transform: translate to earth_center, tilt, then spin
            Quaternion tilt_q = Quaternion::from_axis_angle(Vec3(0, 0, 1), tilt_rad);
            Quaternion spin_q = Quaternion::from_axis_angle(Vec3(0, 1, 0), earth_spin_angle);
            Quaternion earth_rot = tilt_q * spin_q;

            Matrix4x4 earth_mat = Matrix4x4::from_translation_rotation(
                Vec3(earth_center.x(), earth_center.y(), earth_center.z()), earth_rot);
            earth_transformed->set_transform(earth_mat);

            // Moon orbit: two full orbits over finale duration
            double moon_angle = -finale_idx * (2.0 * 2.0 * M_PI / finale_frames);
            Point3 moon_pos(
                earth_center.x() + orbit_radius * std::cos(moon_angle),
                earth_center.y(),
                earth_center.z() + orbit_radius * std::sin(moon_angle));

            // Moon self-rotation: same rate and direction as Earth's spin
            Quaternion moon_rot = Quaternion::from_axis_angle(
                Vec3(0, 1, 0), earth_spin_angle);
            Matrix4x4 moon_mat = Matrix4x4::from_translation_rotation(
                Vec3(moon_pos.x(), moon_pos.y(), moon_pos.z()), moon_rot);
            moon_transformed->set_transform(moon_mat);

            // Animate bowling objects falling away during transition
            if (!bowling_cleared && finale_idx < fall_duration) {
                // Accelerating downward drop (simulates gravity)
                double fall_t = static_cast<double>(finale_idx) / fall_duration;
                double drop = 50.0 * fall_t * fall_t; // quadratic ease-in
                for (int si = 0; si < shape_count; ++si) {
                    if (transformed_shapes[si]) {
                        PhysicsTransform phys_t = physics_->get_transform(body_ids[si]);
                        phys_t.position = Point3(
                            phys_t.position.x(),
                            phys_t.position.y() - drop,
                            phys_t.position.z());
                        Matrix4x4 cur = build_transform(phys_t);
                        Matrix4x4 rel = cur * initial_inv[si];
                        transformed_shapes[si]->set_transform(rel);
                    }
                }
            } else if (!bowling_cleared) {
                // After fall completes, remove bowling shapes for clean rendering
                anim_scene.clear_shapes();
                anim_scene.add_shape(earth_transformed);
                anim_scene.add_shape(moon_transformed);
                bowling_cleared = true;
            }

            // Camera: orbit slowly around Earth, looking at it
            // Distance must be large enough to keep the full Moon orbit visible
            // (orbit_radius = earth_radius * 4.0, so camera at ~10x earth_radius)
            double cam_orbit_angle = -finale_t * M_PI * 0.4; // Stay on lit side of Earth
            double cam_distance = earth_radius * 7.0;
            double cam_elevation = earth_center.y() + earth_radius * 2.5;

            // Smooth transition from bowling camera to Earth camera
            double blend = std::min(1.0, finale_idx / 30.0); // 1-second blend

            Point3 earth_cam_pos(
                earth_center.x() + cam_distance * std::sin(cam_orbit_angle),
                cam_elevation,
                earth_center.z() + cam_distance * std::cos(cam_orbit_angle));

            // Lerp camera position during transition
            Point3 cam_pos(
                cam_start.x() * (1.0 - blend) + earth_cam_pos.x() * blend,
                cam_start.y() * (1.0 - blend) + earth_cam_pos.y() * blend,
                cam_start.z() * (1.0 - blend) + earth_cam_pos.z() * blend);

            Camera frame_camera(cam_pos, earth_center, camera_.vup(),
                                camera_.vfov(), camera_.aspect_ratio(),
                                camera_.image_width());

            // Lighting: directional light from camera toward Earth
            anim_scene.clear_lights();
            // Light direction = normalized(earth_center - cam_pos), so the lit
            // hemisphere always faces the camera.
            Vec3 light_dir = normalize(earth_center - cam_pos);
            anim_scene.add_light(std::make_shared<DirectionalLight>(
                light_dir, Color3(1.0, 0.95, 0.85), 2.5));

            // Finale render settings: dark sky, directional light only (no ambient)
            RenderSettings finale_settings;
            finale_settings.samples_per_pixel = 1; // placeholder; actual SPP set by CLI
            finale_settings.max_depth = 10;
            finale_settings.ambient_factor = 0.02f;
            finale_settings.background_top = Color3(0.02, 0.02, 0.05);
            finale_settings.background_bottom = Color3(0.01, 0.01, 0.02);

            std::string filename = frame_filename(config_.output_directory, frame);
            write_callback_(filename, anim_scene, frame_camera, frame_camera.image_width(), finale_settings);

            if (progress_) {
                progress_->frame_complete(frame + 1);
            }

            // Inter-frame delay
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            // Step physics
            for (int step = 0; step < steps_per_frame; ++step) {
                physics_->step(physics_dt);
            }
        }
    }

    if (progress_) {
        progress_->finish();
    }

    return total_frames;
}

} // namespace nwave
