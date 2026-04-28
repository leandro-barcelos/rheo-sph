#include "terrain_renderer.h"

#include <cstdint>

#include "../core/pipeline.h"
#include "rheo-sph/src/core/command_pool.h"
#include "rheo-sph/src/resources/buffer.h"
#include "rheo-sph/src/resources/elevation.h"
#include "vulkan/vulkan.hpp"

void renderer::TerrainRenderer::Init(
    core::VulkanDevice const& vulkan_device,
    core::VulkanSwapChain const& vulkan_swap_chain,
    core::CommandPools const& command_pools, uint32_t elevation_width,
    uint32_t elevation_height) {
  if (elevation_width < 2 || elevation_height < 2) {
    return;
  }

  CreateCameraDescriptorSetLayout(vulkan_device);
  const std::array camera_descriptor_pool_sizes = {
      resources::DescriptorAllocator::PoolSize{
          .type = vk::DescriptorType::eUniformBuffer, .count = 1}};
  camera_descriptor_allocator_.Init(vulkan_device, 1,
                                    camera_descriptor_pool_sizes);
  CreateGraphicsPipeline(vulkan_device, vulkan_swap_chain);
  CreateIndices(elevation_width, elevation_height);
  CreateBuffers(vulkan_device, command_pools);
  CreateCameraDescriptorSet(vulkan_device, camera_descriptor_allocator_);
  is_initialized_ = true;
}

void renderer::TerrainRenderer::Render(
    vk::raii::CommandBuffer const& command_buffer,
    core::VulkanSwapChain& vulkan_swap_chain,
    simulation::FluidSimulator const* fluid_simulator,
    renderer::Camera const& camera) {
  if (!is_initialized_) {
    return;
  }

  UpdateUniformBuffer(vulkan_swap_chain, camera);

  command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                              *graphics_pipeline_);
  command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                    *graphics_pipeline_layout_, 0,
                                    {*camera_descriptor_set_}, {});
  command_buffer.setViewport(
      0, vk::Viewport(0.0F, 0.0F,
                      static_cast<float>(vulkan_swap_chain.Extent().width),
                      static_cast<float>(vulkan_swap_chain.Extent().height),
                      0.0F, 1.0F));
  command_buffer.setScissor(
      0, vk::Rect2D(vk::Offset2D(0, 0), vulkan_swap_chain.Extent()));
  if (fluid_simulator != nullptr && !indices_.empty()) {
    command_buffer.bindVertexBuffers(
        0, {fluid_simulator->ElevationBuffer().buffer}, {0});
    command_buffer.bindIndexBuffer(indices_buffer_.buffer, 0,
                                   vk::IndexType::eUint32);
    command_buffer.drawIndexed(indices_.size(), 1, 0, 0, 0);
  }
}

void renderer::TerrainRenderer::CreateGraphicsPipeline(
    core::VulkanDevice const& vulkan_device,
    core::VulkanSwapChain const& vulkan_swap_chain) {
  auto binding_description = resources::Elevation::GetBindingDescription();
  auto attribute_descriptions =
      resources::Elevation::GetAttributeDescriptions();

  const std::array<vk::DescriptorSetLayout, 1> set_layouts = {
      *camera_descriptor_set_layout_};
  vk::PipelineLayoutCreateInfo pipeline_layout_info{
      .setLayoutCount = static_cast<uint32_t>(set_layouts.size()),
      .pSetLayouts = set_layouts.data()};
  graphics_pipeline_layout_ =
      vk::raii::PipelineLayout(vulkan_device.Device(), pipeline_layout_info);

  core::PipelineBuilder::GraphicsOptions options;
  options.topology = vk::PrimitiveTopology::eTriangleStrip;

  graphics_pipeline_ = core::PipelineBuilder::Graphics(
      vulkan_device, binding_description, attribute_descriptions,
      graphics_pipeline_layout_, vulkan_swap_chain,
      "shaders/graphics/terrain.spv", options);
}

void renderer::TerrainRenderer::CreateCameraDescriptorSetLayout(
    core::VulkanDevice const& vulkan_device) {
  vk::DescriptorSetLayoutBinding ubo_layout_binding{
      .binding = 0,
      .descriptorType = vk::DescriptorType::eUniformBuffer,
      .descriptorCount = 1,
      .stageFlags = vk::ShaderStageFlagBits::eVertex,
      .pImmutableSamplers = nullptr};

  vk::DescriptorSetLayoutCreateInfo layout_info{
      .bindingCount = 1, .pBindings = &ubo_layout_binding};

  camera_descriptor_set_layout_ =
      vulkan_device.Device().createDescriptorSetLayout(layout_info);
}

void renderer::TerrainRenderer::CreateBuffers(
    core::VulkanDevice const& vulkan_device,
    core::CommandPools const& command_pools) {
  camera_ubo_buffer_ = resources::BufferAllocator::CreateMappedUniformBuffer(
      vulkan_device, sizeof(CameraUBO));
  if (!indices_.empty()) {
    indices_buffer_ = std::move(resources::BufferAllocator::CreateSSBO(
                                    vulkan_device, command_pools, indices_,
                                    false, vk::BufferUsageFlagBits::eIndexBuffer)
                                    .front());
  }
}

void renderer::TerrainRenderer::CreateCameraDescriptorSet(
    core::VulkanDevice const& vulkan_device,
    resources::DescriptorAllocator const& descriptor_allocator) {
  camera_descriptor_set_ = descriptor_allocator.Allocate(
      vulkan_device, camera_descriptor_set_layout_);

  vk::DescriptorBufferInfo camera_buffer_info(camera_ubo_buffer_.buffer, 0,
                                              sizeof(CameraUBO));

  vk::WriteDescriptorSet descriptor_write{
      .dstSet = *camera_descriptor_set_,
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = vk::DescriptorType::eUniformBuffer,
      .pBufferInfo = &camera_buffer_info};

  vulkan_device.Device().updateDescriptorSets(
      vk::ArrayProxy<const vk::WriteDescriptorSet>(descriptor_write), {});
}

void renderer::TerrainRenderer::UpdateUniformBuffer(
    core::VulkanSwapChain const& vulkan_swap_chain,
    renderer::Camera const& camera) {
  float aspect_ratio = static_cast<float>(vulkan_swap_chain.Extent().width) /
                       static_cast<float>(vulkan_swap_chain.Extent().height);

  float elevation_normalization = 1.0F;
  glm::mat4 scale =
      glm::scale(glm::mat4(1), glm::vec3(1, elevation_normalization, 1));
  CameraUBO camera_ubo{.model = {1.0F},
                       .view = camera.ViewMatrix(),
                       .proj = camera.ProjectionMatrix(aspect_ratio)};
  camera_ubo.proj[1][1] *= -1;

  resources::BufferAllocator::WriteMapped(camera_ubo_buffer_, &camera_ubo,
                                          sizeof(CameraUBO));
}

void renderer::TerrainRenderer::CreateIndices(uint32_t width, uint32_t height) {
  indices_.clear();

  for (uint32_t i = 0; i < height - 1; i++) {
    for (uint32_t j = 0; j < width - 1; j++) {
      uint32_t index = (i * width) + j;
      indices_.push_back(index);
      indices_.push_back(index + 1);
      indices_.push_back(index + width);

      indices_.push_back(index + 1);
      indices_.push_back(index + width + 1);
      indices_.push_back(index + width);
    }
  }
}
