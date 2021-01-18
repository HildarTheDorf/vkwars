#version 450

layout(location = 0) in vec2 in_Position;
layout(location = 1) in vec2 in_UV;
layout(location = 2) in vec4 in_Color;

layout(push_constant) uniform PushConstants { vec2 scale; vec2 translate; } pc;

layout(location = 0) out vec4 out_Color;
layout(location = 1) out vec2 out_UV;

void main()
{
    out_Color = in_Color;
    out_UV = in_UV;
    gl_Position = vec4(in_Position * pc.scale + pc.translate, 0.0, 1.0);
}