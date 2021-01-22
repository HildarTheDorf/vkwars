#include "Renderer.hpp"
#include "Window.hpp"

#include "imgui.h"

int main()
{
    ImGui::CreateContext();
    ImGui::GetIO().FontGlobalScale *= 2;

    Window window;

    Renderer renderer([&window](uint32_t *pCount){ return window.getVulkanExtensions(pCount); }, [&window](VkInstance instance, VkAllocationCallbacks *allocator, VkSurfaceKHR *pSurface){
        return window.getVulkanSurface(instance, allocator, pSurface);
    });

    while (!window.shouldClose())
    {
        Window::PollEvents();

        ImGui::NewFrame();
        ImGui::ShowDemoWindow();
        ImGui::ShowMetricsWindow();
        ImGui::Render();

        renderer.render();
    }

    ImGui::DestroyContext();
}
