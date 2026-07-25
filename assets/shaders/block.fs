#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform int   u_mode;         // 0=none, 1=glossy, 2=metallic, 3=metal2 (diamond plate), 4=neon
uniform vec2  u_rectPos;
uniform vec2  u_rectSize;
uniform float u_screenHeight;
uniform float u_amount;       // generic "more" slider -- meaning depends on u_mode, default 1.0

out vec4 finalColor;

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main()
{
    vec4 texel = texture(texture0, fragTexCoord);
    vec4 baseColor = texel * colDiffuse * fragColor;

    if (u_mode == 0 || u_rectSize.x <= 0.0 || u_rectSize.y <= 0.0) {
        finalColor = baseColor;
        return;
    }

    vec2 fragXY = vec2(gl_FragCoord.x, u_screenHeight - gl_FragCoord.y);
    vec2 localXY = fragXY - u_rectPos;
    vec2 uv = clamp(localXY / u_rectSize, 0.0, 1.0); // fine for WIDTH-based features (fixed per key)

    // Distance from the block's own TOP edge, in real pixels. The top is the
    // edge that keeps rising while a note is held, so anchoring here makes
    // the pattern travel with the top -- new pattern reveals further down
    // toward the bottom as the block grows, instead of stretching or
    // detaching from the block.
    float localX = localXY.x;
    float localYFromTop = localXY.y;

    float amount = clamp(u_amount, 0.0, 3.0);

    vec3 result = baseColor.rgb;

    if (u_mode == 1) {
        // --- Glossy: "more" scales the highlight pool, the top rim, and the
        // vertical volume shade together -- i.e. the shine itself gets
        // brighter/more defined, not just brighter overall.
        vec3 hsv = rgb2hsv(baseColor.rgb);

        float shadeAmt = mix(0.05, -0.12, smoothstep(0.0, 1.0, uv.y)) * amount;
        hsv.z = clamp(hsv.z + shadeAmt, 0.0, 1.0);

        vec2 highlightCenter = vec2(0.32, 0.24);
        float dist = distance(uv * vec2(1.0, 1.4), highlightCenter * vec2(1.0, 1.4));
        float highlight = smoothstep(0.75, 0.05, dist) * amount;
        hsv.y = clamp(hsv.y - highlight * 0.55, 0.0, 1.0);
        hsv.z = clamp(hsv.z + highlight * 0.25, 0.0, 1.0);

        float topRim = smoothstep(0.10, 0.0, uv.y) * amount;
        hsv.y = clamp(hsv.y - topRim * 0.30, 0.0, 1.0);
        hsv.z = clamp(hsv.z + topRim * 0.15, 0.0, 1.0);

        result = hsv2rgb(hsv);
    }
    else if (u_mode == 2) {
        // --- Metallic: "more" scales crossShade's DEVIATION from flat (not
        // the raw value, so amount=0 means perfectly flat/no shading) and
        // grain's amplitude, together and proportionally, as requested.
        float t = abs(uv.x - 0.5) * 2.0;
        float crossShadeRaw = mix(0.72, 1.18, smoothstep(0.0, 1.0, t));
        float crossShade = 1.0 + (crossShadeRaw - 1.0) * amount;
        vec3 volumeShaded = baseColor.rgb * crossShade;

        float grain = 0.0;
        grain += sin(localYFromTop * 0.35) * 0.025;
        grain += sin(localYFromTop * 0.15 + 1.7) * 0.03;
        grain += sin(localYFromTop * 0.06 + 0.6) * 0.025;
        grain *= amount;

        result = clamp(volumeShaded + vec3(grain), 0.0, 1.0);
    }
    else if (u_mode == 3) {
        // --- Metal2: "more" shrinks cellSize (tighter grid). Bar size is
        // derived as a fixed proportion of cellSize (not of amount directly),
        // so diamonds keep the same relative size/spacing to their cell at
        // every zoom level -- nothing gets disproportionately cramped or
        // sparse as amount changes.
        float cellSize = 20.0 / clamp(amount, 0.3, 3.0);

        float rowIndex = floor(localYFromTop / cellSize);
        float rowParity = mod(rowIndex, 2.0);
        float cellLocalY = mod(localYFromTop, cellSize) - cellSize * 0.5;

        // Offset alternating rows by half a cell so bar tips connect
        // across rows (herringbone) instead of only meeting at a corner.
        float xShift = (rowParity < 0.5) ? 0.0 : cellSize * 0.5;
        float cellLocalX = mod(localX + xShift, cellSize) - cellSize * 0.5;

        vec2 cellLocal = vec2(cellLocalX, cellLocalY);

        float angle = (rowParity < 0.5) ? radians(45.0) : radians(-45.0);
        float ca = cos(angle);
        float sa = sin(angle);
        vec2 rotated = vec2(cellLocal.x * ca - cellLocal.y * sa,
                            cellLocal.x * sa + cellLocal.y * ca);

        // Fixed proportions of cellSize (0.3 / 0.16 match the original
        // amount=1 baseline: 6/20 and 3.2/20).
        float barHalfLength = cellSize * 0.30;
        float barRadius = cellSize * 0.16;

        float alongClamped = clamp(rotated.x, -barHalfLength, barHalfLength);
        float distToBar = length(vec2(rotated.x - alongClamped, rotated.y));

        float barMask = 1.0 - smoothstep(barRadius * 0.7, barRadius, distToBar);
        float ridge = (1.0 - smoothstep(0.0, barRadius * 0.5, distToBar)) * 0.7;

        vec3 recessed = baseColor.rgb * 0.62;
        vec3 raised = baseColor.rgb * 1.05 + vec3(ridge * 0.5);

        result = mix(recessed, raised, barMask);
    }
    else if (u_mode == 4) {
        // --- Neon: the glow itself (spread/boost) is controlled in
        // blocks.cpp via blocks::setShaderAmount, since that lives in the
        // separate blur render-texture pass, not this fragment shader.
        // This is just the sharp, slightly overexposed core.
        result = baseColor.rgb * 1.25;
    }

    finalColor = vec4(result, baseColor.a);
}