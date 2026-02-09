#version 420

layout(location = 0) flat in vec4 v_Color;
layout(location = 0) out vec4 o_Color;

uniform float u_FillAlpha;

void main()
{
    o_Color = vec4(v_Color.rgb, v_Color.a * u_FillAlpha);
}
