#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec4 fragColor;

// VulkanTutorial ch. 27: separate proj + view (matches glm upload, avoids pre-multiply packing bugs).
layout(push_constant) uniform Push {
    layout(offset = 0) mat4 proj;
    layout(offset = 64) mat4 view;
} pc;

void main() {
    gl_Position = pc.proj * pc.view * vec4(inPosition, 1.0);
    fragNormal = inNormal;
    fragColor = inColor;
}
