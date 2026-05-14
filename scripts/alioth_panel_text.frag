#version 450

layout(set = 0, binding = 0) uniform sampler2D font_atlas;

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec3 v_color;
layout(location = 0) out vec4 out_color;

void main()
{
    float alpha = texture(font_atlas, v_uv).r;
    out_color = vec4(v_color, alpha);
}
