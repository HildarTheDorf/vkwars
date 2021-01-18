#version 450

layout(location = 0) in vec4 in_Color;
layout(location = 1) in vec2 in_UV;

layout(set=0, binding=0) uniform sampler2D u_Sampler;

layout(location = 0) out vec4 out_Color;

void main()
{
    out_Color = in_Color * texture(u_Sampler, in_UV.st);
}