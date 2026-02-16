#ifndef NWAVE_INFRASTRUCTURE_PPM_WRITER_H
#define NWAVE_INFRASTRUCTURE_PPM_WRITER_H

#include "core/vec3.h"
#include <string>
#include <vector>

namespace nwave {

void write_ppm(const std::string& filename, const std::vector<Color3>& pixels,
               int width, int height);

} // namespace nwave

#endif // NWAVE_INFRASTRUCTURE_PPM_WRITER_H
