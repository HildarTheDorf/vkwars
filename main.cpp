#include "Renderer.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"

struct GlfwDeleter
{
    void operator()(GLFWwindow *window)
    {
        glfwDestroyWindow(window);
    }
};

int main()
{
    ImGui::CreateContext();
    ImGui::GetIO().FontGlobalScale *= 2;

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    const auto window = std::unique_ptr<GLFWwindow, GlfwDeleter>(glfwCreateWindow(800, 600, "vkwars", nullptr, nullptr));

    ImGui_ImplGlfw_InitForVulkan(window.get(), true);

    Renderer renderer(glfwGetRequiredInstanceExtensions, [&window](VkInstance instance, VkAllocationCallbacks *allocator, VkSurfaceKHR *pSurface){
        return glfwCreateWindowSurface(instance, window.get(), allocator, pSurface);
    });

    while (!glfwWindowShouldClose(window.get()))
    {
        glfwPollEvents();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();
        ImGui::ShowDemoWindow();
        ImGui::ShowMetricsWindow();
        ImGui::Render();

        renderer.render();
    }

    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
