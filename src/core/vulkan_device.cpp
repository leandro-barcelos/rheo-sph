#include "vulkan_device.h"

#include <ranges>
#include <set>

#include "vulkan/vulkan.hpp"
#include "vulkan_context.h"

[[nodiscard]] bool core::QueueFamilyIndices::IsComplete() const {
  return graphics_.has_value() && compute_.has_value() && present_.has_value();
}

[[nodiscard]] bool core::QueueFamilyIndices::HasAsyncCompute() const {
  return compute_.has_value() && compute_ != graphics_;
}

void core::VulkanDevice::Init(VulkanContext const& vulkan_context,
                              vk::SurfaceKHR surface) {
  PickPhysicalDevice(vulkan_context, surface);
  CreateLogicalDevice(surface);
}

void core::VulkanDevice::PickPhysicalDevice(
    VulkanContext const& vulkan_context, vk::SurfaceKHR surface) {
  const auto physical_devices =
      vulkan_context.Instance().enumeratePhysicalDevices();
  auto const dev_iter =
      std::ranges::find_if(physical_devices, [&](auto const& physical_device) {
        return IsDeviceSuitable(physical_device, surface);
      });
  if (dev_iter == physical_devices.end()) {
    throw std::runtime_error("[ERROR] Vulkan: failed to find a suitable GPU");
  }
  physical_device_ = *dev_iter;
}

void core::VulkanDevice::CreateLogicalDevice(vk::SurfaceKHR surface) {
  FindQueues(surface);

  std::set unique_families = {
      *queue_indices_.Graphics(),
      *queue_indices_.Compute(),
      *queue_indices_.Present(),
  };

  float priority = 1.0F;
  std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
  for (uint32_t family : unique_families) {
    vk::DeviceQueueCreateInfo info{.queueFamilyIndex = family,
                                   .queueCount = 1,
                                   .pQueuePriorities = &priority};
    queue_create_infos.push_back(info);
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

  RetrieveQueues();
}

void core::VulkanDevice::FindQueues(vk::SurfaceKHR surface) {
  const std::vector<vk::QueueFamilyProperties> properties =
      physical_device_.getQueueFamilyProperties();

  for (auto [queue_index, property] :
       std::ranges::views::enumerate(properties)) {
    const auto family_index = static_cast<uint32_t>(queue_index);

    if (property.queueFlags & vk::QueueFlagBits::eGraphics) {
      queue_indices_.SetGraphics(family_index);
    }

    if (property.queueFlags & vk::QueueFlagBits::eCompute) {
      if (!queue_indices_.Compute().has_value() &&
          (!queue_indices_.Graphics().has_value() ||
           queue_index != queue_indices_.Graphics().value())) {
        queue_indices_.SetCompute(family_index);
      }
    }

    if (physical_device_.getSurfaceSupportKHR(family_index, surface) != 0U) {
      queue_indices_.SetPresent(family_index);
    }
  }

  if (!queue_indices_.IsComplete() || !queue_indices_.HasAsyncCompute()) {
    throw std::runtime_error("[ERROR] Vulkan: failed to find required queues!");
  }
}

void core::VulkanDevice::RetrieveQueues() {
  graphics_queue_ =
      vk::raii::Queue(device_, queue_indices_.Graphics().value(), 0);
  compute_queue_ =
      vk::raii::Queue(device_, queue_indices_.Compute().value(), 0);
  present_queue_ = vk::raii::Queue(device_, queue_indices_.Present().value(), 0);
}

bool core::VulkanDevice::IsDeviceSuitable(
    vk::raii::PhysicalDevice const& physical_device,
    vk::SurfaceKHR surface) const {
  bool supports_vulkan_1_3 =
      physical_device.getProperties().apiVersion >= VK_API_VERSION_1_3;

  auto queue_families = physical_device.getQueueFamilyProperties();
  bool supports_graphics =
      std::ranges::any_of(queue_families, [](auto const& qfp) {
        return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
      });
  bool supports_present = false;
  for (size_t queue_index = 0; queue_index < queue_families.size(); ++queue_index) {
        if (physical_device.getSurfaceSupportKHR(
          static_cast<uint32_t>(queue_index), surface) != 0U) {
      supports_present = true;
      break;
    }
  }

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

  auto features = physical_device.getFeatures2<
      vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
      vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
      vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR>();
  bool supports_required_features =
      (features.get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy !=
       0U) &&
      (features.get<vk::PhysicalDeviceFeatures2>()
           .features.shaderTessellationAndGeometryPointSize != 0U) &&
      (features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering !=
       0U) &&
      (features.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 !=
       0U) &&
      (features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
           .extendedDynamicState != 0U) &&
      (features.get<vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR>()
           .timelineSemaphore != 0U);

  return supports_vulkan_1_3 && supports_graphics && supports_present &&
         supports_all_required_extensions && supports_required_features;
}
