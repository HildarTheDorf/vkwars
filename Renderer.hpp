#pragma once

class Window;

class Renderer
{
public:
    explicit Renderer(Window& window);

    void render();
};
