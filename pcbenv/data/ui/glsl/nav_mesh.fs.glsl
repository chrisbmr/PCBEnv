#version 420

layout(location = 0) flat in vec4 v_Color;
layout(location = 0) out vec4 o_Color;

uniform vec4 u_color;

void main()
{
    o_Color = u_color * v_Color;
}
