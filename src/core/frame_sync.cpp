#include "frame_sync.h"

void core::FrameSync::Init(core::VulkanDevice const& vulkan_device,
                           uint32_t max_frames_in_flight) {
  max_frames_in_flight_ = max_frames_in_flight;
  in_flight_fences_.clear();

  vk::SemaphoreTypeCreateInfo semaphore_type{
      .semaphoreType = vk::SemaphoreType::eTimeline, .initialValue = 0};
  vk::SemaphoreCreateInfo semaphore_info{.pNext = &semaphore_type};
  semaphore_ = vk::raii::Semaphore(vulkan_device.Device(), semaphore_info);
  timeline_value_ = 0;

  for (size_t i = 0; i < max_frames_in_flight_; i++) {
    vk::FenceCreateInfo fence_info{};
    in_flight_fences_.emplace_back(vulkan_device.Device(), fence_info);
  }
}

void core::FrameSync::WaitForFence(
    core::VulkanDevice const& vulkan_device) const {
  auto fence_result =
      vulkan_device.Device().waitForFences(*Fence(), vk::True, UINT64_MAX);
  if (fence_result != vk::Result::eSuccess) {
    throw std::runtime_error("[ERROR] Vulkan: failed to wait for fence!");
  }
  vulkan_device.Device().resetFences(*Fence());
}

void core::FrameSync::WaitSemaphore(core::VulkanDevice const& vulkan_device,
                                    uint64_t wait_value) const {
  vk::SemaphoreWaitInfo wait_info{.semaphoreCount = 1,
                                  .pSemaphores = &*semaphore_,
                                  .pValues = &wait_value};

  auto result = vulkan_device.Device().waitSemaphores(wait_info, UINT64_MAX);
  if (result != vk::Result::eSuccess) {
    throw std::runtime_error("[ERROR] Vulkan: failed to wait for semaphore!");
  }
}
