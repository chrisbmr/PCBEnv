#version 420

out gl_PerVertex
{
    vec4 gl_Position;
    float gl_PointSize;
};

layout(std140, binding = 0) uniform u_Camera
{
    mat4 MVP;
};

layout(location = 0) in vec3 a_PositionAndSize;
layout(location = 1) in vec4 a_Color;
layout(location = 0) flat out vec4 v_Color;

uniform float u_PointSize;

void main()
{
    gl_Position = MVP * vec4(a_PositionAndSize.xy, 0.5, 1.0);
    gl_PointSize = u_PointSize * a_PositionAndSize.z;

    v_Color = a_Color;
}
