// A chapter page's tile (PanelImage). Drawn as decoded (one sample, the
// sampler filtering as PanelImage chose it) or, for a page shown larger
// than its picture, enlarged with Catmull-Rom: sharper lines than the
// GPU's bilinear filter. Five bilinear samples stand in for the sixteen
// texels of the filter (the four corner taps weigh least and are left out),
// so phones keep up while a strip scrolls. Low-resolution scans enlarged
// this way can be sharpened as well (Settings → Reading → Sharpen): AMD's
// Contrast Adaptive Sharpening (FidelityFX CAS, MIT), which strengthens
// lines where the contrast is low and leaves strong edges alone, so it adds
// no halos. Four more samples.
// Rebuild panel.frag.qsb with resources/shaders/build.sh after changing it.
#version 440

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float bicubic;
    vec2 texSize;
    float sharpen;
};
layout(binding = 1) uniform sampler2D source;

vec4 catmullRom(vec2 uv) {
    vec2 pos = uv * texSize;
    vec2 center = floor(pos - 0.5) + 0.5;
    vec2 f = pos - center;
    vec2 f2 = f * f;
    vec2 f3 = f2 * f;
    vec2 w0 = -0.5 * f3 + f2 - 0.5 * f;
    vec2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
    vec2 w2 = -1.5 * f3 + 2.0 * f2 + 0.5 * f;
    vec2 w3 = 0.5 * f3 - 0.5 * f2;
    vec2 w12 = w1 + w2;
    vec2 tc0 = (center - 1.0) / texSize;
    vec2 tc12 = (center + w2 / w12) / texSize;
    vec2 tc3 = (center + 2.0) / texSize;
    float a = w12.x * w0.y;
    float b = w0.x * w12.y;
    float c = w12.x * w12.y;
    float d = w3.x * w12.y;
    float e = w12.x * w3.y;
    vec4 color = texture(source, vec2(tc12.x, tc0.y)) * a + texture(source, vec2(tc0.x, tc12.y)) * b
               + texture(source, tc12) * c + texture(source, vec2(tc3.x, tc12.y)) * d
               + texture(source, vec2(tc12.x, tc3.y)) * e;
    color /= a + b + c + d + e;
    // The filter overshoots at hard edges; premultiplied colour stays within
    // its alpha.
    color = clamp(color, 0.0, 1.0);
    return vec4(min(color.rgb, vec3(color.a)), color.a);
}

// CAS on the picture's own pixels around uv, applied to `base` (the
// enlarged colour).
vec4 contrastAdaptiveSharpen(vec2 uv, vec4 base) {
    vec2 px = 1.0 / texSize;
    vec3 n = texture(source, uv + vec2(0.0, -px.y)).rgb;
    vec3 w = texture(source, uv + vec2(-px.x, 0.0)).rgb;
    vec3 e = texture(source, uv + vec2(px.x, 0.0)).rgb;
    vec3 s = texture(source, uv + vec2(0.0, px.y)).rgb;
    vec3 c = base.rgb;
    vec3 mn = min(min(min(n, w), min(c, e)), s);
    vec3 mx = max(max(max(n, w), max(c, e)), s);
    vec3 amp = sqrt(clamp(min(mn, 2.0 - mx) / max(mx, vec3(0.0001)), 0.0, 1.0));
    vec3 weight = amp * (-1.0 / mix(8.0, 5.0, clamp(sharpen, 0.0, 1.0)));
    vec3 rgb = clamp(((n + w + e + s) * weight + c) / (1.0 + 4.0 * weight), 0.0, 1.0);
    return vec4(min(rgb, vec3(base.a)), base.a);
}

void main() {
    vec4 color = bicubic > 0.5 ? catmullRom(texCoord) : texture(source, texCoord);
    if (bicubic > 0.5 && sharpen > 0.0)
        color = contrastAdaptiveSharpen(texCoord, color);
    fragColor = color * qt_Opacity;
}
