
#version 420

layout(location = 0) flat in vec4 v_Color1;
layout(location = 1) flat in vec4 v_Color2;
layout(location = 2) in vec2 v_TexCoord;

layout(location = 0) out vec4 o_Color;

const float RRo = 0.00 * 0.00;
const float RRO = 0.60 * 0.60;
const float RR_ = 0.99 * 0.99;

const float InnerAlpha = 0.7;

void main()
{
    vec4 color;

    float rr = dot(v_TexCoord, v_TexCoord);
    if (rr < RRo)
        color = vec4(v_Color1.rgb, v_Color1.a * (InnerAlpha - 128.0 * (RRo - rr)));
    else if (rr < RRO)
        color = vec4(v_Color1.rgb, v_Color1.a * InnerAlpha);
    else if (rr < RR_)
        color = v_Color2;
    else
        color = vec4(v_Color2.rgb, v_Color2.a * (1.0 - 128.0 * (rr - RR_)));
    
    if (color.a <= 0.03125)
        discard;
    o_Color = color;
}
