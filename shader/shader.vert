#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTex;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTex;
layout(location = 2) flat out int fragTexID;

layout(set = 1, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
} ubo;

layout(push_constant) uniform UIPushConstant {
    int textureID;
    mat4 proj;
} pc;

void main()
{
    gl_Position = pc.proj * ubo.view * ubo.model * vec4(inPos, 0.0, 1.0);
    fragColor = inColor;
    fragTex = inTex;
    fragTexID = pc.textureID;
}