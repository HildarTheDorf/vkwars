#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <memory>

class Window
{
public:
    Window();
    Window(const Window&) = delete;
    Window(Window&& other) noexcept;
    ~Window();

    Window& operator=(const Window&) = delete;
    Window& operator=(Window&& other) noexcept;

    VkResult create_vulkan_surface(VkInstance instance, VkSurfaceKHR *pSurface) noexcept;
    const char **get_required_vulkan_extensions(uint32_t *pEnabledExtensionCount) const noexcept;
    bool is_closed() const noexcept;
    void poll_events() noexcept;
private:
    GLFWwindow *_window;
};
