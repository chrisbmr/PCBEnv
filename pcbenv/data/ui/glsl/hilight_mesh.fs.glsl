#version 420

layout(location = 0) out vec4 o_Color;

in vec4 gl_FragCoord;

void main()
{
    int x = int(gl_FragCoord.x);
    int y = int(gl_FragCoord.y);

    if ((y & 3) != (x & 3))
       o_Color = vec4(0.5, 0.5, 0.5, 1.0);
    else
       o_Color = vec4(0.3, 1.0, 0.3, 1.0);
}
