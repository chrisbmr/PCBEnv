#version 420

layout(location = 0) flat in vec4 v_Color;
layout(location = 1) flat in int v_Z;
layout(location = 2) in float v_S;
layout(location = 3) in float v_CapPos;

layout(location = 0) out vec4 o_Color;

const float InnerStart = 0.6;
const float OuterStart = 0.9;

const vec3 LayerColor[4] =
{
    vec3(1.0, 0.8, 0.0),
    vec3(0.2, 0.4, 0.8),
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0)
};

void main()
{
    float s = v_S; // sides: -1 for cw, +1 for ccw perpendicular position
    float z = v_Z;
#if 0
    vec3 color = v_Color.rgb;
    if (v_Z > 0)
        color = vec3(1.0) - color;
#else
    vec3 color = LayerColor[v_Z];
#endif
    float r = v_S * v_S + v_CapPos * v_CapPos;
    if (r > 1.0)
        discard;
    float a = 1.0;
    if (r < InnerStart)
        a = 0.75;
#if 0
    else
    if (r < OuterStart)
        a = mix(0.75, 1.0, 20.0 * (r - 0.85));
#endif
    o_Color = vec4(color, v_Color.a * a);
}
