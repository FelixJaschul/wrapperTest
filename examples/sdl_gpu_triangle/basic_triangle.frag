#version 450

layout(location = 0) in vec3 v_color;
layout(location = 0) out vec4 out_color;

layout(std140, set = 3, binding = 0) uniform FragmentUniforms {
    vec4 global_tint;
} ubo;

void main()
{
    out_color = vec4(v_color * ubo.global_tint.xyz, 1.0);
}
