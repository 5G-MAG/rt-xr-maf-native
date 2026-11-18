#version 450
// A triangle covering the target: the picture coordinates of the frame (its top-left region), flipped vertically on
// demand. Regenerate VulkanShaders.h with compile.sh after a change.

layout(push_constant) uniform Constants
{
    vec2 uvScale; // frame size / picture size
    float flipY;  // 1: the frame is flipped vertically
} constants;

layout(location = 0) out vec2 uv;

void main()
{
    vec2 position = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2); // (0,0) (2,0) (0,2)
    vec2 t = position;
    if (constants.flipY > 0.5)
    {
        t.y = 1.0 - t.y;
    }
    uv = t * constants.uvScale;
    gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
}
