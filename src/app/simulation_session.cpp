#include "simulation_session.h"

namespace app {

void SimulationSession::ApplyParameters(
    simulation::FluidSimulator::Parameters const& params,
    core::VulkanDevice const& vulkan_device,
    core::CommandPools const& command_pools) {
  vulkan_device_ = &vulkan_device;
  command_pools_ = &command_pools;

  parameters_ = params;
  parameters_dirty_ = true;
}

std::optional<uint64_t> SimulationSession::Tick(
    core::VulkanDevice const& vulkan_device, core::FrameSync& frame_sync,
    double delta_ms) {
  if (!running_ || fluid_simulator_ == nullptr) {
    return std::nullopt;
  }

  return fluid_simulator_->Run(vulkan_device, frame_sync, delta_ms);
}

void SimulationSession::Play() {
  if (parameters_dirty_) {
    if (vulkan_device_ != nullptr && command_pools_ != nullptr) {
      Recreate(*vulkan_device_, *command_pools_);
    }
  }

  if (fluid_simulator_ != nullptr) {
    running_ = true;
  }
}

void SimulationSession::Pause() { running_ = false; }

void SimulationSession::Reset(core::VulkanDevice const& vulkan_device,
                             core::CommandPools const& command_pools) {
  vulkan_device_ = &vulkan_device;
  command_pools_ = &command_pools;

  Recreate(vulkan_device, command_pools);
  running_ = false;
}

simulation::FluidSimulator const* SimulationSession::Simulator() const {
  return fluid_simulator_.get();
}

void SimulationSession::Recreate(core::VulkanDevice const& vulkan_device,
                                core::CommandPools const& command_pools) {
  if (!parameters_.has_value()) {
    return;
  }

  vulkan_device.Device().waitIdle();

  fluid_simulator_.reset();
  fluid_simulator_ = std::make_unique<simulation::FluidSimulator>(*parameters_);
  fluid_simulator_->Init(vulkan_device, command_pools);

  parameters_dirty_ = false;
}

}  // namespace app
