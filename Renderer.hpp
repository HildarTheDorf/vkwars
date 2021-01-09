#pragma once

#include "RendererBase.hpp"

class Window;

class Renderer : public RendererBase
{
public:
    explicit Renderer(Window& window);

    void render();
private:
    void record_command_buffer(const PerFrameData& perFrame, const PerImageData& perImage);
};
