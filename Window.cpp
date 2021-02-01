#include "Window.hpp"

#include "imgui_impl_glfw.h"

#include <mutex>

Window::Window()
{
    static std::once_flag s_init;
    std::call_once(s_init, []{
        glfwInit();
        atexit(glfwTerminate);
    });

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_MAXIMIZED, true);
    window = glfwCreateWindow(800, 600, "vkwars", nullptr, nullptr);

    ImGui_ImplGlfw_InitForVulkan(window, true);
}

Window::~Window()
{
    ImGui_ImplGlfw_Shutdown();
    glfwDestroyWindow(window);
}

const char **Window::getVulkanExtensions(uint32_t *pCount) const noexcept
{
    return glfwGetRequiredInstanceExtensions(pCount);
}

VkResult Window::getVulkanSurface(VkInstance instance, VkAllocationCallbacks *allocator, VkSurfaceKHR *pSurface) noexcept
{
    return glfwCreateWindowSurface(instance, window, allocator, pSurface);
}

bool Window::shouldClose() const noexcept
{
    return glfwWindowShouldClose(window);
}

void Window::PollEvents()
{
    glfwPollEvents();
    ImGui_ImplGlfw_NewFrame();
}