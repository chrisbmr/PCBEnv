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
layout(location = 1) in vec3 a_ColorFill;
layout(location = 2) in vec3 a_ColorLine;
layout(location = 3) in vec3 a_Coord; // z value is where cap starts: 0 for circles, 1 for rectangles
layout(location = 0) flat out vec3 v_ColorFill;
layout(location = 1) flat out vec3 v_ColorLine;
layout(location = 2) out vec2 v_Coord;
layout(location = 3) flat out vec2 v_CapStart;

void main()
{
    gl_Position = MVP * vec4(a_Position, 0.5, 1.0);

    v_ColorFill = a_ColorFill;
    v_ColorLine = a_ColorLine;
    v_Coord     = a_Coord.xy;
    v_CapStart  = vec2(a_Coord.z, 1.0 / (1 - a_Coord.z));
}
