#version 450

layout(location = 0) out vec3 v_color;

layout(std140, set = 1, binding = 0) uniform VertexUniforms {
    vec4 positions[3];
    vec4 colors[3];
} ubo;

void main()
{
    gl_Position = vec4(ubo.positions[gl_VertexIndex].xy, 0.0, 1.0);
    v_color = ubo.colors[gl_VertexIndex].xyz;
}
