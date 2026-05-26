#ifndef RHEOSPH_ELEVATION_H
#define RHEOSPH_ELEVATION_H

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace resources {

struct Elevation {  // NOLINT(altera-struct-pack-align)
  glm::vec2 uv;
  float elevation;
  float pad0;
  glm::vec3 position;
  float pad1;

  static vk::VertexInputBindingDescription GetBindingDescription();
  static std::vector<vk::VertexInputAttributeDescription>
  GetAttributeDescriptions();

  [[nodiscard]] static float GetElevation(
      std::vector<Elevation> const& elevation_samples, uint32_t elevation_width,
      uint32_t elevation_height, glm::vec3 const& position);
} __attribute__((aligned(16)));

}  // namespace resources

#endif  // !RHEOSPH_ELEVATION_H
