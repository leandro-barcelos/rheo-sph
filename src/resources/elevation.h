#ifndef RHEOSPH_ELEVATION_H
#define RHEOSPH_ELEVATION_H

#include <glm/glm.hpp>

namespace resources {

struct Elevation {
  glm::vec2 uv;
  float elevation;
} __attribute__((aligned(16)));

}  // namespace resources

#endif  // !RHEOSPH_ELEVATION_H
