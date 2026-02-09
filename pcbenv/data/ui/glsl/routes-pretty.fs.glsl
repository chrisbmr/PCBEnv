#version 420

layout(location = 0) flat in vec4 v_Color;
layout(location = 1) flat in vec2 v_Limit;
layout(location = 2) in float v_S;
layout(location = 3) in float v_CapPos;

layout(location = 0) out vec4 o_Color;

void main()
{
    float s = v_S; // sides: -1 for cw, +1 for ccw perpendicular position
    float r = v_S * v_S + v_CapPos * v_CapPos;
    if (r < v_Limit.x || r > v_Limit.y)
        discard;
    o_Color = v_Color;
}
