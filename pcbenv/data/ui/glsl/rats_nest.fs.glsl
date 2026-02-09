#version 420

layout(location = 0) in vec4 v_Color;
layout(location = 0) out vec4 o_Color;

uniform vec4 u_colors[2];

void main()
{
    o_Color = v_Color * u_colors[0] + u_colors[1];
}
