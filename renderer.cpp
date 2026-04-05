#include "renderer.h"

#include <sys/types.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "vulkan/vulkan.hpp"
#include "window.h"

void render::Renderer::Init(Parameters parameters) {
  parameters_ = parameters;

  CreateInstance();
  PickPhysicalDevice();
  CreateLogicalDevice();
  CreateBucketDescriptorSetLayout();
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
  CreateBucketCommandBuffers();
  CreateSyncObjects();
}

void render::Renderer::Update() { Simulate(); }

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
      (features.template get<vk::PhysicalDeviceVulkan13Features>()
           .dynamicRendering != 0U) &&
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
      feature_chain = {{},
                       {.dynamicRendering = vk::True},
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
    vk::FenceCreateInfo fence_info{.flags = vk::FenceCreateFlagBits::eSignaled};
    in_flight_fences_.emplace_back(device_, fence_info);
  }
}

void render::Renderer::Simulate() {
  auto fence_result = device_.waitForFences(*in_flight_fences_[frame_index_],
                                            vk::True, UINT64_MAX);
  if (fence_result != vk::Result::eSuccess) {
    throw std::runtime_error("[ERROR] Vulkan: failed to wait for fence!");
  }
  device_.resetFences(*in_flight_fences_[frame_index_]);

  uint64_t fluid_bucket_wait_value = timeline_value_;
  uint64_t fluid_bucket_signal_value = ++timeline_value_;

  RecordClearBucketCommandBuffer();
  RecordFluidBucketCommandBuffer();
  RecordWallBucketCommandBuffer();
  RecordBucketPrimaryCommandBuffer();

  vk::TimelineSemaphoreSubmitInfo fluid_bucket_timeline_info{
      .waitSemaphoreValueCount = 1,
      .pWaitSemaphoreValues = &fluid_bucket_wait_value,
      .signalSemaphoreValueCount = 1,
      .pSignalSemaphoreValues = &fluid_bucket_signal_value};

  std::array<vk::PipelineStageFlags, 1> wait_stages{
      vk::PipelineStageFlagBits::eComputeShader};

  vk::SubmitInfo fluid_bucket_submit_info{
      .pNext = &fluid_bucket_timeline_info,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &*semaphore_,
      .pWaitDstStageMask = wait_stages.data(),
      .commandBufferCount = 1,
      .pCommandBuffers = &*bucket_primary_command_buffers_[frame_index_],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &*semaphore_};

  compute_queue_.submit(fluid_bucket_submit_info,
                        in_flight_fences_[frame_index_]);

  frame_index_ = (frame_index_ + 1) % kMaxFramesInFlight;
}

void render::Renderer::CreateFluidBucketPipeline() {
  vk::raii::ShaderModule bucket_shader_module =
      CreateShaderModule(ReadFile("shaders/bucket.spv"));

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
      CreateShaderModule(ReadFile("shaders/bucket.spv"));

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
      CreateShaderModule(ReadFile("shaders/bucket.spv"));

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
