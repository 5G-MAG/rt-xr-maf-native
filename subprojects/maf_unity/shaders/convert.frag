#version 450
// Samples the picture: the immutable sampler of the binding converts YCbCr pictures to RGB. Regenerate VulkanShaders.h
// with compile.sh after a change.

layout(set = 0, binding = 0) uniform sampler2D picture;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;

void main()
{
    color = texture(picture, uv);
}
