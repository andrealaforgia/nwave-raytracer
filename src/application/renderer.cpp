#include "application/renderer.h"
#include "core/math_utils.h"
#include "domain/materials/material.h"
#include <algorithm>
#include <cmath>
#include <thread>
#include <iostream>

namespace nwave {

void Renderer::render_scanlines(int y_start, int y_end, int width,
                                 const Camera& camera, const Scene& scene,
                                 const RenderSettings& settings,
                                 std::vector<Color3>& pixels) const {
    for (int y = y_start; y < y_end; ++y) {
        for (int x = 0; x < width; ++x) {
            Color3 color(0, 0, 0);
            for (int s = 0; s < settings.samples_per_pixel; ++s) {
                Ray ray = (settings.samples_per_pixel > 1)
                    ? camera.generate_ray_random(x, y)
                    : camera.generate_ray(x, y);
                color += trace_ray(ray, scene, settings, settings.max_depth);
            }
            color /= static_cast<double>(settings.samples_per_pixel);

            // Gamma correction (gamma 2.0)
            double r = std::sqrt(clamp(color.r(), 0.0, 1.0));
            double g = std::sqrt(clamp(color.g(), 0.0, 1.0));
            double b = std::sqrt(clamp(color.b(), 0.0, 1.0));

            pixels[y * width + x] = Color3(r, g, b);
        }
    }
}

std::vector<Color3> Renderer::render(const Camera& camera, const Scene& scene,
                                      const RenderSettings& settings) const {
    int width = camera.image_width();
    int height = camera.image_height();
    std::vector<Color3> pixels(width * height);

    int num_threads = settings.num_threads;
    if (num_threads <= 0) {
        num_threads = static_cast<int>(std::thread::hardware_concurrency());
        if (num_threads <= 0) num_threads = 4;
    }

    if (num_threads == 1) {
        render_scanlines(0, height, width, camera, scene, settings, pixels);
        return pixels;
    }

    if (!quiet_) {
        std::cout << "Using " << num_threads << " threads\n";
    }

    std::vector<std::thread> threads;
    int rows_per_thread = height / num_threads;
    int remainder = height % num_threads;

    int y_start = 0;
    for (int t = 0; t < num_threads; ++t) {
        int y_end = y_start + rows_per_thread + (t < remainder ? 1 : 0);
        threads.emplace_back(&Renderer::render_scanlines, this,
                             y_start, y_end, width,
                             std::cref(camera), std::cref(scene),
                             std::cref(settings), std::ref(pixels));
        y_start = y_end;
    }

    for (auto& t : threads) {
        t.join();
    }

    return pixels;
}

Color3 Renderer::trace_ray(const Ray& ray, const Scene& scene,
                            const RenderSettings& settings, int depth) const {
    if (depth <= 0) {
        return Color3(0, 0, 0);
    }

    HitRecord rec;
    if (scene.hit(ray, epsilon, infinity, rec)) {
        Color3 color(0, 0, 0);

        // Emitted light from material (e.g., neon)
        if (rec.material != nullptr) {
            color += rec.material->emitted();
        }

        // Get material attenuation (albedo)
        Color3 attenuation;
        Ray scattered;
        bool has_material = rec.material != nullptr
                            && rec.material->scatter(ray, rec, attenuation, scattered);

        if (has_material) {
            // Small ambient term
            color += 0.05 * attenuation;
        }

        // Direct illumination from each light
        for (const auto& light : scene.lights()) {
            LightSample light_sample = light->sample(rec.point);
            Point3 shadow_origin = rec.point + epsilon * rec.normal;
            Ray shadow_ray(shadow_origin, light_sample.direction);

            HitRecord shadow_rec;
            if (!scene.hit(shadow_ray, epsilon, light_sample.distance - epsilon, shadow_rec)) {
                double diffuse_factor = std::max(0.0, dot(rec.normal, light_sample.direction));
                if (has_material) {
                    color += attenuation * light_sample.intensity * diffuse_factor;
                } else {
                    color += light_sample.intensity * diffuse_factor;
                }
            }
        }

        // Indirect illumination: recursively trace scattered ray
        if (has_material) {
            Color3 indirect = trace_ray(scattered, scene, settings, depth - 1);
            color += attenuation * indirect;
        }

        return color;
    }

    // Sky gradient for miss
    Vec3 unit_direction = normalize(ray.direction());
    double a = 0.5 * (unit_direction.y() + 1.0);
    return (1.0 - a) * settings.background_bottom + a * settings.background_top;
}

} // namespace nwave
