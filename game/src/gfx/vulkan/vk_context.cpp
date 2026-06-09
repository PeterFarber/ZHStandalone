#include "zh/gfx/vulkan/vk_context.hpp"
#include "zh/gfx/vulkan/vk_swapchain.hpp"

#include <GLFW/glfw3.h>

#include <set>
#include <string_view>
#include <vector>

namespace zh::gfx::vk {

namespace {

char const *kDeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

[[nodiscard]] std::vector<char const *> required_instance_extensions() {
    std::uint32_t count = 0;
    char const **const names = glfwGetRequiredInstanceExtensions(&count);
    if (names == nullptr || count == 0) {
        return {};
    }
    return std::vector<char const *>(names, names + count);
}

[[nodiscard]] QueueFamilies find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilies result{};
    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (std::uint32_t i = 0; i < count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            result.graphics = i;
        }
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present);
        if (present == VK_TRUE) {
            result.present = i;
        }
        if (result.complete()) {
            break;
        }
    }
    return result;
}

[[nodiscard]] bool check_device_extensions(VkPhysicalDevice device) {
    std::uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

    std::set<std::string_view> required(kDeviceExtensions,
                                        kDeviceExtensions + std::size(kDeviceExtensions));
    for (auto const &ext : available) {
        required.erase(ext.extensionName);
    }
    return required.empty();
}

[[nodiscard]] bool is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
    auto const families = find_queue_families(device, surface);
    if (!families.complete() || !check_device_extensions(device)) {
        return false;
    }
    auto const support = query_swapchain_support(device, surface);
    return !support.formats.empty() && !support.present_modes.empty();
}

}  // namespace

VkContext::~VkContext() {
    shutdown();
}

bool VkContext::init(GLFWwindow *window) {
    window_ = window;
    if (!create_instance() || !setup_debug_messenger() || !create_surface(window) ||
        !pick_physical_device() || !create_logical_device()) {
        shutdown();
        return false;
    }
    return true;
}

void VkContext::shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    if (debug_messenger_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        if (func != nullptr) {
            func(instance_, debug_messenger_, nullptr);
        }
        debug_messenger_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
    queue_families_ = {};
    physical_device_ = VK_NULL_HANDLE;
}

bool VkContext::create_instance() {
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "ZH Standalone";
    app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app.pEngineName = "zh_gfx";
    app.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app.apiVersion = VK_API_VERSION_1_2;

    auto extensions = required_instance_extensions();
    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &app;
    info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    return vkCreateInstance(&info, nullptr, &instance_) == VK_SUCCESS;
}

bool VkContext::setup_debug_messenger() {
    return true;
}

bool VkContext::create_surface(GLFWwindow *window) {
    return glfwCreateWindowSurface(instance_, window, nullptr, &surface_) == VK_SUCCESS;
}

bool VkContext::pick_physical_device() {
    std::uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) {
        return false;
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());
    for (auto device : devices) {
        if (is_device_suitable(device, surface_)) {
            physical_device_ = device;
            queue_families_ = find_queue_families(device, surface_);
            return true;
        }
    }
    return false;
}

bool VkContext::create_logical_device() {
    std::set<std::uint32_t> unique = {queue_families_.graphics.value(),
                                      queue_families_.present.value()};
    float priority = 1.f;
    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    for (auto family : unique) {
        VkDeviceQueueCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        info.queueFamilyIndex = family;
        info.queueCount = 1;
        info.pQueuePriorities = &priority;
        queue_infos.push_back(info);
    }

    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = static_cast<std::uint32_t>(queue_infos.size());
    info.pQueueCreateInfos = queue_infos.data();
    info.pEnabledFeatures = &features;
    info.enabledExtensionCount = static_cast<std::uint32_t>(std::size(kDeviceExtensions));
    info.ppEnabledExtensionNames = kDeviceExtensions;

    if (vkCreateDevice(physical_device_, &info, nullptr, &device_) != VK_SUCCESS) {
        return false;
    }
    vkGetDeviceQueue(device_, queue_families_.graphics.value(), 0, &graphics_queue_);
    vkGetDeviceQueue(device_, queue_families_.present.value(), 0, &present_queue_);
    return true;
}

}  // namespace zh::gfx::vk
