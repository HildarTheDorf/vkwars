#pragma once

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

    bool is_closed() const;
    void poll_events();
private:
    GLFWwindow *_window;
};
