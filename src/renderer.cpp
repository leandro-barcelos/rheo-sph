#include "renderer.h"

#include <sys/types.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "vulkan/vulkan.hpp"
#include "window.h"

vk::VertexInputBindingDescription
render::FluidParticle::GetBindingDescription() {
  return {.binding = 0,
          .stride = sizeof(FluidParticle),
          .inputRate = vk::VertexInputRate::eVertex};
}

std::array<vk::VertexInputAttributeDescription, 3>
render::FluidParticle::GetAttributeDescriptions() {
  return {
      vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat,
                                          offsetof(FluidParticle, position)),
      vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat,
                                          offsetof(FluidParticle, velocity)),
      vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32Sfloat,
                                          offsetof(FluidParticle, density)),
  };
}

void render::Renderer::Init(window::Window const& window,
                            Parameters parameters) {
  parameters_ = parameters;

  CreateInstance();
  CreateSurface(window);
  PickPhysicalDevice();
  CreateLogicalDevice();
  CreateSwapChain(window);
  CreateImageViews();
  CreateBucketDescriptorSetLayout();
  CreateGraphicsPipeline();
  CreateFluidBucketPipeline();
  CreateClearBucketPipeline();
  CreateWallBucketPipeline();
  CreateCommandPools();
  CreateBucketUniformBuffer();
  CreateFluidParticlesStorageBuffers();
  CreateWallParticlesStorageBuffers();
  CreateBucketParametersBuffers();
  CreateDescriptorPool();
  CreateFluidBucketDescriptorSets();
  CreateGraphicsCommandBuffers();
  CreateBucketCommandBuffers();
  CreateSyncObjects();
}

void render::Renderer::Update(window::Window const& window) {
  auto [result, image_index] = swap_chain_.acquireNextImage(
      UINT64_MAX, nullptr, *in_flight_fences_[frame_index_]);
  auto fence_result = device_.waitForFences(*in_flight_fences_[frame_index_],
                                            vk::True, UINT64_MAX);
  if (fence_result != vk::Result::eSuccess) {
    throw std::runtime_error("[ERROR] Vulkan: failed to wait for fence!");
  }
  device_.resetFences(*in_flight_fences_[frame_index_]);

  uint64_t simulation_signal_value = Simulate();

  render::RenderInfo render_info{
      .image_index = image_index,
      .semaphore_wait_value = simulation_signal_value};
  uint64_t graphics_signal_value = Render(render_info);

  vk::SemaphoreWaitInfo wait_info{.semaphoreCount = 1,
                                  .pSemaphores = &*semaphore_,
                                  .pValues = &graphics_signal_value};

  result = device_.waitSemaphores(wait_info, UINT64_MAX);
  if (result != vk::Result::eSuccess) {
    throw std::runtime_error("[ERROR] Vulkan: failed to wait for semaphore!");
  }

  vk::PresentInfoKHR present_info{.waitSemaphoreCount = 0,
                                  .pWaitSemaphores = nullptr,
                                  .swapchainCount = 1,
                                  .pSwapchains = &*swap_chain_,
                                  .pImageIndices = &image_index};

  result = graphics_queue_.presentKHR(present_info);
  if ((result == vk::Result::eSuboptimalKHR) ||
      (result == vk::Result::eErrorOutOfDateKHR) || framebuffer_resized_) {
    framebuffer_resized_ = false;
    RecreateSwapChain(window);
  } else {
    assert(result == vk::Result::eSuccess);
  }

  frame_index_ = (frame_index_ + 1) % kMaxFramesInFlight;
}

void render::Renderer::CreateInstance() {
  constexpr vk::ApplicationInfo kAppInfo{
      .pApplicationName = "Rheo SPH",
      .applicationVersion = VK_MAKE_VERSION(0, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(0, 0, 0),
      .apiVersion = vk::ApiVersion14};

  std::vector<const char*> extensions = window::Window::GetRequiredExtensions();
  auto extension_properties = context_.enumerateInstanceExtensionProperties();
  for (const auto* extension : extensions) {
    if (std::ranges::none_of(
            extension_properties, [extension](auto const& extension_property) {
              return strcmp(extension_property.extensionName, extension) == 0;
            })) {
      throw std::runtime_error(
          "[ERROR] Vulkan: required window extension not supported: " +
          std::string(extension));
    }
  }

  std::vector<const char*> layers{};
  if (kEnableValidationLayers) {
    layers.assign(kValidationLayers.begin(), kValidationLayers.end());
  }

  auto layer_properties = context_.enumerateInstanceLayerProperties();
  for (const auto* layer : layers) {
    if (std::ranges::none_of(
            layer_properties, [layer](auto const& layer_property) {
              return strcmp(layer_property.layerName, layer) == 0;
            })) {
      throw std::runtime_error(
          "[ERROR] Vulkan: required layer not supported: " +
          std::string(layer));
    }
  }

  const vk::InstanceCreateInfo create_info{
      .pApplicationInfo = &kAppInfo,
      .enabledLayerCount = static_cast<uint32_t>(layers.size()),
      .ppEnabledLayerNames = layers.data(),
      .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
      .ppEnabledExtensionNames = extensions.data(),
  };

  instance_ = vk::raii::Instance(context_, create_info);
}

bool render::Renderer::IsDeviceSuitable(
    vk::raii::PhysicalDevice const& physical_device) {
  bool supports_vulkan_1_3 =
      physical_device.getProperties().apiVersion >= VK_API_VERSION_1_3;

  auto queue_families = physical_device.getQueueFamilyProperties();
  bool supports_graphics =
      std::ranges::any_of(queue_families, [](auto const& qfp) {
        return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
      });

  auto available_device_extensions =
      physical_device.enumerateDeviceExtensionProperties();
  bool supports_all_required_extensions = std::ranges::all_of(
      required_device_extensions_,
      [&available_device_extensions](auto const& required_device_extension) {
        return std::ranges::any_of(
            available_device_extensions,
            [required_device_extension](
                auto const& available_device_extension) {
              return strcmp(available_device_extension.extensionName,
                            required_device_extension) == 0;
            });
      });

  auto features = physical_device.template getFeatures2<
      vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
      vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
      vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR>();
  bool supports_required_features =
      (features.template get<vk::PhysicalDeviceFeatures2>()
           .features.samplerAnisotropy != 0U) &&
      (features.template get<vk::PhysicalDeviceFeatures2>()
           .features.shaderTessellationAndGeometryPointSize != 0U) &&
      (features.template get<vk::PhysicalDeviceVulkan13Features>()
           .dynamicRendering != 0U) &&
      (features.template get<vk::PhysicalDeviceVulkan13Features>()
           .synchronization2 != 0U) &&
      (features
           .template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
           .extendedDynamicState != 0U) &&
      (features.template get<vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR>()
           .timelineSemaphore != 0U);

  return supports_vulkan_1_3 && supports_graphics &&
         supports_all_required_extensions && supports_required_features;
}

void render::Renderer::PickPhysicalDevice() {
  const auto physical_devices = instance_.enumeratePhysicalDevices();
  auto const dev_iter =
      std::ranges::find_if(physical_devices, [&](auto const& physical_device) {
        return IsDeviceSuitable(physical_device);
      });
  if (dev_iter == physical_devices.end()) {
    throw std::runtime_error("[ERROR] Vulkan: failed to find a suitable GPU");
  }
  physical_device_ = *dev_iter;
}

void render::Renderer::CreateLogicalDevice() {
  std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
  queue_create_infos.reserve(3);

  graphics_queue_index_ = FindQueue(vk::QueueFlagBits::eGraphics);
  constexpr float kGraphicsQueuePriority = 0.5F;
  const vk::DeviceQueueCreateInfo graphics_queue_create_info{
      .queueFamilyIndex = graphics_queue_index_,
      .queueCount = 1,
      .pQueuePriorities = &kGraphicsQueuePriority};
  queue_create_infos.push_back(graphics_queue_create_info);

  compute_queue_index_ =
      FindQueue(vk::QueueFlagBits::eCompute, {graphics_queue_index_});
  if (compute_queue_index_ == graphics_queue_index_) {
    throw std::runtime_error(
        "[ERROR] Vulkan: failed to find a dedicated compute queue");
  }
  constexpr float kComputeQueuePriority = 0.5F;
  const vk::DeviceQueueCreateInfo compute_queue_create_info{
      .queueFamilyIndex = compute_queue_index_,
      .queueCount = 1,
      .pQueuePriorities = &kComputeQueuePriority};
  queue_create_infos.push_back(compute_queue_create_info);

  transfer_queue_index_ =
      FindQueue(vk::QueueFlagBits::eTransfer,
                {graphics_queue_index_, compute_queue_index_});
  if (transfer_queue_index_ != graphics_queue_index_ &&
      transfer_queue_index_ != compute_queue_index_) {
    constexpr float kTransferQueuePriority = 0.5F;
    const vk::DeviceQueueCreateInfo transfer_queue_create_info{
        .queueFamilyIndex = transfer_queue_index_,
        .queueCount = 1,
        .pQueuePriorities = &kTransferQueuePriority};
    queue_create_infos.push_back(transfer_queue_create_info);
  }

  vk::StructureChain<vk::PhysicalDeviceFeatures2,
                     vk::PhysicalDeviceVulkan13Features,
                     vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
                     vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR>
      feature_chain = {
          {.features = {.samplerAnisotropy = vk::True,
                        .shaderTessellationAndGeometryPointSize = vk::True}},
          {.synchronization2 = vk::True, .dynamicRendering = vk::True},
          {.extendedDynamicState = vk::True},
          {.timelineSemaphore = vk::True}};

  std::vector required_device_extensions = {vk::KHRSwapchainExtensionName};

  const vk::DeviceCreateInfo device_create_info{
      .pNext = &feature_chain.get<vk::PhysicalDeviceFeatures2>(),
      .queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
      .pQueueCreateInfos = queue_create_infos.data(),
      .enabledExtensionCount =
          static_cast<uint32_t>(required_device_extensions.size()),
      .ppEnabledExtensionNames = required_device_extensions.data()};

  device_ = vk::raii::Device(physical_device_, device_create_info);
  graphics_queue_ = vk::raii::Queue(device_, graphics_queue_index_, 0);
  compute_queue_ = vk::raii::Queue(device_, compute_queue_index_, 0);
  if (transfer_queue_index_ == graphics_queue_index_) {
    transfer_queue_ = graphics_queue_;
  } else if (transfer_queue_index_ == compute_queue_index_) {
    transfer_queue_ = compute_queue_;
  } else {
    transfer_queue_ = vk::raii::Queue(device_, transfer_queue_index_, 0);
  }
}

void render::Renderer::CreateDescriptorPool() {
  std::array pool_size{
      vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer,
                             kMaxFramesInFlight),
      vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer,
                             kMaxFramesInFlight * 3)};
  vk::DescriptorPoolCreateInfo pool_info{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = kMaxFramesInFlight,
      .poolSizeCount = pool_size.size(),
      .pPoolSizes = pool_size.data(),
  };
  descriptor_pool_ = vk::raii::DescriptorPool(device_, pool_info);
}

void render::Renderer::CreateCommandPools() {
  vk::CommandPoolCreateInfo graphics_pool_info{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = graphics_queue_index_,
  };
  graphics_command_pool_ = vk::raii::CommandPool(device_, graphics_pool_info);

  vk::CommandPoolCreateInfo compute_pool_info{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = compute_queue_index_,
  };
  compute_command_pool_ = vk::raii::CommandPool(device_, compute_pool_info);

  vk::CommandPoolCreateInfo transfer_pool_info{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = transfer_queue_index_,
  };
  transfer_command_pool_ = vk::raii::CommandPool(device_, transfer_pool_info);
}

void render::Renderer::CreateSyncObjects() {
  in_flight_fences_.clear();

  vk::SemaphoreTypeCreateInfo semaphore_type{
      .semaphoreType = vk::SemaphoreType::eTimeline, .initialValue = 0};
  vk::SemaphoreCreateInfo semaphore_info{.pNext = &semaphore_type};
  semaphore_ = vk::raii::Semaphore(device_, semaphore_info);
  timeline_value_ = 0;

  for (size_t i = 0; i < kMaxFramesInFlight; i++) {
    vk::FenceCreateInfo fence_info{};
    in_flight_fences_.emplace_back(device_, fence_info);
  }
}

uint64_t render::Renderer::Simulate() {
  uint64_t bucket_wait_value = timeline_value_;
  uint64_t bucket_signal_value = ++timeline_value_;

  RecordClearBucketCommandBuffer();
  RecordFluidBucketCommandBuffer();
  RecordWallBucketCommandBuffer();
  RecordBucketPrimaryCommandBuffer();

  vk::TimelineSemaphoreSubmitInfo bucket_timeline_info{
      .waitSemaphoreValueCount = 1,
      .pWaitSemaphoreValues = &bucket_wait_value,
      .signalSemaphoreValueCount = 1,
      .pSignalSemaphoreValues = &bucket_signal_value};

  std::array<vk::PipelineStageFlags, 1> wait_stages{
      vk::PipelineStageFlagBits::eComputeShader};

  vk::SubmitInfo bucket_submit_info{
      .pNext = &bucket_timeline_info,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &*semaphore_,
      .pWaitDstStageMask = wait_stages.data(),
      .commandBufferCount = 1,
      .pCommandBuffers = &*bucket_primary_command_buffers_[frame_index_],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &*semaphore_};

  compute_queue_.submit(bucket_submit_info, nullptr);

  return bucket_signal_value;
}

void render::Renderer::CreateFluidBucketPipeline() {
  vk::raii::ShaderModule bucket_shader_module =
      CreateShaderModule(ReadFile("shaders/compute/bucket.spv"));

  vk::PipelineShaderStageCreateInfo fluid_bucket_stage_info{
      .stage = vk::ShaderStageFlagBits::eCompute,
      .module = bucket_shader_module,
      .pName = "fluid_bucket"};

  vk::PipelineLayoutCreateInfo pipeline_layout_info{
      .setLayoutCount = 1, .pSetLayouts = &*bucket_descriptor_set_layout_};
  bucket_pipeline_layout_ =
      vk::raii::PipelineLayout(device_, pipeline_layout_info);

  vk::ComputePipelineCreateInfo pipeline_info{
      .stage = fluid_bucket_stage_info, .layout = *bucket_pipeline_layout_};
  fluid_bucket_pipeline_ = vk::raii::Pipeline(device_, nullptr, pipeline_info);
}

void render::Renderer::CreateClearBucketPipeline() {
  vk::raii::ShaderModule bucket_shader_module =
      CreateShaderModule(ReadFile("shaders/compute/bucket.spv"));

  vk::PipelineShaderStageCreateInfo clear_bucket_stage_info{
      .stage = vk::ShaderStageFlagBits::eCompute,
      .module = bucket_shader_module,
      .pName = "clear_bucket"};

  vk::ComputePipelineCreateInfo pipeline_info{
      .stage = clear_bucket_stage_info, .layout = *bucket_pipeline_layout_};
  clear_bucket_pipeline_ = vk::raii::Pipeline(device_, nullptr, pipeline_info);
}

void render::Renderer::CreateWallBucketPipeline() {
  vk::raii::ShaderModule bucket_shader_module =
      CreateShaderModule(ReadFile("shaders/compute/bucket.spv"));

  vk::PipelineShaderStageCreateInfo wall_bucket_stage_info{
      .stage = vk::ShaderStageFlagBits::eCompute,
      .module = bucket_shader_module,
      .pName = "wall_bucket"};

  vk::ComputePipelineCreateInfo pipeline_info{
      .stage = wall_bucket_stage_info, .layout = *bucket_pipeline_layout_};
  wall_bucket_pipeline_ = vk::raii::Pipeline(device_, nullptr, pipeline_info);
}

void render::Renderer::CreateBucketCommandBuffers() {
  bucket_primary_command_buffers_.clear();
  clear_bucket_secondary_command_buffers_.clear();
  fluid_bucket_secondary_command_buffers_.clear();
  wall_bucket_secondary_command_buffers_.clear();

  vk::CommandBufferAllocateInfo primary_alloc_info{
      .commandPool = *compute_command_pool_,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = kMaxFramesInFlight,
  };
  bucket_primary_command_buffers_ =
      vk::raii::CommandBuffers(device_, primary_alloc_info);

  vk::CommandBufferAllocateInfo secondary_alloc_info{
      .commandPool = *compute_command_pool_,
      .level = vk::CommandBufferLevel::eSecondary,
      .commandBufferCount = kMaxFramesInFlight,
  };
  clear_bucket_secondary_command_buffers_ =
      vk::raii::CommandBuffers(device_, secondary_alloc_info);
  fluid_bucket_secondary_command_buffers_ =
      vk::raii::CommandBuffers(device_, secondary_alloc_info);
  wall_bucket_secondary_command_buffers_ =
      vk::raii::CommandBuffers(device_, secondary_alloc_info);
}

void render::Renderer::CreateBucketDescriptorSetLayout() {
  std::array layout_bindings{
      vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr),
      vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr),
      vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr),
      vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr)};

  vk::DescriptorSetLayoutCreateInfo layout_info{
      .bindingCount = static_cast<uint32_t>(layout_bindings.size()),
      .pBindings = layout_bindings.data()};
  bucket_descriptor_set_layout_ =
      vk::raii::DescriptorSetLayout(device_, layout_info);
}

void render::Renderer::CreateBucketUniformBuffer() {
  vk::DeviceSize buffer_size = sizeof(Parameters);

  vk::raii::Buffer staging_buffer({});
  vk::raii::DeviceMemory staging_buffer_memory({});
  CreateBuffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               staging_buffer, staging_buffer_memory);

  void* data_staging = staging_buffer_memory.mapMemory(0, buffer_size);
  memcpy(data_staging, &parameters_, (size_t)buffer_size);
  staging_buffer_memory.unmapMemory();

  vk::raii::Buffer shader_storage_buffer_temp({});
  vk::raii::DeviceMemory shader_storage_buffer_temp_memory({});
  CreateBuffer(buffer_size,
               vk::BufferUsageFlagBits::eUniformBuffer |
                   vk::BufferUsageFlagBits::eTransferDst,
               vk::MemoryPropertyFlagBits::eDeviceLocal,
               shader_storage_buffer_temp, shader_storage_buffer_temp_memory);
  CopyBuffer(staging_buffer, shader_storage_buffer_temp, buffer_size);
  parameters_uniform_buffer_ = std::move(shader_storage_buffer_temp);
  parameters_uniform_buffer_memory_ =
      std::move(shader_storage_buffer_temp_memory);
}

void render::Renderer::CreateFluidParticlesStorageBuffers() {
  fluid_particles_buffers_.clear();
  fluid_particles_buffers_memory_.clear();

  if (parameters_.fluid_particle_count == 0) {
    return;
  }

  std::vector<FluidParticle> particles(parameters_.fluid_particle_count);
  vk::DeviceSize buffer_size = sizeof(FluidParticle) * particles.size();

  vk::raii::Buffer staging_buffer({});
  vk::raii::DeviceMemory staging_buffer_memory({});
  CreateBuffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               staging_buffer, staging_buffer_memory);

  void* data_staging = staging_buffer_memory.mapMemory(0, buffer_size);
  std::memcpy(data_staging, particles.data(), static_cast<size_t>(buffer_size));
  staging_buffer_memory.unmapMemory();

  for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
    vk::raii::Buffer storage_buffer({});
    vk::raii::DeviceMemory storage_buffer_memory({});
    CreateBuffer(buffer_size,
                 vk::BufferUsageFlagBits::eStorageBuffer |
                     vk::BufferUsageFlagBits::eVertexBuffer |
                     vk::BufferUsageFlagBits::eTransferDst,
                 vk::MemoryPropertyFlagBits::eDeviceLocal, storage_buffer,
                 storage_buffer_memory);
    CopyBuffer(staging_buffer, storage_buffer, buffer_size);
    fluid_particles_buffers_.emplace_back(std::move(storage_buffer));
    fluid_particles_buffers_memory_.emplace_back(
        std::move(storage_buffer_memory));
  }
}

void render::Renderer::CreateWallParticlesStorageBuffers() {
  wall_particles_buffers_.clear();
  wall_particles_buffers_memory_.clear();

  if (parameters_.wall_particle_count == 0) {
    return;
  }

  std::vector<WallParticle> particles(parameters_.wall_particle_count);
  vk::DeviceSize buffer_size = sizeof(WallParticle) * particles.size();

  vk::raii::Buffer staging_buffer({});
  vk::raii::DeviceMemory staging_buffer_memory({});
  CreateBuffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               staging_buffer, staging_buffer_memory);

  void* data_staging = staging_buffer_memory.mapMemory(0, buffer_size);
  std::memcpy(data_staging, particles.data(), static_cast<size_t>(buffer_size));
  staging_buffer_memory.unmapMemory();

  for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
    vk::raii::Buffer storage_buffer({});
    vk::raii::DeviceMemory storage_buffer_memory({});
    CreateBuffer(buffer_size,
                 vk::BufferUsageFlagBits::eStorageBuffer |
                     vk::BufferUsageFlagBits::eVertexBuffer |
                     vk::BufferUsageFlagBits::eTransferDst,
                 vk::MemoryPropertyFlagBits::eDeviceLocal, storage_buffer,
                 storage_buffer_memory);
    CopyBuffer(staging_buffer, storage_buffer, buffer_size);
    wall_particles_buffers_.emplace_back(std::move(storage_buffer));
    wall_particles_buffers_memory_.emplace_back(
        std::move(storage_buffer_memory));
  }
}

void render::Renderer::CreateBucketParametersBuffers() {
  bucket_buffers_.clear();
  bucket_buffers_memory_.clear();

  const size_t bucket_count =
      static_cast<size_t>(parameters_.bucket_size[0]) *
      static_cast<size_t>(parameters_.bucket_size[1]) *
      static_cast<size_t>(parameters_.bucket_size[2]) *
      static_cast<size_t>(parameters_.voxel_max_particles);
  if (bucket_count == 0) {
    return;
  }

  std::vector<uint32_t> buckets(bucket_count, parameters_.total_particle_count);
  vk::DeviceSize buffer_size = sizeof(uint32_t) * buckets.size();

  vk::raii::Buffer staging_buffer({});
  vk::raii::DeviceMemory staging_buffer_memory({});
  CreateBuffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
               vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent,
               staging_buffer, staging_buffer_memory);

  void* data_staging = staging_buffer_memory.mapMemory(0, buffer_size);
  std::memcpy(data_staging, buckets.data(), static_cast<size_t>(buffer_size));
  staging_buffer_memory.unmapMemory();

  for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
    vk::raii::Buffer storage_buffer({});
    vk::raii::DeviceMemory storage_buffer_memory({});
    CreateBuffer(buffer_size,
                 vk::BufferUsageFlagBits::eStorageBuffer |
                     vk::BufferUsageFlagBits::eTransferDst,
                 vk::MemoryPropertyFlagBits::eDeviceLocal, storage_buffer,
                 storage_buffer_memory);
    CopyBuffer(staging_buffer, storage_buffer, buffer_size);
    bucket_buffers_.emplace_back(std::move(storage_buffer));
    bucket_buffers_memory_.emplace_back(std::move(storage_buffer_memory));
  }
}

void render::Renderer::CreateFluidBucketDescriptorSets() {
  std::vector<vk::DescriptorSetLayout> layouts(kMaxFramesInFlight,
                                               bucket_descriptor_set_layout_);
  vk::DescriptorSetAllocateInfo alloc_info{
      .descriptorPool = *descriptor_pool_,
      .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
      .pSetLayouts = layouts.data()};
  fluid_bucket_descriptor_sets_.clear();
  fluid_bucket_descriptor_sets_ = device_.allocateDescriptorSets(alloc_info);

  for (size_t i = 0; i < fluid_bucket_descriptor_sets_.size(); ++i) {
    vk::DescriptorBufferInfo parameters_buffer_info{
        .buffer = parameters_uniform_buffer_,
        .offset = 0,
        .range = sizeof(Parameters)};
    vk::DescriptorBufferInfo fluid_particles_buffer_info{
        .buffer = fluid_particles_buffers_[i],
        .offset = 0,
        .range = VK_WHOLE_SIZE};
    vk::DescriptorBufferInfo wall_particles_buffer_info{
        .buffer = wall_particles_buffers_[i],
        .offset = 0,
        .range = VK_WHOLE_SIZE};
    vk::DescriptorBufferInfo bucket_buffer_info{
        .buffer = bucket_buffers_[i], .offset = 0, .range = VK_WHOLE_SIZE};

    std::array descriptor_writes{
        vk::WriteDescriptorSet{
            .dstSet = *fluid_bucket_descriptor_sets_[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pImageInfo = nullptr,
            .pBufferInfo = &parameters_buffer_info,
            .pTexelBufferView = nullptr},
        vk::WriteDescriptorSet{
            .dstSet = *fluid_bucket_descriptor_sets_[i],
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .pImageInfo = nullptr,
            .pBufferInfo = &fluid_particles_buffer_info,
            .pTexelBufferView = nullptr},
        vk::WriteDescriptorSet{
            .dstSet = *fluid_bucket_descriptor_sets_[i],
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .pImageInfo = nullptr,
            .pBufferInfo = &wall_particles_buffer_info,
            .pTexelBufferView = nullptr},
        vk::WriteDescriptorSet{
            .dstSet = *fluid_bucket_descriptor_sets_[i],
            .dstBinding = 3,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .pImageInfo = nullptr,
            .pBufferInfo = &bucket_buffer_info,
            .pTexelBufferView = nullptr},
    };
    device_.updateDescriptorSets(descriptor_writes, {});
  }
}

void render::Renderer::RecordFluidBucketCommandBuffer() {
  fluid_bucket_secondary_command_buffers_[frame_index_].reset();

  vk::CommandBufferInheritanceInfo inheritance_info{};
  vk::CommandBufferBeginInfo begin_info{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
      .pInheritanceInfo = &inheritance_info};

  fluid_bucket_secondary_command_buffers_[frame_index_].begin(begin_info);

  fluid_bucket_secondary_command_buffers_[frame_index_].bindPipeline(
      vk::PipelineBindPoint::eCompute, fluid_bucket_pipeline_);
  fluid_bucket_secondary_command_buffers_[frame_index_].bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, bucket_pipeline_layout_, 0,
      {fluid_bucket_descriptor_sets_[frame_index_]}, {});
  fluid_bucket_secondary_command_buffers_[frame_index_].dispatch(
      (parameters_.fluid_particle_count + kNumThreads - 1) / kNumThreads, 1, 1);

  fluid_bucket_secondary_command_buffers_[frame_index_].end();
}

void render::Renderer::RecordClearBucketCommandBuffer() {
  clear_bucket_secondary_command_buffers_[frame_index_].reset();

  vk::CommandBufferInheritanceInfo inheritance_info{};
  vk::CommandBufferBeginInfo begin_info{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
      .pInheritanceInfo = &inheritance_info};

  clear_bucket_secondary_command_buffers_[frame_index_].begin(begin_info);

  clear_bucket_secondary_command_buffers_[frame_index_].bindPipeline(
      vk::PipelineBindPoint::eCompute, clear_bucket_pipeline_);
  clear_bucket_secondary_command_buffers_[frame_index_].bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, bucket_pipeline_layout_, 0,
      {fluid_bucket_descriptor_sets_[frame_index_]}, {});

  const uint32_t bucket_dispatch_x =
      (parameters_.bucket_size[0] + kClearBucketsNumThreads - 1) /
      kClearBucketsNumThreads;
  const uint32_t bucket_dispatch_y =
      (parameters_.bucket_size[1] + kClearBucketsNumThreads - 1) /
      kClearBucketsNumThreads;
  const uint32_t bucket_dispatch_z =
      (parameters_.bucket_size[2] + kClearBucketsNumThreads - 1) /
      kClearBucketsNumThreads;
  clear_bucket_secondary_command_buffers_[frame_index_].dispatch(
      bucket_dispatch_x, bucket_dispatch_y, bucket_dispatch_z);

  clear_bucket_secondary_command_buffers_[frame_index_].end();
}

void render::Renderer::RecordBucketPrimaryCommandBuffer() {
  bucket_primary_command_buffers_[frame_index_].reset();

  vk::CommandBufferBeginInfo begin_info{};
  bucket_primary_command_buffers_[frame_index_].begin(begin_info);

  std::array<vk::CommandBuffer, 3> secondary_command_buffers{
      *clear_bucket_secondary_command_buffers_[frame_index_],
      *fluid_bucket_secondary_command_buffers_[frame_index_],
      *wall_bucket_secondary_command_buffers_[frame_index_]};
  bucket_primary_command_buffers_[frame_index_].executeCommands(
      secondary_command_buffers);

  bucket_primary_command_buffers_[frame_index_].end();
}

void render::Renderer::RecordWallBucketCommandBuffer() {
  wall_bucket_secondary_command_buffers_[frame_index_].reset();

  vk::CommandBufferInheritanceInfo inheritance_info{};
  vk::CommandBufferBeginInfo begin_info{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
      .pInheritanceInfo = &inheritance_info};

  wall_bucket_secondary_command_buffers_[frame_index_].begin(begin_info);

  wall_bucket_secondary_command_buffers_[frame_index_].bindPipeline(
      vk::PipelineBindPoint::eCompute, wall_bucket_pipeline_);
  wall_bucket_secondary_command_buffers_[frame_index_].bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, bucket_pipeline_layout_, 0,
      {fluid_bucket_descriptor_sets_[frame_index_]}, {});
  wall_bucket_secondary_command_buffers_[frame_index_].dispatch(
      (parameters_.wall_particle_count + kNumThreads - 1) / kNumThreads, 1, 1);

  wall_bucket_secondary_command_buffers_[frame_index_].end();
}

void render::Renderer::CreateSurface(window::Window const& window) {
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  window.CreateSurface(*instance_, surface);
  surface_ = vk::raii::SurfaceKHR(instance_, surface);
}

void render::Renderer::CreateSwapChain(window::Window const& window) {
  vk::SurfaceCapabilitiesKHR surface_capabilities =
      physical_device_.getSurfaceCapabilitiesKHR(*surface_);
  swap_chain_extent_ = ChooseSwapExtent(window, surface_capabilities);
  uint32_t min_image_count = ChooseSwapMinImageCount(surface_capabilities);

  std::vector<vk::SurfaceFormatKHR> available_formats =
      physical_device_.getSurfaceFormatsKHR(*surface_);
  swap_chain_surface_format_ = ChooseSwapSurfaceFormat(available_formats);

  std::vector<vk::PresentModeKHR> available_present_modes =
      physical_device_.getSurfacePresentModesKHR(*surface_);
  vk::PresentModeKHR present_mode =
      ChooseSwapPresentMode(available_present_modes);

  vk::SwapchainCreateInfoKHR swap_chain_create_info{
      .surface = *surface_,
      .minImageCount = min_image_count,
      .imageFormat = swap_chain_surface_format_.format,
      .imageColorSpace = swap_chain_surface_format_.colorSpace,
      .imageExtent = swap_chain_extent_,
      .imageArrayLayers = 1,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
      .imageSharingMode = vk::SharingMode::eExclusive,
      .preTransform = surface_capabilities.currentTransform,
      .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode = present_mode,
      .clipped = vk::True};

  swap_chain_ = vk::raii::SwapchainKHR(device_, swap_chain_create_info);
  swap_chain_images_ = swap_chain_.getImages();
  swap_chain_image_layouts_.assign(swap_chain_images_.size(),
                                   vk::ImageLayout::eUndefined);
}

void render::Renderer::CreateImageViews() {
  assert(swap_chain_image_views_.empty());

  vk::ImageViewCreateInfo image_view_create_info{
      .viewType = vk::ImageViewType::e2D,
      .format = swap_chain_surface_format_.format,
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};

  for (auto& image : swap_chain_images_) {
    image_view_create_info.image = image;
    swap_chain_image_views_.emplace_back(device_, image_view_create_info);
  }
}

vk::Extent2D render::Renderer::ChooseSwapExtent(
    window::Window const& window,
    vk::SurfaceCapabilitiesKHR const& surface_capabilities) {
  if (surface_capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return surface_capabilities.currentExtent;
  }

  window::Size framebuffer_size = window.GetSize();

  return {
      .width = std::clamp<uint32_t>(framebuffer_size.width,
                                    surface_capabilities.minImageExtent.width,
                                    surface_capabilities.maxImageExtent.width),
      .height = std::clamp<uint32_t>(
          framebuffer_size.height, surface_capabilities.minImageExtent.height,
          surface_capabilities.maxImageExtent.height)};
}

uint32_t render::Renderer::ChooseSwapMinImageCount(
    vk::SurfaceCapabilitiesKHR const& surface_capabilities) {
  auto min_image_count = std::max(3U, surface_capabilities.minImageCount);
  if ((0 < surface_capabilities.maxImageCount) &&
      (surface_capabilities.maxImageCount < min_image_count)) {
    min_image_count = surface_capabilities.maxImageCount;
  }
  return min_image_count;
}

vk::SurfaceFormatKHR render::Renderer::ChooseSwapSurfaceFormat(
    std::vector<vk::SurfaceFormatKHR> const& available_formats) {
  assert(!available_formats.empty());
  const auto format_it =
      std::ranges::find_if(available_formats, [](const auto& format) {
        return format.format == vk::Format::eB8G8R8A8Srgb &&
               format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
      });

  return format_it != available_formats.end() ? *format_it
                                              : available_formats[0];
}

vk::PresentModeKHR render::Renderer::ChooseSwapPresentMode(
    std::vector<vk::PresentModeKHR> const& available_present_modes) {
  assert(std::ranges::any_of(available_present_modes, [](auto present_mode) {
    return present_mode == vk::PresentModeKHR::eFifo;
  }));
  return std::ranges::any_of(available_present_modes,
                             [](const vk::PresentModeKHR value) {
                               return vk::PresentModeKHR::eMailbox == value;
                             })
             ? vk::PresentModeKHR::eMailbox
             : vk::PresentModeKHR::eFifo;
}

void render::Renderer::CreateGraphicsCommandBuffers() {
  graphics_command_buffers_.clear();
  vk::CommandBufferAllocateInfo alloc_info{
      .commandPool = *graphics_command_pool_,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = kMaxFramesInFlight};
  graphics_command_buffers_ = device_.allocateCommandBuffers(alloc_info);
}

void render::Renderer::TransitionImageLayout(
    uint32_t image_index, vk::ImageLayout old_layout,
    vk::ImageLayout new_layout, vk::AccessFlags2 src_access_mask,
    vk::AccessFlags2 dst_access_mask, vk::PipelineStageFlags2 src_stage_mask,
    vk::PipelineStageFlags2 dst_stage_mask) {
  vk::ImageMemoryBarrier2 barrier = {
      .srcStageMask = src_stage_mask,
      .srcAccessMask = src_access_mask,
      .dstStageMask = dst_stage_mask,
      .dstAccessMask = dst_access_mask,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
      .image = swap_chain_images_[image_index],
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};
  vk::DependencyInfo dependency_info{.dependencyFlags = {},
                                     .imageMemoryBarrierCount = 1,
                                     .pImageMemoryBarriers = &barrier};
  graphics_command_buffers_[frame_index_].pipelineBarrier2(dependency_info);
}

void render::Renderer::RecordGraphicsCommandBuffer(uint32_t image_index) {
  auto& command_buffer = graphics_command_buffers_[frame_index_];
  command_buffer.reset();
  command_buffer.begin({});

  TransitionImageLayout(image_index, swap_chain_image_layouts_[image_index],
                        vk::ImageLayout::eColorAttachmentOptimal, {},
                        vk::AccessFlagBits2::eColorAttachmentWrite,
                        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                        vk::PipelineStageFlagBits2::eColorAttachmentOutput);
  swap_chain_image_layouts_[image_index] =
      vk::ImageLayout::eColorAttachmentOptimal;

  vk::ClearValue clear_color = vk::ClearColorValue(0.0F, 0.0F, 0.0F, 1.0F);
  vk::RenderingAttachmentInfo attachment_info{
      .imageView = swap_chain_image_views_[image_index],
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = clear_color};
  vk::RenderingInfo rendering_info{
      .renderArea = {.offset = {.x = 0, .y = 0}, .extent = swap_chain_extent_},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &attachment_info};
  command_buffer.beginRendering(rendering_info);
  command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                              *graphics_pipeline_);
  command_buffer.setViewport(
      0,
      vk::Viewport(0.0F, 0.0F, static_cast<float>(swap_chain_extent_.width),
                   static_cast<float>(swap_chain_extent_.height), 0.0F, 1.0F));
  command_buffer.setScissor(0,
                            vk::Rect2D(vk::Offset2D(0, 0), swap_chain_extent_));
  command_buffer.bindVertexBuffers(0, {fluid_particles_buffers_[frame_index_]},
                                   {0});
  command_buffer.draw(parameters_.fluid_particle_count, 1, 0, 0);
  command_buffer.endRendering();

  TransitionImageLayout(image_index, vk::ImageLayout::eColorAttachmentOptimal,
                        vk::ImageLayout::ePresentSrcKHR,
                        vk::AccessFlagBits2::eColorAttachmentWrite, {},
                        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                        vk::PipelineStageFlagBits2::eBottomOfPipe);
  swap_chain_image_layouts_[image_index] = vk::ImageLayout::ePresentSrcKHR;
  command_buffer.end();
}

void render::Renderer::CleanupSwapChain() {
  swap_chain_image_views_.clear();
  swap_chain_image_layouts_.clear();
  swap_chain_ = nullptr;
}

void render::Renderer::RecreateSwapChain(window::Window const& window) {
  window::Size window_size = window.GetSize();
  while (window_size.width == 0 || window_size.height == 0) {
    window_size = window.GetSize();
    window::Window::WaitEvents();
  }

  device_.waitIdle();

  CleanupSwapChain();
  CreateSwapChain(window);
  CreateImageViews();
}

void render::Renderer::CreateGraphicsPipeline() {
  vk::raii::ShaderModule shader_module =
      CreateShaderModule(ReadFile("shaders/graphics/particle.spv"));

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

  auto binding_description = FluidParticle::GetBindingDescription();
  auto attribute_descriptions = FluidParticle::GetAttributeDescriptions();
  vk::PipelineVertexInputStateCreateInfo vertex_input_info{
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &binding_description,
      .vertexAttributeDescriptionCount =
          static_cast<uint32_t>(attribute_descriptions.size()),
      .pVertexAttributeDescriptions = attribute_descriptions.data()};

  vk::PipelineInputAssemblyStateCreateInfo input_assembly{
      .topology = vk::PrimitiveTopology::ePointList,
      .primitiveRestartEnable = vk::False};

  vk::PipelineViewportStateCreateInfo viewport_state{.viewportCount = 1,
                                                     .scissorCount = 1};

  vk::PipelineRasterizationStateCreateInfo rasterizer{
      .depthClampEnable = vk::False,
      .rasterizerDiscardEnable = vk::False,
      .polygonMode = vk::PolygonMode::eFill,
      .cullMode = vk::CullModeFlagBits::eBack,
      .frontFace = vk::FrontFace::eCounterClockwise,
      .depthBiasEnable = vk::False,
      .lineWidth = 1.0F};

  vk::PipelineMultisampleStateCreateInfo multisampling{
      .rasterizationSamples = vk::SampleCountFlagBits::e1,
      .sampleShadingEnable = vk::False};

  vk::PipelineColorBlendAttachmentState color_blending_attachment{
      .blendEnable = vk::True,
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

  vk::PipelineLayoutCreateInfo pipeline_layout_info;
  graphics_pipeline_layout_ =
      vk::raii::PipelineLayout(device_, pipeline_layout_info);

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
              .layout = graphics_pipeline_layout_,
              .renderPass = nullptr},
          vk::PipelineRenderingCreateInfo{
              .colorAttachmentCount = 1,
              .pColorAttachmentFormats = &swap_chain_surface_format_.format},
      };
  graphics_pipeline_ = vk::raii::Pipeline(
      device_, nullptr,
      pipeline_create_info_chain.get<vk::GraphicsPipelineCreateInfo>());
}

uint64_t render::Renderer::Render(render::RenderInfo info) {
  uint64_t graphics_wait_value = info.semaphore_wait_value;
  uint64_t graphics_signal_value = ++timeline_value_;

  RecordGraphicsCommandBuffer(info.image_index);

  vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eVertexInput;
  vk::TimelineSemaphoreSubmitInfo graphics_timeline_info{
      .waitSemaphoreValueCount = 1,
      .pWaitSemaphoreValues = &graphics_wait_value,
      .signalSemaphoreValueCount = 1,
      .pSignalSemaphoreValues = &graphics_signal_value};

  vk::SubmitInfo graphics_submit_info{
      .pNext = &graphics_timeline_info,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &*semaphore_,
      .pWaitDstStageMask = &wait_stage,
      .commandBufferCount = 1,
      .pCommandBuffers = &*graphics_command_buffers_[frame_index_],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &*semaphore_};
  graphics_queue_.submit(graphics_submit_info, nullptr);

  return graphics_signal_value;
}

uint32_t render::Renderer::FindQueue(
    const vk::QueueFlags flags,
    const std::unordered_set<uint32_t>& exclude) const {
  const std::vector<vk::QueueFamilyProperties> properties =
      physical_device_.getQueueFamilyProperties();

  uint32_t shared_queue_index = ~0U;

  for (auto [queue_index, property] :
       std::ranges::views::enumerate(properties)) {
    if ((property.queueFlags & flags)) {
      if (!exclude.contains(queue_index)) {
        return queue_index;
      }

      if (shared_queue_index == ~0U) {
        shared_queue_index = queue_index;
      }
    }
  }

  if (shared_queue_index == ~0U) {
    throw std::runtime_error("[ERROR] Vulkan: failed to find a suitable queue");
  }

  return shared_queue_index;
}

vk::raii::ShaderModule render::Renderer::CreateShaderModule(
    const std::vector<char>& code) {
  vk::ShaderModuleCreateInfo create_info{
      .codeSize = code.size(),
      .pCode = reinterpret_cast<const uint32_t*>(code.data())};
  vk::raii::ShaderModule shader_module{device_, create_info};
  return shader_module;
}

std::vector<char> render::Renderer::ReadFile(const std::string& filename) {
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

void render::Renderer::CreateBuffer(vk::DeviceSize size,
                                    vk::BufferUsageFlags usage,
                                    vk::MemoryPropertyFlags properties,
                                    vk::raii::Buffer& buffer,
                                    vk::raii::DeviceMemory& buffer_memory) {
  vk::BufferCreateInfo buffer_info{
      .size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive};
  buffer = vk::raii::Buffer(device_, buffer_info);
  vk::MemoryRequirements mem_requirements = buffer.getMemoryRequirements();
  vk::MemoryAllocateInfo alloc_info{
      .allocationSize = mem_requirements.size,
      .memoryTypeIndex =
          FindMemoryType(mem_requirements.memoryTypeBits, properties)};
  buffer_memory = vk::raii::DeviceMemory(device_, alloc_info);
  buffer.bindMemory(buffer_memory, 0);
}

uint32_t render::Renderer::FindMemoryType(uint32_t type_filter,
                                          vk::MemoryPropertyFlags properties) {
  vk::PhysicalDeviceMemoryProperties mem_properties =
      physical_device_.getMemoryProperties();

  for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
    if (((type_filter & (1 << i)) != 0U) &&
        (mem_properties.memoryTypes.at(i).propertyFlags & properties) ==
            properties) {
      return i;
    }
  }

  throw std::runtime_error(
      "[ERROR] Vulkan: failed to find suitable memory type!");
}

vk::raii::CommandBuffer render::Renderer::BeginSingleTimeTransferCommands()
    const {
  vk::CommandBufferAllocateInfo alloc_info{};
  alloc_info.commandPool = *transfer_command_pool_;
  alloc_info.level = vk::CommandBufferLevel::ePrimary;
  alloc_info.commandBufferCount = 1;
  vk::raii::CommandBuffer command_buffer =
      std::move(vk::raii::CommandBuffers(device_, alloc_info).front());

  vk::CommandBufferBeginInfo begin_info{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
  command_buffer.begin(begin_info);

  return command_buffer;
}

void render::Renderer::EndSingleTimeTransferCommands(
    const vk::raii::CommandBuffer& command_buffer) const {
  command_buffer.end();

  vk::SubmitInfo submit_info{};
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &*command_buffer;
  transfer_queue_.submit(submit_info, nullptr);
  transfer_queue_.waitIdle();
}

void render::Renderer::CopyBuffer(const vk::raii::Buffer& src_buffer,
                                  const vk::raii::Buffer& dst_buffer,
                                  vk::DeviceSize size) const {
  vk::raii::CommandBuffer command_copy_buffer =
      BeginSingleTimeTransferCommands();
  command_copy_buffer.copyBuffer(src_buffer, dst_buffer,
                                 vk::BufferCopy(0, 0, size));
  EndSingleTimeTransferCommands(command_copy_buffer);
}
