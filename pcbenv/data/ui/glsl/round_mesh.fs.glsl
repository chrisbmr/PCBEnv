#version 410

#ifndef ENABLE_ANTIALIAS
#define ENABLE_ANTIALIAS 0
#endif

layout(location = 0) flat in vec3 v_ColorFill;
layout(location = 1) flat in vec3 v_ColorLine;
layout(location = 2) in vec2 v_Coord;
layout(location = 3) flat in vec2 v_CapStart;

layout(location = 0) out vec4 o_Color;

uniform vec2 u_FillLineAlpha;

const float lineRange = 1.0;
const float fadeRange = 0.96;

void main()
{
    float y = 0.0;
    if (v_Coord.y > v_CapStart.x)
        y = v_Coord.y - v_CapStart.x;
    else if (v_Coord.y < -v_CapStart.x)
        y = v_Coord.y + v_CapStart.x;
    float s = 0.0;
    if (y != 0.0) {
        vec2 coord = vec2(v_Coord.x, y * v_CapStart.y);
        s = dot(coord, coord);
    }
#if ENABLE_ANTIALIAS
    if (s > 1.0)
        discard;
    if (s > fadeRange) { // line to black
        float k = 1.0 - (s - fadeRange) / (1.0 - fadeRange);
        o_Color = vec4(v_ColorLine, k);
    } else if (s > lineRange) { // fill to line
        float k = (s - lineRange) / (fadeRange - lineRange);
        o_Color = vec4(v_ColorLine * k, k) + vec4(v_ColorFill, u_FillLineAlpha.x) * (1.0 - k);
    } else {
        o_Color = vec4(v_ColorFill, u_FillLineAlpha.x);
    }
#else
    if (s > 1.0)
        discard;
    if (s > lineRange)
        o_Color = vec4(v_ColorLine, u_FillLineAlpha.y);
    else
        o_Color = vec4(v_ColorFill, u_FillLineAlpha.x);
#endif
}
