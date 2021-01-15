#include "Renderer.hpp"
#include "Window.hpp"

#include "imgui.h"

static void render_ui()
{
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();
    ImGui::ShowMetricsWindow();

    ImGui::Begin("Test");
    ImGui::LabelText("Example", "%d", 255);
    ImGui::End();

    ImGui::Render();
}

int main()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    Window window;
    Renderer renderer(window);

    do
    {
        window.poll_events();
        render_ui();
        renderer.render();
    } while (!window.is_closed());

    ImGui::DestroyContext();
}
