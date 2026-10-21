// Rounded cover art in one pass: crops the image to fill the item (as
// Image.PreserveAspectCrop), zooms it about the centre and cuts the corners
// with an antialiased rounded rectangle. Replaces a layer + mask + MultiEffect
// per cover, so a grid of covers needs no offscreen buffers.
// Rebuild cover.frag.qsb with resources/shaders/build.sh after changing it.
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 size;   // the item, in pixels
    vec2 crop;   // the share of the image shown along x and y (<= 1)
    float radius;
    float zoom;
};
layout(binding = 1) uniform sampler2D source;

void main() {
    vec2 uv = (qt_TexCoord0 - 0.5) * crop / zoom + 0.5;
    vec2 p = qt_TexCoord0 * size;
    vec2 q = abs(p - size * 0.5) - (size * 0.5 - vec2(radius));
    float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
    fragColor = texture(source, uv) * clamp(0.5 - d, 0.0, 1.0) * qt_Opacity;
}
