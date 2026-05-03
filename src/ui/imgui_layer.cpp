#include "imgui_layer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <stdexcept>

#include "IconsFontAwesome6.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

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

  SetupImGuiStyle();

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
    throw std::runtime_error(
        "[ERROR] ImGui: failed to initialize Vulkan backend");
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

void ui::ImGuiLayer::Render(
    vk::raii::CommandBuffer const& command_buffer) const {
  if (!initialized_) {
    return;
  }

  ImDrawData* draw_data = ImGui::GetDrawData();
  if (draw_data != nullptr) {
    for (int list_index = 0; list_index < draw_data->CmdListsCount;
         ++list_index) {
      ImDrawList* cmd_list = draw_data->CmdLists[list_index];
      if (cmd_list == nullptr) {
        continue;
      }

      for (ImDrawCmd& cmd : cmd_list->CmdBuffer) {
        if (cmd.TexRef._TexData != nullptr) {
          continue;
        }
      }
    }
  }

  ImGui_ImplVulkan_RenderDrawData(draw_data, *command_buffer);
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

void ui::ImGuiLayer::SetupImGuiStyle() {
  ImGuiStyle& style = ImGui::GetStyle();
  auto& colors = style.Colors;

  // --- 1. Sizing and Spacing ---
  style.WindowPadding = ImVec2(10.0F, 10.0F);
  style.FramePadding = ImVec2(6.0F, 4.0F);
  style.ItemSpacing = ImVec2(8.0F, 4.0F);
  style.ScrollbarSize = 15.0F;
  style.GrabMinSize = 10.0F;

  // --- 2. Borders & Rounding ---
  style.WindowRounding = 5.0F;
  style.FrameRounding = 4.0F;
  style.PopupRounding = 4.0F;
  style.ScrollbarRounding = 12.0F;
  style.GrabRounding = 3.0F;
  style.TabRounding = 4.0F;

  style.WindowBorderSize = 1.0F;
  style.FrameBorderSize = 1.0F;

  // --- 3. Color Palette ---

  // Text
  colors[ImGuiCol_Text] = ImVec4(0.90F, 0.93F, 0.97F, 1.00F);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.40F, 0.50F, 0.65F, 1.00F);

  // Backgrounds
  colors[ImGuiCol_WindowBg] =
      ImVec4(0.07F, 0.09F, 0.12F, 1.00F);  // Deep midnight
  colors[ImGuiCol_ChildBg] = ImVec4(0.09F, 0.12F, 0.16F, 1.00F);
  colors[ImGuiCol_PopupBg] = ImVec4(0.07F, 0.09F, 0.12F, 0.95F);

  // Borders
  colors[ImGuiCol_Border] = ImVec4(0.15F, 0.25F, 0.35F, 0.70F);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00F, 0.00F, 0.00F, 0.00F);

  // Frames (Inputs, Checkboxes, etc.)
  colors[ImGuiCol_FrameBg] = ImVec4(0.12F, 0.18F, 0.26F, 1.00F);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18F, 0.28F, 0.40F, 1.00F);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.25F, 0.38F, 0.55F, 1.00F);

  // Title Bars
  colors[ImGuiCol_TitleBg] = ImVec4(0.09F, 0.12F, 0.18F, 1.00F);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.14F, 0.22F, 0.35F, 1.00F);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.05F, 0.08F, 0.12F, 1.00F);

  // Menus
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.12F, 0.16F, 0.22F, 1.00F);

  // Scrollbars
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.06F, 0.08F, 0.11F, 1.00F);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20F, 0.32F, 0.48F, 1.00F);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28F, 0.42F, 0.60F, 1.00F);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.35F, 0.50F, 0.75F, 1.00F);

  // Interactables
  colors[ImGuiCol_CheckMark] = ImVec4(0.40F, 0.70F, 1.00F, 1.00F);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.30F, 0.55F, 0.85F, 1.00F);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.45F, 0.75F, 1.00F, 1.00F);
  colors[ImGuiCol_Button] = ImVec4(0.18F, 0.35F, 0.55F, 1.00F);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.25F, 0.48F, 0.75F, 1.00F);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.35F, 0.60F, 0.90F, 1.00F);
  colors[ImGuiCol_Header] = ImVec4(0.18F, 0.35F, 0.55F, 1.00F);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.25F, 0.48F, 0.75F, 1.00F);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.35F, 0.60F, 0.90F, 1.00F);

  // Tabs
  colors[ImGuiCol_Tab] = ImVec4(0.12F, 0.20F, 0.32F, 1.00F);
  colors[ImGuiCol_TabHovered] = ImVec4(0.25F, 0.45F, 0.70F, 1.00F);
  colors[ImGuiCol_TabActive] = ImVec4(0.18F, 0.35F, 0.55F, 1.00F);
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.08F, 0.12F, 0.18F, 1.00F);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.12F, 0.20F, 0.32F, 1.00F);

  // Tables
  colors[ImGuiCol_TableHeaderBg] = ImVec4(0.15F, 0.25F, 0.40F, 1.00F);
  colors[ImGuiCol_TableBorderStrong] = ImVec4(0.20F, 0.35F, 0.55F, 1.00F);
  colors[ImGuiCol_TableBorderLight] = ImVec4(0.15F, 0.25F, 0.40F, 1.00F);
  colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00F, 1.00F, 1.00F, 0.05F);

  // Misc
  colors[ImGuiCol_TextSelectedBg] = ImVec4(0.30F, 0.55F, 0.85F, 0.40F);
  colors[ImGuiCol_DragDropTarget] = ImVec4(0.50F, 0.80F, 1.00F, 0.90F);
  colors[ImGuiCol_NavHighlight] = ImVec4(0.40F, 0.70F, 1.00F, 1.00F);

#ifdef IMGUI_HAS_DOCK
  colors[ImGuiCol_DockingPreview] = ImVec4(0.25F, 0.50F, 0.80F, 0.50F);
  colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.07F, 0.09F, 0.12F, 1.00F);
#endif
}
