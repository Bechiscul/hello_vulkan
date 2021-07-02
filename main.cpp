#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>
#include <algorithm>


#include <windows.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *) {
  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ||
      messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    std::cout << pCallbackData->pMessage << '\n';
  }

  return VK_FALSE;
}

// A physical device does not necessarily have presentation support,
// so if doing this properly create a fake window and check for presentation
// support. Before creating the actual surface.
struct QueueFamilyIndices {
  std::optional<uint32_t> graphics_family;

  inline constexpr operator bool() const noexcept {
    return graphics_family.has_value();
  }
};

static VkExtent2D framebuffer = {};

LRESULT __stdcall window_callback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept
{
    switch (msg) {
    case WM_SIZE:
        framebuffer.width = LOWORD(lparam);
        framebuffer.height = HIWORD(lparam);
        break;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

int main() {
  // 1. Create Window.
  WNDCLASSEXW wcx{
      .cbSize = sizeof(WNDCLASSEXW),
      .style = CS_OWNDC,
      .lpfnWndProc = window_callback,
      .cbClsExtra = 0,
      .cbWndExtra = 0,
      .hInstance = GetModuleHandleW(NULL),
      .hIcon = NULL,
      .hCursor = NULL,
      .hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)),
      .lpszMenuName = NULL,
      .lpszClassName = L"hello_vulkan",
      .hIconSm = NULL,
  };

  if (RegisterClassExW(&wcx) == 0) {
    throw std::runtime_error("Failed to register window class!");
  }

  HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, wcx.lpszClassName,
                              L"Hello Vulkan", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                              CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                              CW_USEDEFAULT, NULL, NULL, wcx.hInstance, NULL);

  if (hwnd == NULL) {
    throw std::runtime_error("Failed to create window!");
  }

  // 2. Create vulkan instance.
  VkApplicationInfo application_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = nullptr,
      .pApplicationName = "HelloVulkan",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(0, 0, 0),
      .apiVersion = VK_API_VERSION_1_2,
  };

  std::vector<const char *> instance_layers = {};

#ifdef _DEBUG
  instance_layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

  std::vector<const char *> instance_extensions = {
      VK_KHR_SURFACE_EXTENSION_NAME, "VK_KHR_win32_surface"};

#ifdef _DEBUG
  instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  VkInstanceCreateInfo instance_create_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .pApplicationInfo = &application_info,
      .enabledLayerCount = static_cast<uint32_t>(instance_layers.size()),
      .ppEnabledLayerNames = instance_layers.data(),
      .enabledExtensionCount =
          static_cast<uint32_t>(instance_extensions.size()),
      .ppEnabledExtensionNames = instance_extensions.data(),
  };

  VkInstance instance = VK_NULL_HANDLE;
  if (vkCreateInstance(&instance_create_info, nullptr, &instance) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create vulkan instance.");
  }

  // 3. Enable the debug messenger.
#ifdef _DEBUG
  auto fn_vkCreateDebugUtilsMessengerEXT =
      reinterpret_cast<decltype(vkCreateDebugUtilsMessengerEXT) *>(
          vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
  if (fn_vkCreateDebugUtilsMessengerEXT == nullptr) {
    throw std::runtime_error("Failed to find vkCreateDebugUtilsMessengerEXT");
  }

  VkDebugUtilsMessageSeverityFlagsEXT message_severity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

  VkDebugUtilsMessageTypeFlagsEXT message_type =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

  VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .pNext = nullptr,
      .flags = 0,
      .messageSeverity = message_severity,
      .messageType = message_type,
      .pfnUserCallback = debug_callback,
      .pUserData = nullptr,
  };
  VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
  if (fn_vkCreateDebugUtilsMessengerEXT(instance, &debug_messenger_create_info,
                                        nullptr,
                                        &debug_messenger) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create debug messenger!");
  }
#endif

  // 4. Pick physical device.
  uint32_t physical_device_count = 0;
  if (vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr)) {
    throw std::runtime_error("Failed to enumerate physical devices!");
  }

  if (physical_device_count == 0) {
    throw std::runtime_error("Failed to find GPU with vulkan support!");
  }

  std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
  vkEnumeratePhysicalDevices(instance, &physical_device_count,
                             physical_devices.data());

  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  QueueFamilyIndices queue_family_indices = {};
  for (auto it = physical_devices.begin(); it != physical_devices.end(); ++it) {
    physical_device = *it;

    uint32_t queue_family_property_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &queue_family_property_count, nullptr);

    if (queue_family_property_count == 0) {
      continue;
    }

    std::vector<VkQueueFamilyProperties> queue_family_properties(
        queue_family_property_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                             &queue_family_property_count,
                                             queue_family_properties.data());

    uint32_t i = 0;
    for (const VkQueueFamilyProperties &queue_family_property :
         queue_family_properties) {
      if (queue_family_property.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        queue_family_indices.graphics_family = i;
      }

      i++;
    }

    if (queue_family_indices) {
      break;
    }
  }

  if (!queue_family_indices) {
    throw std::runtime_error(
        "Failed to find physical device with required queues");
  }

  // 5. Create logical device.
  float queue_family_priority = 0.0f;
  std::vector<VkDeviceQueueCreateInfo> device_queue_create_infos = {
      {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
       .pNext = nullptr,
       .flags = 0,
       .queueFamilyIndex = *queue_family_indices.graphics_family,
       .queueCount = 1,
       .pQueuePriorities = &queue_family_priority}};

  std::vector<const char *> device_extensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  VkDeviceCreateInfo device_create_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .queueCreateInfoCount =
          static_cast<uint32_t>(device_queue_create_infos.size()),
      .pQueueCreateInfos = device_queue_create_infos.data(),
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = nullptr,
      .enabledExtensionCount = static_cast<uint32_t>(device_extensions.size()),
      .ppEnabledExtensionNames = device_extensions.data(),
      .pEnabledFeatures = nullptr,
  };

  VkDevice logical_device = VK_NULL_HANDLE;
  if (vkCreateDevice(physical_device, &device_create_info, nullptr,
                     &logical_device) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create logical device!");
  }

  // 6. Get the device queues.
  VkQueue graphics_queue;
  VkQueue present_queue;
  vkGetDeviceQueue(logical_device, *queue_family_indices.graphics_family, 0,
                   &graphics_queue);

  // This is bad because the graphics queue doesn't necessarily support
  // presentation, but for the most part they will end up being the same.
  present_queue = graphics_queue;

  // 7. Create the surface.
  VkWin32SurfaceCreateInfoKHR surface_create_info = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .pNext = nullptr,
      .flags = 0,
      .hinstance = wcx.hInstance,
      .hwnd = hwnd};

  VkSurfaceKHR surface;
  if (vkCreateWin32SurfaceKHR(instance, &surface_create_info, nullptr,
                              &surface) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create surface!");
  }

  // 8. Create the swapchain.
  VkSurfaceCapabilitiesKHR surface_capabilities;
  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
          physical_device, surface, &surface_capabilities) != VK_SUCCESS) {
    throw std::runtime_error("Failed to get surface capabilities!");
  }

  uint32_t surface_format_count = 0;
  if (vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                           &surface_format_count, nullptr)) {
    throw std::runtime_error("Failed to get surface formats!");
  }

  if (surface_format_count == 0) {
    throw std::runtime_error("Unable to find any surface formats!");
  }

  std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(
      physical_device, surface, &surface_format_count, surface_formats.data());

  uint32_t surface_present_mode_count = 0;
  if (vkGetPhysicalDeviceSurfacePresentModesKHR(
          physical_device, surface, &surface_present_mode_count, nullptr)) {
    throw std::runtime_error("Failed to get surface present modes!");
  }

  if (surface_present_mode_count == 0) {
    throw std::runtime_error("Unable to find any surface present modes!");
  }

  std::vector<VkPresentModeKHR> surface_present_modes(
      surface_present_mode_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                            &surface_format_count,
                                            surface_present_modes.data());

  VkSurfaceFormatKHR surface_format;
  bool found_surface_format = false;
  for (const VkSurfaceFormatKHR &available_format : surface_formats) {
    if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
        available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      surface_format = available_format;
      found_surface_format = true;
      break;
    }
  }

  if (!found_surface_format) {
    surface_format = surface_formats.front();
  }

  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
  for (const VkPresentModeKHR available_mode : surface_present_modes) {
    if (available_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      present_mode = available_mode;
      break;
    }
  }

  VkExtent2D swap_extent;
  if (surface_capabilities.currentExtent.width != UINT32_MAX) {
      swap_extent = surface_capabilities.currentExtent;
  }
  else {
      swap_extent.width = std::clamp(framebuffer.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
      swap_extent.height = std::clamp(framebuffer.width, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
  }

  VkSwapchainCreateInfoKHR swapchain_create_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext = nullptr,
      .flags = 0,
      .surface = surface,
      .minImageCount = surface_capabilities.minImageCount + 1,
      .imageFormat = surface_format.format,
      .imageColorSpace = surface_format.colorSpace,
      .imageExtent = swap_extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, // Only because we don't need graphics- and presentqueue as seperate.
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .preTransform = surface_capabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = present_mode,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
  };

  VkSwapchainKHR swapchain;
  if (vkCreateSwapchainKHR(logical_device, &swapchain_create_info, nullptr, &swapchain) != VK_SUCCESS)
  {
      throw std::runtime_error("Failed to create swapchain!");
  }

  uint32_t image_count = 0;
  vkGetSwapchainImagesKHR(logical_device, swapchain, &image_count, nullptr);
  std::vector<VkImage> images(image_count);
  vkGetSwapchainImagesKHR(logical_device, swapchain, &image_count, images.data());

  MSG msg;
  while (GetMessageW(&msg, hwnd, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  vkDestroySwapchainKHR(logical_device, swapchain, nullptr);
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkDestroyDevice(logical_device, nullptr);

#ifdef _DEBUG
  auto fn_vkDestroyDebugUtilsMessengerEXT =
      reinterpret_cast<decltype(vkDestroyDebugUtilsMessengerEXT) *>(
          vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
  if (fn_vkDestroyDebugUtilsMessengerEXT == nullptr) {
    throw std::runtime_error(
        "Failed to find vkDestroyDebugUtilsMessengerEXT in the instance!");
  }

  fn_vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
#endif

  vkDestroyInstance(instance, nullptr);
}