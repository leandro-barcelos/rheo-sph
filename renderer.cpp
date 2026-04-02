#include "renderer.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "vulkan/vulkan.hpp"
#include "window.h"

void render::Renderer::Init() {
  CreateInstance();
  PickPhysicalDevice();
  CreateLogicalDevice();
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

void render::Renderer::PickPhysicalDevice() {
  auto physical_devices = instance_.enumeratePhysicalDevices();

  if (physical_devices.empty()) {
    throw std::runtime_error(
        "[ERROR] Vulkan: failed to find GPUs with Vulkan support");
  }

  std::multimap<int, vk::raii::PhysicalDevice> candidates;

  for (const auto& physical_device : physical_devices) {
    auto properties = physical_device.getProperties();
    auto features = physical_device.getFeatures();
    uint32_t score = 0;

    if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
      score += 1000;
    }

    score += properties.limits.maxImageDimension2D;

    if (features.geometryShader == 0U) {
      continue;
    }

    candidates.insert(std::make_pair(score, physical_device));
  }

  if (candidates.rbegin()->first > 0) {
    physical_device_ = candidates.rbegin()->second;
  } else {
    throw std::runtime_error("[ERROR] Vulkan: failed to find a suitable GPU");
  }
}

void render::Renderer::CreateLogicalDevice() {
  graphics_queue_index_ = FindQueue(vk::QueueFlagBits::eGraphics);
  float graphics_queue_priority = 0.5f;
  vk::DeviceQueueCreateInfo graphics_queue_create_info{
      .queueFamilyIndex = graphics_queue_index_,
      .queueCount = 1,
      .pQueuePriorities = &graphics_queue_priority};

  compute_queue_index_ =
      FindQueue(vk::QueueFlagBits::eCompute, {graphics_queue_index_});
  if (compute_queue_index_ == graphics_queue_index_) {
    throw std::runtime_error(
        "[ERROR] Vulkan: failed to find a dedicated compute queue");
  }
  float compute_queue_priority = 0.5f;
  const vk::DeviceQueueCreateInfo compute_queue_create_info{
      .queueFamilyIndex = compute_queue_index_,
      .queueCount = 1,
      .pQueuePriorities = &compute_queue_priority};

  std::vector queue_create_infos = {
      graphics_queue_create_info,
      compute_queue_create_info,
  };

  vk::StructureChain<vk::PhysicalDeviceFeatures2,
                     vk::PhysicalDeviceVulkan13Features,
                     vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
      feature_chain = {
          {}, {.dynamicRendering = true}, {.extendedDynamicState = true}};

  std::vector required_device_extensions = {vk::KHRSwapchainExtensionName};

  vk::DeviceCreateInfo device_create_info{
      .pNext = &feature_chain.get<vk::PhysicalDeviceFeatures2>(),
      .queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
      .pQueueCreateInfos = queue_create_infos.data(),
      .enabledExtensionCount =
          static_cast<uint32_t>(required_device_extensions.size()),
      .ppEnabledExtensionNames = required_device_extensions.data()};

  device_ = vk::raii::Device(physical_device_, device_create_info);
  graphics_queue_ = vk::raii::Queue(device_, graphics_queue_index_, 0);
  compute_queue_ = vk::raii::Queue(device_, compute_queue_index_, 0);
}

uint32_t render::Renderer::FindQueue(
    const vk::QueueFlags flags,
    const std::unordered_set<uint32_t>& exclude) const {
  const std::vector<vk::QueueFamilyProperties> properties =
      physical_device_.getQueueFamilyProperties();

  uint32_t shared_queue_index = ~0U;

  for (uint32_t i = 0; i < properties.size(); i++) {
    if ((properties.at(i).queueFlags & flags)) {
      if (!exclude.contains(i)) {
        return i;
      }

      if (shared_queue_index == ~0U) {
        shared_queue_index = i;
      }
    }
  }

  if (shared_queue_index == ~0U) {
    throw std::runtime_error("[ERROR] Vulkan: failed to find a suitable queue");
  }

  return shared_queue_index;
}
