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
layout(location = 1) flat out int v_Z;
layout(location = 2) out float v_S;
layout(location = 3) out float v_CapPos;

void main()
{
    vec2 pos = a_Position.xy;

    gl_Position = MVP * vec4(pos, 0.5, 1.0);

    v_Z = int(a_Position.z);
    v_S = a_Position.w;

    v_CapPos = a_CapPos;

    v_Color = a_Color;
}
