#include "Window.hpp"

#include "imgui/imgui_impl_glfw.h"

#include <mutex>

Window::Window()
{
    static std::once_flag init_flag;
    std::call_once(init_flag, []{
        glfwInit();
        atexit(glfwTerminate);
    });

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    _window = glfwCreateWindow(800, 600, "vkwars", nullptr, nullptr);

    ImGui_ImplGlfw_InitForVulkan(_window, true);
}

Window::Window(Window&& other) noexcept
{
    *this = std::move(other);
}

Window::~Window()
{
    ImGui_ImplGlfw_Shutdown();
    glfwDestroyWindow(_window);
}

Window& Window::operator=(Window&& other) noexcept
{
    _window = other._window;
    other._window = nullptr;
    return *this;
}

VkResult Window::create_vulkan_surface(VkInstance instance, VkSurfaceKHR *pSurface) noexcept
{
    return glfwCreateWindowSurface(instance, _window, nullptr, pSurface);
}

const char **Window::get_required_vulkan_extensions(uint32_t *pEnabledExtensionCount) const noexcept
{
    return glfwGetRequiredInstanceExtensions(pEnabledExtensionCount);
}

bool Window::is_closed() const noexcept
{
    return glfwWindowShouldClose(_window);
}

void Window::poll_events() noexcept
{
    glfwPollEvents();
    ImGui_ImplGlfw_NewFrame();
}
