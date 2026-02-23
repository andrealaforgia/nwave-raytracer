#ifndef NWAVE_DOMAIN_ANIMATION_CONFIG_H
#define NWAVE_DOMAIN_ANIMATION_CONFIG_H

#include <cmath>
#include <string>

namespace nwave {

struct FinaleConfig {
    bool enabled = false;
    int start_frame = -1;
    std::string earth_texture_path;
    std::string moon_texture_path;
    double earth_radius = 1.5;
    double earth_tilt_degrees = 23.5;
    double moon_radius_ratio = 0.25;
};

struct AnimationConfig {
    double duration;
    double physics_timestep;
    double render_fps;
    std::string output_directory = "frames/";
    int wake_frame = -1; // frame at which all sleeping bodies are woken (-1 = disabled)
    FinaleConfig finale;

    int total_frames() const {
        return static_cast<int>(std::ceil(duration * render_fps));
    }

    double render_dt() const {
        return 1.0 / render_fps;
    }

    int steps_per_frame() const {
        return static_cast<int>(std::round(render_dt() / physics_timestep));
    }
};

} // namespace nwave

#endif // NWAVE_DOMAIN_ANIMATION_CONFIG_H
