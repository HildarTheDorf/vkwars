#include "Renderer.hpp"
#include "Window.hpp"

#include "imgui.h"

void ShowBackendCheckerWindow(bool* p_open = nullptr)
{
    if (!ImGui::Begin("Dear ImGui Backend Checker", p_open))
    {
        ImGui::End();
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Dear ImGui %s Backend Checker", ImGui::GetVersion());
    ImGui::Text("io.BackendPlatformName: %s", io.BackendPlatformName ? io.BackendPlatformName : "NULL");
    ImGui::Text("io.BackendRendererName: %s", io.BackendRendererName ? io.BackendRendererName : "NULL");
    ImGui::Separator();
    
    if (ImGui::TreeNode("0001: Renderer: Large Mesh Support"))
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        {
            static int vtx_count = 60000;
            ImGui::SliderInt("VtxCount##1", &vtx_count, 0, 100000);
            ImVec2 p = ImGui::GetCursorScreenPos();
            for (int n = 0; n < vtx_count / 4; n++)
            {
                float off_x = (float)(n % 100) * 3.0f;
                float off_y = (float)(n % 100) * 1.0f;
                ImU32 col = IM_COL32(((n * 17) & 255), ((n * 59) & 255), ((n * 83) & 255), 255);
                draw_list->AddRectFilled(ImVec2(p.x + off_x, p.y + off_y), ImVec2(p.x + off_x + 50, p.y + off_y + 50), col);
            }
            ImGui::Dummy(ImVec2(300 + 50, 100 + 50));
            ImGui::Text("VtxBuffer.Size = %d", draw_list->VtxBuffer.Size);
        }
        {
            static int vtx_count = 60000;
            ImGui::SliderInt("VtxCount##2", &vtx_count, 0, 100000);
            ImVec2 p = ImGui::GetCursorScreenPos();
            for (int n = 0; n < vtx_count / (10*4); n++)
            {
                float off_x = (float)(n % 100) * 3.0f;
                float off_y = (float)(n % 100) * 1.0f;
                ImU32 col = IM_COL32(((n * 17) & 255), ((n * 59) & 255), ((n * 83) & 255), 255);
                draw_list->AddText(ImVec2(p.x + off_x, p.y + off_y), col, "ABCDEFGHIJ");
            }
            ImGui::Dummy(ImVec2(300 + 50, 100 + 20));
            ImGui::Text("VtxBuffer.Size = %d", draw_list->VtxBuffer.Size);
        }
        ImGui::TreePop();
    }

    ImGui::End();
}

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
        ShowBackendCheckerWindow();
        ImGui::Render();

        renderer.render();
    }

    ImGui::DestroyContext();
}
