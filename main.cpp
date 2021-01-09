#include "Renderer.hpp"
#include "Window.hpp"

int main()
{
    Window window;
    Renderer renderer(window);

    do
    {
        window.poll_events();
        renderer.render();
    } while (!window.is_closed());
}