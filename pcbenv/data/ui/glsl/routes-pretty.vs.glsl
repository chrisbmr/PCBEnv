#version 420

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(std140, binding = 0) uniform u_Camera
{
    mat4 MVP;
};

layout(location = 0) in vec4 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in float a_CapPos;

layout(location = 0) flat out vec4 v_Color;
layout(location = 1) flat out vec2 v_Limit;
layout(location = 2) out float v_S;
layout(location = 3) out float v_CapPos;

const float InnerStart = 0.6;
const float OuterStart = 0.9;

uniform vec4 u_LayerColor[16];

const vec2 Limit[3] =
{
    vec2(0.0, 0.6),
    vec2(0.6, 1.0),
    vec2(0.0, 1.0)
};
const float Alpha[3] =
{
    0.75,
    1.0,
    1.0
};

void main()
{
    gl_Position = MVP * vec4(a_Position.xy, 0.5, 1.0);

    v_Color = u_LayerColor[int(a_Position.z)];
    v_S = a_Position.w;
    v_CapPos = a_CapPos;
    v_Color.a *= Alpha[gl_InstanceID] * a_Color.a;
    v_Limit = Limit[gl_InstanceID];
}
