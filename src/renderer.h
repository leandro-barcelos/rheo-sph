#ifndef RHEOSPH_RENDERER_H
#define RHEOSPH_RENDERER_H

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "glm/fwd.hpp"
#include "vulkan/vulkan.hpp"
#include "window.h"

namespace render {

constexpr uint32_t kMaxFramesInFlight = 2;
constexpr uint32_t kNumThreads = 256;
constexpr uint32_t kClearBucketsNumThreads = 10;
constexpr std::array kValidationLayers{"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool kEnableValidationLayers = false;
#else
constexpr bool kEnableValidationLayers = true;
#endif

struct FluidParticle {
  glm::vec3 position;
  glm::vec3 velocity;
  glm::vec3 distance_traveled;
  glm::vec4 color;
  float density;

  static vk::VertexInputBindingDescription GetBindingDescription();
  static std::array<vk::VertexInputAttributeDescription, 3>
  GetAttributeDescriptions();
} __attribute__((aligned(64)));

struct WallParticle {
  glm::vec3 position;
} __attribute__((aligned(16)));

struct Parameters {
  uint32_t voxel_max_particles;
  uint32_t fluid_particle_count;
  uint32_t wall_particle_count;
  uint32_t total_particle_count;
  glm::uvec4 bucket_size;
  glm::vec4 min_bound;
  glm::vec4 max_bound;
} __attribute__((aligned(64)));

struct RenderInfo {
  uint32_t image_index;
  uint64_t semaphore_wait_value;
} __attribute__((aligned(16)));

class Renderer {
 public:
  void Init(window::Window const& window, Parameters parameters);
  void Update(window::Window const& window);

 private:
  // General
  vk::raii::Context context_;
  vk::raii::Instance instance_ = nullptr;
  vk::raii::PhysicalDevice physical_device_ = nullptr;
  vk::raii::Device device_ = nullptr;
  vk::raii::Queue graphics_queue_ = nullptr;
  uint32_t graphics_queue_index_ = ~0;
  vk::raii::Queue compute_queue_ = nullptr;
  uint32_t compute_queue_index_ = ~0;
  vk::raii::Queue transfer_queue_ = nullptr;
  uint32_t transfer_queue_index_ = ~0;
  vk::raii::DescriptorPool descriptor_pool_ = nullptr;
  vk::raii::CommandPool graphics_command_pool_ = nullptr;
  vk::raii::CommandPool compute_command_pool_ = nullptr;
  vk::raii::CommandPool transfer_command_pool_ = nullptr;

  vk::raii::Semaphore semaphore_ = nullptr;
  std::vector<vk::raii::Fence> in_flight_fences_;
  uint32_t frame_index_ = 0;
  uint64_t timeline_value_ = 0;

  std::vector<const char*> required_device_extensions_ = {
      vk::KHRSwapchainExtensionName};

  void CreateInstance();
  bool IsDeviceSuitable(vk::raii::PhysicalDevice const& physical_device);
  void PickPhysicalDevice();
  void CreateLogicalDevice();
  void CreateDescriptorPool();
  void CreateCommandPools();
  void CreateSyncObjects();

  // Simulation
  Parameters parameters_;

  uint64_t Simulate();

  // Posso criar um command buffer primario
  // pra "pipeline" inteira (processo todo), ou
  // um para cada estágio (por exemplo, para a
  // geração do bucket, etc), e criar command buffers
  // secundarios para cada shader específica. Assim,
  // precisa apenas de um submit.
  // Outra ideia é criar um command buffer para
  // cada shader e fazer vários submits.

  // Bucket Shader
  std::vector<vk::raii::CommandBuffer> bucket_primary_command_buffers_;
  std::vector<vk::raii::CommandBuffer> clear_bucket_secondary_command_buffers_;
  std::vector<vk::raii::CommandBuffer> fluid_bucket_secondary_command_buffers_;
  std::vector<vk::raii::CommandBuffer> wall_bucket_secondary_command_buffers_;
  vk::raii::PipelineLayout bucket_pipeline_layout_ = nullptr;
  vk::raii::Pipeline clear_bucket_pipeline_ = nullptr;
  vk::raii::Pipeline fluid_bucket_pipeline_ = nullptr;
  vk::raii::Pipeline wall_bucket_pipeline_ = nullptr;
  vk::raii::DescriptorSetLayout bucket_descriptor_set_layout_ = nullptr;
  vk::raii::Buffer parameters_uniform_buffer_ = nullptr;
  vk::raii::DeviceMemory parameters_uniform_buffer_memory_ = nullptr;
  std::vector<vk::raii::Buffer> fluid_particles_buffers_;
  std::vector<vk::raii::DeviceMemory> fluid_particles_buffers_memory_;
  std::vector<vk::raii::Buffer> wall_particles_buffers_;
  std::vector<vk::raii::DeviceMemory> wall_particles_buffers_memory_;
  std::vector<vk::raii::Buffer> bucket_buffers_;
  std::vector<vk::raii::DeviceMemory> bucket_buffers_memory_;
  std::vector<vk::raii::DescriptorSet> fluid_bucket_descriptor_sets_;

  void CreateClearBucketPipeline();
  void CreateFluidBucketPipeline();
  void CreateWallBucketPipeline();
  void CreateBucketCommandBuffers();
  void CreateBucketDescriptorSetLayout();
  void CreateBucketUniformBuffer();
  void CreateFluidParticlesStorageBuffers();
  void CreateWallParticlesStorageBuffers();
  void CreateBucketParametersBuffers();
  void CreateFluidBucketDescriptorSets();
  void RecordBucketPrimaryCommandBuffer();
  void RecordClearBucketCommandBuffer();
  void RecordFluidBucketCommandBuffer();
  void RecordWallBucketCommandBuffer();

  // Graphics & Presentation
  vk::raii::SurfaceKHR surface_ = nullptr;
  std::vector<vk::raii::CommandBuffer> graphics_command_buffers_;
  vk::raii::SwapchainKHR swap_chain_ = nullptr;
  std::vector<vk::Image> swap_chain_images_;
  std::vector<vk::ImageLayout> swap_chain_image_layouts_;
  vk::SurfaceFormatKHR swap_chain_surface_format_;
  vk::Extent2D swap_chain_extent_;
  std::vector<vk::raii::ImageView> swap_chain_image_views_;
  vk::raii::PipelineLayout graphics_pipeline_layout_ = nullptr;
  vk::raii::Pipeline graphics_pipeline_ = nullptr;

  bool framebuffer_resized_ = false;

  void CreateSurface(window::Window const& window);
  void CreateSwapChain(window::Window const& window);
  void CreateImageViews();
  static vk::Extent2D ChooseSwapExtent(
      window::Window const& window,
      vk::SurfaceCapabilitiesKHR const& surface_capabilities);
  static uint32_t ChooseSwapMinImageCount(
      vk::SurfaceCapabilitiesKHR const& surface_capabilities);
  static vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(
      std::vector<vk::SurfaceFormatKHR> const& available_formats);
  static vk::PresentModeKHR ChooseSwapPresentMode(
      std::vector<vk::PresentModeKHR> const& available_present_modes);
  void CreateGraphicsCommandBuffers();
  void TransitionImageLayout(uint32_t image_index, vk::ImageLayout old_layout,
                             vk::ImageLayout new_layout,
                             vk::AccessFlags2 src_access_mask,
                             vk::AccessFlags2 dst_access_mask,
                             vk::PipelineStageFlags2 src_stage_mask,
                             vk::PipelineStageFlags2 dst_stage_mask);
  void RecordGraphicsCommandBuffer(uint32_t image_index);
  void CleanupSwapChain();
  void RecreateSwapChain(window::Window const& window);
  void CreateGraphicsPipeline();
  uint64_t Render(RenderInfo info);

  // Utils
  [[nodiscard]] uint32_t FindQueue(
      vk::QueueFlags flags,
      const std::unordered_set<uint32_t>& exclude = {}) const;
  [[nodiscard]] vk::raii::ShaderModule CreateShaderModule(
      const std::vector<char>& code);
  static std::vector<char> ReadFile(const std::string& filename);
  void CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                    vk::MemoryPropertyFlags properties,
                    vk::raii::Buffer& buffer,
                    vk::raii::DeviceMemory& buffer_memory);
  uint32_t FindMemoryType(uint32_t type_filter,
                          vk::MemoryPropertyFlags properties);
  [[nodiscard]] vk::raii::CommandBuffer BeginSingleTimeTransferCommands() const;
  void EndSingleTimeTransferCommands(
      const vk::raii::CommandBuffer& command_buffer) const;
  void CopyBuffer(const vk::raii::Buffer& src_buffer,
                  const vk::raii::Buffer& dst_buffer,
                  vk::DeviceSize size) const;
};
}  // namespace render

#endif  // !RHEOSPH_RENDERER_H
