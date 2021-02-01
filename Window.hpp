#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Window
{
public:
    Window();
    Window(const Window&) = delete;
    Window(Window&&) noexcept;
    ~Window();

    Window& operator=(const Window&) = delete;
    Window& operator=(Window&&) noexcept;

    const char **getVulkanExtensions(uint32_t *pCount) const noexcept;
    VkResult getVulkanSurface(VkInstance instance, VkAllocationCallbacks *allocator, VkSurfaceKHR *pSurface) noexcept;
    bool shouldClose() const noexcept;

    static void PollEvents();

private:
    GLFWwindow *window;
};
