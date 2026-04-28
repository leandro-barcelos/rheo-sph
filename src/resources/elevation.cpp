#include "elevation.h"

vk::VertexInputBindingDescription
resources::Elevation::GetBindingDescription() {
  return {.binding = 0,
          .stride = sizeof(Elevation),
          .inputRate = vk::VertexInputRate::eVertex};
}

std::vector<vk::VertexInputAttributeDescription>
resources::Elevation::GetAttributeDescriptions() {
  return {
      vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat,
                                          offsetof(Elevation, uv)),
      vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32Sfloat,
                                          offsetof(Elevation, elevation)),
      vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32B32Sfloat,
                                          offsetof(Elevation, position)),
  };
}
