#include "fluid_renderer.h"

#include <sys/stat.h>

#include <vulkan/vulkan_raii.hpp>

#include "../core/pipeline.h"
#include "../simulation/fluid_simulator.h"
#include "rheo-sph/src/renderer/camera.h"
#include "rheo-sph/src/resources/buffer.h"
#include "vulkan/vulkan.hpp"

namespace {}  // namespace

void renderer::FluidRenderer::Init(
    core::VulkanDevice const& vulkan_device,
    core::VulkanSwapChain const& vulkan_swap_chain) {
  CreateCameraDescriptorSetLayout(vulkan_device);
  const std::array camera_descriptor_pool_sizes = {
      resources::DescriptorAllocator::PoolSize{
          .type = vk::DescriptorType::eUniformBuffer, .count = 1}};
  camera_descriptor_allocator_.Init(vulkan_device, 1,
                                    camera_descriptor_pool_sizes);
  CreateGraphicsPipeline(vulkan_device, vulkan_swap_chain);
  CreateBuffers(vulkan_device);
  CreateCameraDescriptorSet(vulkan_device, camera_descriptor_allocator_);
}

void renderer::FluidRenderer::Render(
    vk::raii::CommandBuffer const& command_buffer,
    core::VulkanSwapChain& vulkan_swap_chain,
    simulation::FluidSimulator const* fluid_simulator,
    renderer::Camera const& camera) {
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
  if (fluid_simulator != nullptr) {
    command_buffer.bindVertexBuffers(
        0, {fluid_simulator->FluidParticlesReadBuffer().buffer}, {0});
    command_buffer.draw(fluid_simulator->FluidParticleCount(), 1, 0, 0);
  }
}

void renderer::FluidRenderer::CreateGraphicsPipeline(
    core::VulkanDevice const& vulkan_device,
    core::VulkanSwapChain const& vulkan_swap_chain) {
  auto binding_description =
      simulation::FluidSimulator::FluidParticle::GetBindingDescription();
  auto attribute_descriptions =
      simulation::FluidSimulator::FluidParticle::GetAttributeDescriptions();

  const std::array<vk::DescriptorSetLayout, 1> set_layouts = {
      *camera_descriptor_set_layout_};
  vk::PipelineLayoutCreateInfo pipeline_layout_info{
      .setLayoutCount = static_cast<uint32_t>(set_layouts.size()),
      .pSetLayouts = set_layouts.data()};
  graphics_pipeline_layout_ =
      vk::raii::PipelineLayout(vulkan_device.Device(), pipeline_layout_info);

  graphics_pipeline_ = core::PipelineBuilder::Graphics(
      vulkan_device, binding_description, attribute_descriptions,
      graphics_pipeline_layout_, vulkan_swap_chain,
      "shaders/graphics/particle.spv");
}

void renderer::FluidRenderer::CreateCameraDescriptorSetLayout(
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

void renderer::FluidRenderer::CreateBuffers(
    core::VulkanDevice const& vulkan_device) {
  camera_ubo_buffer_ = resources::BufferAllocator::CreateMappedUniformBuffer(
      vulkan_device, sizeof(CameraUBO));
}

void renderer::FluidRenderer::CreateCameraDescriptorSet(
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

void renderer::FluidRenderer::UpdateUniformBuffer(
    core::VulkanSwapChain const& vulkan_swap_chain,
    renderer::Camera const& camera) {
  float aspect_ratio = static_cast<float>(vulkan_swap_chain.Extent().width) /
                       static_cast<float>(vulkan_swap_chain.Extent().height);
  CameraUBO camera_ubo{.model = {1.0F},
                       .view = camera.ViewMatrix(),
                       .proj = camera.ProjectionMatrix(aspect_ratio)};
  camera_ubo.proj[1][1] *= -1;

  resources::BufferAllocator::WriteMapped(camera_ubo_buffer_, &camera_ubo,
                                          sizeof(CameraUBO));
}
