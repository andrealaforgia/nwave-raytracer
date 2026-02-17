#ifndef NWAVE_DOMAIN_ANIMATION_CONFIG_H
#define NWAVE_DOMAIN_ANIMATION_CONFIG_H

#include <cmath>
#include <string>

namespace nwave {

struct AnimationConfig {
    double duration;
    double physics_timestep;
    double render_fps;
    std::string output_directory = "frames/";

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
