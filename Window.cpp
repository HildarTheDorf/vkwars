#include "Window.hpp"

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
}

Window::Window(Window&& other)
{
    *this = std::move(other);
}

Window::~Window()
{
    glfwDestroyWindow(_window);
}

Window& Window::operator=(Window&& other)
{
    _window = other._window;
    other._window = nullptr;
    return *this;
}

bool Window::is_closed() const
{
    return glfwWindowShouldClose(_window);
}

void Window::poll_events()
{
    glfwPollEvents();
}