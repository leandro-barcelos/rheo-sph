#ifndef RHEOSPH_PIPELINE_H
#define RHEOSPH_PIPELINE_H

#include <vulkan/vulkan_raii.hpp>

#include "vulkan_device.h"
#include "vulkan_swap_chain.h"

namespace core {

class PipelineBuilder {
 public:
  [[nodiscard]] static vk::raii::Pipeline Compute(
      core::VulkanDevice const& vulkan_device,
      vk::raii::PipelineLayout const& pipeline_layout,
      std::string const& shader_filename, const char* name);
  [[nodiscard]] static vk::raii::Pipeline Graphics(
      core::VulkanDevice const& vulkan_device,
      vk::VertexInputBindingDescription binding_description,
      std::vector<vk::VertexInputAttributeDescription> const&
          attribute_descriptions,
      vk::raii::PipelineLayout const& pipeline_layout,
      core::VulkanSwapChain const& swap_chain,
      std::string const& shader_filename);
};

}  // namespace core

#endif  // !RHEOSPH_PIPELINE_H
