#pragma once

#include "RendererBase.hpp"

class Window;

class Renderer : public RendererBase
{
public:
    explicit Renderer(Window& window);
    Renderer(const Renderer&) = delete;
    Renderer(Renderer&&) noexcept;
    ~Renderer();

    Renderer& operator=(const Renderer&) = delete;
    Renderer& operator=(Renderer&&) noexcept;

    void render();
private:
    void build_swapchain();
    void rebuild_swapchain();
    void record_command_buffer(const PerFrameData& perFrame, const PerImageData& perImage);
};
