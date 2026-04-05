#include "renderer.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <map>
#include <ranges>
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
                     vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
      feature_chain = {
          {}, {.dynamicRendering = VK_TRUE}, {.extendedDynamicState = VK_TRUE}};

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
