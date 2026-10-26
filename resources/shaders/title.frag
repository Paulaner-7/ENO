// The Retro hub's title in a gradient: the text (drawn white into a layer)
// gives the shape, two colours blend across it from left to right. Replaces
// a gradient rectangle masked by MultiEffect, which left a faint box.
// Rebuild title.frag.qsb with resources/shaders/build.sh after changing it.
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 from;
    vec4 to;
};
layout(binding = 1) uniform sampler2D source;

void main() {
    float a = texture(source, qt_TexCoord0).a;
    vec4 c = mix(from, to, qt_TexCoord0.x);
    fragColor = vec4(c.rgb, 1.0) * a * qt_Opacity;
}
