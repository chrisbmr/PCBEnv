#version 420

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(std140, binding = 0) uniform u_Camera
{
    mat4 MVP;
};

layout(location = 0) in vec2 a_Position;

void main()
{
    gl_Position = MVP * vec4(a_Position, 0.5, 1.0);
}
