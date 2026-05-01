#ifndef RHEOSPH_PIPELINE_H
#define RHEOSPH_PIPELINE_H

#include <vulkan/vulkan_raii.hpp>

#include "vulkan_device.h"
#include "vulkan_swap_chain.h"

namespace core {

class PipelineBuilder {
 public:
    struct GraphicsOptions {
        vk::PrimitiveTopology topology = vk::PrimitiveTopology::ePointList;
        vk::PolygonMode polygon_mode = vk::PolygonMode::eFill;
        vk::CullModeFlags cull_mode = vk::CullModeFlagBits::eBack;
        vk::FrontFace front_face = vk::FrontFace::eCounterClockwise;
        bool enable_blending = true;
        bool depth_test_enable = false;
        bool depth_write_enable = false;
    } __attribute__((aligned(32))) __attribute__((packed));

  [[nodiscard]] static vk::raii::Pipeline Compute(
      core::VulkanDevice const& vulkan_device,
      vk::raii::PipelineLayout const& pipeline_layout,
      std::string const& shader_filename,
      const char* name = "comp_main");
  [[nodiscard]] static vk::raii::Pipeline Graphics(
      core::VulkanDevice const& vulkan_device,
      vk::VertexInputBindingDescription binding_description,
      std::vector<vk::VertexInputAttributeDescription> const&
          attribute_descriptions,
      vk::raii::PipelineLayout const& pipeline_layout,
      core::VulkanSwapChain const& swap_chain,
      std::string const& shader_filename);
  [[nodiscard]] static vk::raii::Pipeline Graphics(
      core::VulkanDevice const& vulkan_device,
      vk::VertexInputBindingDescription binding_description,
      std::vector<vk::VertexInputAttributeDescription> const&
          attribute_descriptions,
      vk::raii::PipelineLayout const& pipeline_layout,
      core::VulkanSwapChain const& swap_chain,
      std::string const& shader_filename,
      GraphicsOptions options);
};

}  // namespace core

#endif  // !RHEOSPH_PIPELINE_H
