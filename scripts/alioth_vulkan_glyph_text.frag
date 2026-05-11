#version 450

layout(set = 0, binding = 0) uniform sampler2D u_glyph_atlas;

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec3 v_color;
layout(location = 0) out vec4 out_color;

void main()
{
    vec4 texel = texture(u_glyph_atlas, v_uv);

    if (texel.a < 0.5)
        discard;
    out_color = vec4(v_color, 1.0);
}
