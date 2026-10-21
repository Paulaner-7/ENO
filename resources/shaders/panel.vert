// A chapter page's tile (PanelImage): see panel.frag.
// Rebuild panel.vert.qsb with resources/shaders/build.sh after changing it.
#version 440

layout(location = 0) in vec4 qt_VertexPosition;
layout(location = 1) in vec2 qt_VertexTexCoord;
layout(location = 0) out vec2 texCoord;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float bicubic;
    vec2 texSize;
};

void main() {
    texCoord = qt_VertexTexCoord;
    gl_Position = qt_Matrix * qt_VertexPosition;
}
