#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <memory>

class Window
{
public:
    Window();
    Window(const Window&) = delete;
    Window(Window&& other);
    ~Window();

    Window& operator=(const Window&) = delete;
    Window& operator=(Window&& other);

    VkResult create_vulkan_surface(VkInstance instance, VkSurfaceKHR *pSurface);
    const char **get_required_vulkan_extensions(uint32_t *pEnabledExtensionCount);
    bool is_closed() const;
    void poll_events();
private:
    GLFWwindow *_window;
};
