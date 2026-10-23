// macOS 27 Native Liquid Glass (NSGlassEffectView / CASDFLayer + glassBackground)
// Implements the optical pipeline of macOS 27's CABackdropLayer glassBackground filter:
// 1. Rounded-rectangle / capsule / circle Signed Distance Field (CASDFLayer)
// 2. Curved meniscus lens refraction along the bezel (inputInnerRefractionAmount = -30..-39,
//    inputInnerRefractionHeight = 15..20) with interactive flex (effectIsInteractive)
// 3. Chromatic dispersion along the refracted surface normal
// 4. Variable blur opacity (clearer refractive rim, frosted center) + face color matrix
//    (inputFaceColorMatrixSaturation, inputFaceColorMatrixMaxLuma, theme tint)
// 5. Directional key-fill specular rim highlight (inputKeyFillHighlightAngle, Spread, Amount)
// Rebuild liquidglass.frag.qsb with resources/shaders/build.sh after changing it.
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 size;
    vec2 pointer;
    vec4 tint;
    float radius;
    float refractionAmount;
    float refractionHeight;
    float saturation;
    float highlightAngle;
    float highlightSpread;
    float highlightAmount;
    float darkMode;
    float interactive;
    float clearStyle;
    float hasBackdrop;
};

layout(binding = 1) uniform sampler2D sharpSource;
layout(binding = 2) uniform sampler2D blurredSource;

void main() {
    vec2 p = qt_TexCoord0 * size;
    vec2 halfSize = max(size * 0.5, vec2(1.0));
    float r = clamp(radius, 0.0, min(halfSize.x, halfSize.y));

    // Signed Distance Field for rounded rectangle / capsule / circle (CASDFLayer)
    vec2 rel = p - halfSize;
    vec2 box = halfSize - vec2(r);
    vec2 dVec = abs(rel) - box;
    vec2 maxD = max(dVec, vec2(0.0));
    float outsideLen = length(maxD);
    float insideMax = min(max(dVec.x, dVec.y), 0.0);
    float sdf = outsideLen + insideMax - r;

    float edgeAlpha = clamp(0.5 - sdf, 0.0, 1.0);
    if (edgeAlpha <= 0.0001) {
        fragColor = vec4(0.0);
        return;
    }

    // Outward 2D surface normal of the SDF boundary
    vec2 grad;
    if (outsideLen > 0.0001) {
        grad = sign(rel) * (maxD / outsideLen);
    } else {
        grad = (dVec.x > dVec.y) ? vec2(sign(rel.x), 0.0) : vec2(0.0, sign(rel.y));
    }

    // Curved meniscus lens profile (inputInnerRefractionHeight = 15..20px)
    float maxBezel = min(refractionHeight, min(halfSize.x, halfSize.y) * 0.78);
    float rimDist = clamp(-sdf, 0.0, max(maxBezel, 1.0));
    float rimFactor = 1.0 - (rimDist / max(maxBezel, 1.0));
    float meniscus = rimFactor * rimFactor * (1.85 - 0.85 * rimFactor);

    // Interactive pointer liquid tension (NSGlassEffectView.effectIsInteractive on macOS 27)
    vec2 ptrPx = pointer * size;
    vec2 toPtr = p - ptrPx;
    float ptrDist = length(toPtr);
    float ptrSpan = max(size.x, size.y) * 0.65;
    float ptrInfluence = (interactive > 0.001) ? clamp(1.0 - ptrDist / max(ptrSpan, 1.0), 0.0, 1.0) : 0.0;
    vec2 ptrDir = (ptrDist > 0.001) ? (toPtr / ptrDist) : vec2(0.0);

    vec3 glassRgb;
    if (hasBackdrop > 0.5) {
        // Lens refraction displacement along the curved bezel normal
        float refractPx = (refractionAmount * 0.45) * meniscus * (1.0 + 0.2 * interactive);
        vec2 flexOffset = -ptrDir * (ptrInfluence * ptrInfluence * 3.2 * interactive);
        vec2 uvOffset = (grad * refractPx + flexOffset) / max(size, vec2(1.0));
        vec2 uvBase = clamp(qt_TexCoord0 + uvOffset, vec2(0.002), vec2(0.998));

        // Chromatic dispersion across the curved glass edge
        vec2 disp = uvOffset * (0.14 + 0.08 * clearStyle);
        vec2 uvR = clamp(uvBase - disp, vec2(0.002), vec2(0.998));
        vec2 uvG = uvBase;
        vec2 uvB = clamp(uvBase + disp, vec2(0.002), vec2(0.998));

        vec3 blurredRgb = vec3(
            texture(blurredSource, uvR).r,
            texture(blurredSource, uvG).g,
            texture(blurredSource, uvB).b
        );
        vec3 sharpRgb = vec3(
            texture(sharpSource, uvR).r,
            texture(sharpSource, uvG).g,
            texture(sharpSource, uvB).b
        );

        // macOS 27 glassBackground: inputBlurOpacity0 = 0.4 at the outer bezel, 1.0 in the core
        float clearRimMix = smoothstep(0.45, 0.98, rimFactor) * (clearStyle > 0.5 ? 0.58 : 0.36);
        glassRgb = mix(blurredRgb, sharpRgb, clearRimMix);

        // Face color matrix: saturation boost & HDR-like luma holding tone
        float luma = dot(glassRgb, vec3(0.2126, 0.7152, 0.0722));
        glassRgb = mix(vec3(luma), glassRgb, saturation);

        if (darkMode > 0.5) {
            // DarkAqua: inputFaceColorMatrixWhite = 1.125, Black = 0.08, MaxLuma = 0.6
            glassRgb = clamp(glassRgb * 1.06 + vec3(0.035), vec3(0.03), vec3(clearStyle > 0.5 ? 0.82 : 0.66));
        } else {
            // Aqua: inputFaceColorMatrixWhite = 1.0, Black = 0.11, MaxLuma = 0.94
            glassRgb = clamp(glassRgb * 0.98 + vec3(0.08), vec3(0.14), vec3(0.96));
        }

        // Theme tint wash (slightly lighter right at the refractive meniscus rim)
        float washAlpha = tint.a * (1.0 - 0.28 * smoothstep(0.6, 1.0, rimFactor));
        glassRgb = mix(glassRgb, tint.rgb, clamp(washAlpha, 0.0, 0.92));
    } else {
        // Fallback when no live backdrop texture is attached: rich translucent glass body
        glassRgb = tint.rgb;
    }

    // Directional Key-Fill Specular Rim Highlight (inputKeyFillHighlightAngle, Spread, Amount)
    float dynAngle = highlightAngle + (interactive > 0.001 ? (pointer.x - 0.5) * 0.45 : 0.0);
    vec2 keyDir = normalize(vec2(-cos(dynAngle * 0.65), -sin(dynAngle)));
    vec2 fillDir = -keyDir;

    float keyDot = max(0.0, dot(grad, keyDir));
    float fillDot = max(0.0, dot(grad, fillDir));
    float spreadExp = clamp(3.4 / max(highlightSpread, 0.5), 1.2, 3.2);
    float directionalLobe = pow(keyDot, spreadExp) + pow(fillDot, spreadExp + 0.6) * 0.48 + 0.18;

    // 1. Crisp outer specular bezel rim (1.35px)
    float outerRimMask = smoothstep(1.55, 0.0, abs(sdf + 0.65));
    float specStrength = highlightAmount * (darkMode > 0.5 ? 0.72 : 1.15) * (1.0 + 0.22 * interactive);
    float outerSpec = outerRimMask * directionalLobe * specStrength;

    // 2. Inner meniscus caustic reflection along the top-lit curve
    float innerCausticMask = smoothstep(5.0, 0.6, -sdf) * smoothstep(-0.2, 1.2, -sdf);
    float innerCaustic = innerCausticMask * pow(keyDot, 1.4) * (darkMode > 0.5 ? 0.20 : 0.48) * (1.0 + 0.25 * interactive);

    // 3. Top-to-bottom subtle glass body sheen & bottom pooling shade
    float topSheen = pow(clamp(1.0 - qt_TexCoord0.y * 1.65, 0.0, 1.0), 1.6) * (darkMode > 0.5 ? 0.09 : 0.28);
    float bottomPool = pow(clamp(qt_TexCoord0.y * 1.4 - 0.4, 0.0, 1.0), 1.8) * (darkMode > 0.5 ? 0.14 : 0.07);

    // 4. Interactive pointer specular glow (macOS 27 effectIsInteractive)
    float ptrGlow = ptrInfluence * ptrInfluence * (darkMode > 0.5 ? 0.14 : 0.26) * interactive;

    vec3 finalRgb = glassRgb * (1.0 - bottomPool);
    float totalLight = clamp(outerSpec + innerCaustic + topSheen + ptrGlow, 0.0, 0.95);
    finalRgb = mix(finalRgb, vec3(1.0), totalLight);

    // Subtle chromatic tint on the brightest specular rim
    if (outerSpec > 0.05) {
        finalRgb += vec3(0.03, 0.01, -0.02) * outerSpec * (1.0 - keyDot);
    }

    float finalAlpha = (hasBackdrop > 0.5 ? 1.0 : clamp(tint.a + totalLight * 0.6, 0.0, 1.0)) * edgeAlpha * qt_Opacity;
    fragColor = vec4(finalRgb * finalAlpha, finalAlpha);
}
