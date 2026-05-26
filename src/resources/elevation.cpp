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

float resources::Elevation::GetElevation(
    std::vector<Elevation> const& elevation_samples, uint32_t elevation_width,
    uint32_t elevation_height, glm::vec3 const& position) {
  if (elevation_width == 0 || elevation_height == 0 ||
      elevation_samples.empty()) {
    return 0.0F;
  }

  size_t const expected_sample_count = static_cast<size_t>(elevation_width) *
                                       static_cast<size_t>(elevation_height);
  if (elevation_samples.size() < expected_sample_count) {
    return 0.0F;
  }

  glm::vec3 const& bounds_min = elevation_samples.front().position;
  glm::vec3 const& bounds_max =
      elevation_samples[expected_sample_count - 1].position;

  glm::vec2 const bounds_min_xz{bounds_min[0], bounds_min[2]};
  glm::vec2 const bounds_max_xz{bounds_max[0], bounds_max[2]};
  glm::vec2 const bounds_extent = bounds_max_xz - bounds_min_xz;

  if (std::abs(bounds_extent[0]) < 1e-6F ||
      std::abs(bounds_extent[1]) < 1e-6F) {
    return elevation_samples.front().elevation;
  }

  glm::vec2 sample_uv = glm::vec2{position[0], position[2]} - bounds_min_xz;
  sample_uv /= bounds_extent;
  sample_uv = glm::clamp(sample_uv, glm::vec2{0.0F}, glm::vec2{1.0F});

  float const scaled_x = sample_uv[0] * static_cast<float>(std::max<uint32_t>(
                                            1U, elevation_width - 1U));
  float const scaled_y = sample_uv[1] * static_cast<float>(std::max<uint32_t>(
                                            1U, elevation_height - 1U));

  auto sample_elevation = [&elevation_samples, elevation_width](
                              uint32_t column_index, uint32_t row_index) {
    size_t const row_offset =
        static_cast<size_t>(row_index) * static_cast<size_t>(elevation_width);
    size_t const sample_index = row_offset + static_cast<size_t>(column_index);
    return elevation_samples[sample_index].elevation;
  };

  uint32_t const column_index_0 = std::min(
      static_cast<uint32_t>(std::floor(scaled_x)), elevation_width - 1U);
  uint32_t const row_index_0 = std::min(
      static_cast<uint32_t>(std::floor(scaled_y)), elevation_height - 1U);
  uint32_t const column_index_1 =
      std::min(column_index_0 + 1U, elevation_width - 1U);
  uint32_t const row_index_1 =
      std::min(row_index_0 + 1U, elevation_height - 1U);

  float const x_fraction = scaled_x - static_cast<float>(column_index_0);
  float const y_fraction = scaled_y - static_cast<float>(row_index_0);

  float const elevation00 = sample_elevation(column_index_0, row_index_0);
  float const elevation10 = sample_elevation(column_index_1, row_index_0);
  float const elevation01 = sample_elevation(column_index_0, row_index_1);
  float const elevation11 = sample_elevation(column_index_1, row_index_1);

  return std::lerp(std::lerp(elevation00, elevation10, x_fraction),
                   std::lerp(elevation01, elevation11, x_fraction), y_fraction);
}
