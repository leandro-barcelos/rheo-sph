#include "pipeline.h"

#include <fstream>
#include <vulkan/vulkan_raii.hpp>

#include "vulkan/vulkan.hpp"
#include "vulkan_device.h"
#include "vulkan_swap_chain.h"

namespace {

vk::raii::ShaderModule CreateShaderModule(
    core::VulkanDevice const& vulkan_device, std::vector<char> const& code) {
  vk::ShaderModuleCreateInfo create_info{
      .codeSize = code.size(),
      .pCode = reinterpret_cast<const uint32_t*>(code.data())};
  vk::raii::ShaderModule shader_module{vulkan_device.Device(), create_info};
  return shader_module;
}

std::vector<char> ReadFile(const std::string& filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("[ERROR] IO: failed to open file " + filename);
  }

  std::vector<char> buffer(file.tellg());
  file.seekg(0, std::ios::beg);
  file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  file.close();

  return buffer;
}

}  // namespace

vk::raii::Pipeline core::PipelineBuilder::Compute(
    core::VulkanDevice const& vulkan_device,
    vk::raii::PipelineLayout const& pipeline_layout,
    std::string const& shader_filename, const char* name) {
  vk::raii::ShaderModule shader_module =
      CreateShaderModule(vulkan_device, ReadFile(shader_filename));

  vk::PipelineShaderStageCreateInfo stage_info{
      .stage = vk::ShaderStageFlagBits::eCompute,
      .module = shader_module,
      .pName = name};

  vk::ComputePipelineCreateInfo pipeline_info{.stage = stage_info,
                                              .layout = *pipeline_layout};

  return {vulkan_device.Device(), nullptr, pipeline_info};
}

vk::raii::Pipeline core::PipelineBuilder::Graphics(
    core::VulkanDevice const& vulkan_device,
    vk::VertexInputBindingDescription binding_description,
    std::vector<vk::VertexInputAttributeDescription> const&
        attribute_descriptions,
    vk::raii::PipelineLayout const& pipeline_layout,
    core::VulkanSwapChain const& swap_chain,
    std::string const& shader_filename) {
  return Graphics(vulkan_device, binding_description, attribute_descriptions,
                  pipeline_layout, swap_chain, shader_filename,
                  GraphicsOptions{});
}

vk::raii::Pipeline core::PipelineBuilder::Graphics(
    core::VulkanDevice const& vulkan_device,
    vk::VertexInputBindingDescription binding_description,
    std::vector<vk::VertexInputAttributeDescription> const&
        attribute_descriptions,
    vk::raii::PipelineLayout const& pipeline_layout,
    core::VulkanSwapChain const& swap_chain,
    std::string const& shader_filename,
    GraphicsOptions options) {
  vk::raii::ShaderModule shader_module =
      CreateShaderModule(vulkan_device, ReadFile(shader_filename));

  vk::PipelineShaderStageCreateInfo vert_shader_stage_info{
      .stage = vk::ShaderStageFlagBits::eVertex,
      .module = shader_module,
      .pName = "vert_main"};
  vk::PipelineShaderStageCreateInfo frag_shader_stage_info{
      .stage = vk::ShaderStageFlagBits::eFragment,
      .module = shader_module,
      .pName = "frag_main"};
  std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages = {
      vert_shader_stage_info, frag_shader_stage_info};

  vk::PipelineVertexInputStateCreateInfo vertex_input_info{
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &binding_description,
      .vertexAttributeDescriptionCount =
          static_cast<uint32_t>(attribute_descriptions.size()),
      .pVertexAttributeDescriptions = attribute_descriptions.data()};

  vk::PipelineInputAssemblyStateCreateInfo input_assembly{
      .topology = options.topology,
      .primitiveRestartEnable = vk::False};

  vk::PipelineViewportStateCreateInfo viewport_state{.viewportCount = 1,
                                                     .scissorCount = 1};

  vk::PipelineRasterizationStateCreateInfo rasterizer{
      .depthClampEnable = vk::False,
      .rasterizerDiscardEnable = vk::False,
      .polygonMode = options.polygon_mode,
      .cullMode = options.cull_mode,
      .frontFace = options.front_face,
      .depthBiasEnable = vk::False,
      .lineWidth = 1.0F};

  vk::PipelineMultisampleStateCreateInfo multisampling{
      .rasterizationSamples = vk::SampleCountFlagBits::e1,
      .sampleShadingEnable = vk::False};

  vk::PipelineColorBlendAttachmentState color_blending_attachment{
      .blendEnable = options.enable_blending ? vk::True : vk::False,
      .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
      .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
      .colorBlendOp = vk::BlendOp::eAdd,
      .srcAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
      .dstAlphaBlendFactor = vk::BlendFactor::eZero,
      .alphaBlendOp = vk::BlendOp::eAdd,
      .colorWriteMask =
          vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
  vk::PipelineColorBlendStateCreateInfo color_blending{
      .logicOpEnable = vk::False,
      .logicOp = vk::LogicOp::eCopy,
      .attachmentCount = 1,
      .pAttachments = &color_blending_attachment};

  std::vector dynamic_states = {vk::DynamicState::eViewport,
                                vk::DynamicState::eScissor};
  vk::PipelineDynamicStateCreateInfo dynamic_state{
      .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
      .pDynamicStates = dynamic_states.data()};

  vk::StructureChain<vk::GraphicsPipelineCreateInfo,
                     vk::PipelineRenderingCreateInfo>
      pipeline_create_info_chain{
          vk::GraphicsPipelineCreateInfo{
              .stageCount = 2,
              .pStages = shader_stages.data(),
              .pVertexInputState = &vertex_input_info,
              .pInputAssemblyState = &input_assembly,
              .pViewportState = &viewport_state,
              .pRasterizationState = &rasterizer,
              .pMultisampleState = &multisampling,
              .pColorBlendState = &color_blending,
              .pDynamicState = &dynamic_state,
              .layout = pipeline_layout,
              .renderPass = nullptr},
          vk::PipelineRenderingCreateInfo{
              .colorAttachmentCount = 1,
              .pColorAttachmentFormats = &swap_chain.SurfaceFormat().format},
      };

  return {vulkan_device.Device(), nullptr,
          pipeline_create_info_chain.get<vk::GraphicsPipelineCreateInfo>()};
}
