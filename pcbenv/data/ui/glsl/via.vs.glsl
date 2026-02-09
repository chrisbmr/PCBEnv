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
layout(location = 1) in vec4 a_Color1;
layout(location = 2) in vec4 a_Color2;
layout(location = 0) flat out vec4 v_Color1;
layout(location = 1) flat out vec4 v_Color2;
layout(location = 2) out vec2 v_TexCoord;

void main()
{
    gl_Position = MVP * vec4(a_Position.xy, 0.5, 1.0);
    v_Color1 = a_Color1;
    v_Color2 = a_Color2;
    v_TexCoord = a_Position.zw;
}
