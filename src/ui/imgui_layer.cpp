#include "imgui_layer.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <stdexcept>

#include "IconsFontAwesome6.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

namespace {

std::string ResolveIconFontPath() {
  constexpr std::array kCandidates = {
      "resources/fonts/fa-solid-900.ttf",
      "../resources/fonts/fa-solid-900.ttf",
      "../../resources/fonts/fa-solid-900.ttf",
  };

  for (const char* candidate : kCandidates) {
    if (std::filesystem::exists(candidate)) {
      return candidate;
    }
  }

  throw std::runtime_error("[ERROR] ImGui: Font Awesome TTF not found");
}

}  // namespace

ui::ImGuiLayer::~ImGuiLayer() { Shutdown(); }

void ui::ImGuiLayer::Init(core::Window const& window,
                          core::VulkanContext const& context,
                          core::VulkanDevice const& vulkan_device,
                          core::VulkanSwapChain const& vulkan_swap_chain) {
  if (initialized_) {
    return;
  }

  constexpr std::array kPoolSizes = {
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eSampler,
                 .descriptorCount = 1000},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eCombinedImageSampler,
                 .descriptorCount = 1000},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eSampledImage,
                 .descriptorCount = 1000},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageImage,
                 .descriptorCount = 1000},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformTexelBuffer,
                 .descriptorCount = 1000},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageTexelBuffer,
                 .descriptorCount = 1000},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer,
                 .descriptorCount = 1000},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer,
                 .descriptorCount = 1000},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBufferDynamic,
                 .descriptorCount = 1000},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBufferDynamic,
                 .descriptorCount = 1000},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eInputAttachment,
                 .descriptorCount = 1000},
  };

  vk::DescriptorPoolCreateInfo descriptor_pool_info{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = 1000 * static_cast<uint32_t>(kPoolSizes.size()),
      .poolSizeCount = static_cast<uint32_t>(kPoolSizes.size()),
      .pPoolSizes = kPoolSizes.data()};
  descriptor_pool_ =
      vk::raii::DescriptorPool(vulkan_device.Device(), descriptor_pool_info);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& imgui_io = ImGui::GetIO();
  imgui_io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  imgui_io.Fonts->AddFontDefault();

  ImFontConfig icon_font_config;
  icon_font_config.MergeMode = true;
  icon_font_config.GlyphMinAdvanceX = 13.0F;

  static constexpr std::array<ImWchar, 3> kIconGlyphRanges = {
      ICON_MIN_FA,
      ICON_MAX_16_FA,
      0,
  };

  std::string icon_font_path = ResolveIconFontPath();
  if (imgui_io.Fonts->AddFontFromFileTTF(icon_font_path.c_str(), 13.0F,
                                         &icon_font_config,
                                         kIconGlyphRanges.data()) == nullptr) {
    throw std::runtime_error("[ERROR] ImGui: failed to load Font Awesome TTF");
  }

  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 0.0F;
  style.FrameRounding = 2.0F;
  style.GrabRounding = 2.0F;
  style.ScrollbarRounding = 2.0F;
  style.WindowBorderSize = 1.0F;
  style.FrameBorderSize = 1.0F;

  ImGui_ImplGlfw_InitForVulkan(window.Handle(), true);

  auto color_attachment_format =
      static_cast<VkFormat>(vulkan_swap_chain.SurfaceFormat().format);
  VkPipelineRenderingCreateInfoKHR rendering_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &color_attachment_format};

  min_image_count_ = std::max(2U, vulkan_swap_chain.ImageCount());

  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.ApiVersion = VK_API_VERSION_1_4;
  init_info.Instance = *context.Instance();
  init_info.PhysicalDevice = *vulkan_device.PhysicalDevice();
  init_info.Device = *vulkan_device.Device();
  init_info.QueueFamily = vulkan_device.GraphicsQueueFamilyIndex();
  init_info.Queue = *vulkan_device.GraphicsQueue();
  init_info.DescriptorPool = *descriptor_pool_;
  init_info.MinImageCount = min_image_count_;
  init_info.ImageCount = vulkan_swap_chain.ImageCount();
  init_info.UseDynamicRendering = true;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo = rendering_info;
  init_info.CheckVkResultFn = CheckVkResult;

  if (!ImGui_ImplVulkan_Init(&init_info)) {
    throw std::runtime_error("[ERROR] ImGui: failed to initialize Vulkan backend");
  }

  initialized_ = true;
}

void ui::ImGuiLayer::BeginFrame() const {
  if (!initialized_) {
    return;
  }

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void ui::ImGuiLayer::EndFrame() const {
  if (!initialized_) {
    return;
  }

  ImGui::Render();
}

void ui::ImGuiLayer::Render(vk::raii::CommandBuffer const& command_buffer) const {
  if (!initialized_) {
    return;
  }

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *command_buffer);
}

void ui::ImGuiLayer::OnSwapChainRecreated(
    core::VulkanSwapChain const& vulkan_swap_chain) {
  if (!initialized_) {
    return;
  }

  uint32_t const next_min_image_count =
      std::max(2U, vulkan_swap_chain.ImageCount());
  if (next_min_image_count != min_image_count_) {
    min_image_count_ = next_min_image_count;
    ImGui_ImplVulkan_SetMinImageCount(min_image_count_);
  }
}

void ui::ImGuiLayer::Shutdown() {
  if (!initialized_) {
    return;
  }

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  descriptor_pool_.clear();

  initialized_ = false;
  min_image_count_ = 0;
}

void ui::ImGuiLayer::CheckVkResult(VkResult result) {
  if (result < 0) {
    throw std::runtime_error("[ERROR] ImGui: Vulkan backend call failed");
  }
}
