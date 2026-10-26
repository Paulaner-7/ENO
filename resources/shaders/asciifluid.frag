// ENO - Multidirectional Bold ASCII Fluid Background Shader
// Computes 2-layer domain-warped fluid math directly in the fragment shader,
// indexing into a 20-character monospace glyph atlas with neon green palette,
// ambient dimming, and radial vignette for optimal UI contrast.
// Rebuild asciifluid.frag.qsb with /opt/homebrew/bin/qsb after changing it.
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    vec2 size;
    vec2 cellSize;
    float dim;
    float vignette;
};

layout(binding = 1) uniform sampler2D glyphAtlas;

void main() {
    vec2 p = qt_TexCoord0 * size;
    vec2 cell = floor(p / cellSize);
    vec2 cellOffset = fract(p / cellSize);

    // Isotropic scaling so no single axis dominates
    float nx = cell.x * 0.028;
    float ny = cell.y * 0.028;

    // First layer of multidirectional domain warping
    float qx = sin(nx * 1.1 + time * 0.7) + cos(ny * 1.1 - time * 0.5);
    float qy = cos(nx * 0.9 - time * 0.6) + sin(ny * 1.2 + time * 0.8);

    // Second layer of warping, creating swirling, folding currents in all directions
    float rx = sin(nx + 1.6 * qx + 1.7 + time * 0.4) + cos(ny + 1.6 * qy + 9.2 - time * 0.3);
    float ry = cos(nx + 1.6 * qx + 8.3 - time * 0.5) + sin(ny + 1.6 * qy + 2.8 + time * 0.6);

    // Evaluate final fluid height from the twice-warped coordinates
    float wave = sin(nx + 1.4 * rx) 
               + cos(ny + 1.4 * ry) 
               + sin((rx + ry) * 0.85);

    // Normalize from [-3, 3] to [0, 1]
    float norm = clamp((wave + 3.0) / 6.0, 0.0, 1.0);

    // Exponent 1.6 keeps a wide pool of green visible while preserving black voids
    float intensity = pow(norm, 1.6);

    // Deep background #010301
    vec3 bgColor = vec3(1.0 / 255.0, 3.0 / 255.0, 1.0 / 255.0);

    if (intensity < 0.04) {
        fragColor = vec4(bgColor, 1.0) * qt_Opacity;
        return;
    }

    float charCount = 20.0;
    float charIndex = min(charCount - 1.0, floor(intensity * charCount));

    // Spaces at index 0 and 1
    if (charIndex < 2.0) {
        fragColor = vec4(bgColor, 1.0) * qt_Opacity;
        return;
    }

    // Horizontal safe margin within cell to prevent atlas edge bleed
    float clampedOffsetX = clamp(cellOffset.x, 0.01, 0.99);
    float u = (charIndex + clampedOffsetX) / charCount;
    float v = clamp(cellOffset.y, 0.01, 0.99);
    float glyphAlpha = texture(glyphAtlas, vec2(u, v)).a;

    // High green multiplier for neon accents and bright white-green cores
    float g = min(1.0, pow(intensity, 0.8) * (350.0 / 255.0));
    float r = min(1.0, pow(intensity, 2.5) * (215.0 / 255.0));
    float b = min(1.0, pow(intensity, 2.1) * (165.0 / 255.0));
    vec3 charColor = vec3(r, g, b);

    // Ambient dimming so background doesn't compete with UI text
    charColor = mix(bgColor, charColor, dim);

    // Subtle radial vignette
    vec2 rel = (qt_TexCoord0 - vec2(0.5)) * vec2(size.x / max(size.y, 1.0), 1.0);
    float dist = length(rel);
    float vig = clamp(1.0 - dist * (vignette * 0.5), 0.4, 1.0);
    charColor *= vig;

    vec3 finalColor = mix(bgColor, charColor, glyphAlpha);
    fragColor = vec4(finalColor, 1.0) * qt_Opacity;
}
