#version 330 core
in vec2 vUv;
in vec3 vWorldPosition;
out vec4 FragColor;

const vec2 CELL = vec2(10.0, 16.0);

const int GLYPHS[35] = int[](
    0, 0, 0, 0, 0, 12, 12,
    0, 0, 8, 21, 2, 0, 0,
    0, 4, 4, 31, 4, 4, 0,
    10, 10, 31, 10, 31, 10, 10,
    4, 15, 20, 14, 5, 30, 4
);

bool glyphPixel(int glyph, vec2 local) {
    vec2 g = (local - vec2(0.10, 0.08)) / vec2(0.80, 0.84);
    if (g.x < 0.0 || g.x >= 1.0 || g.y < 0.0 || g.y >= 1.0) {
        return false;
    }
    int column = int(g.x * 5.0);
    int row = 6 - int(g.y * 7.0);
    return ((GLYPHS[glyph * 7 + row] >> (4 - column)) & 1) == 1;
}

void main() {
    if (!gl_FrontFacing) {
        discard;
    }

    vec2 pixel = gl_FragCoord.xy;
    vec2 cell = floor(pixel / CELL);
    vec2 local = fract(pixel / CELL);

    vec2 toCentre = (cell + 0.5) * CELL - pixel;
    vec3 dPosX = dFdx(vWorldPosition);
    vec3 dPosY = dFdy(vWorldPosition);
    vec3 cellPosition = vWorldPosition + dPosX * toCentre.x + dPosY * toCentre.y;
    vec2 cellUv = vUv + dFdx(vUv) * toCentre.x + dFdy(vUv) * toCentre.y;

    vec3 normal = normalize(cross(dPosX, dPosY));
    float diffuse = max(dot(normal, normalize(vec3(-0.45, 0.75, 0.50))), 0.0);
    float sweep = 0.5 + 0.5 * dot(cellPosition, normalize(vec3(-0.6, 0.5, 0.3))) * 0.5;
    vec2 toEdge = min(cellUv, 1.0 - cellUv);
    float edge = 1.0 - smoothstep(0.0, 0.10, min(toEdge.x, toEdge.y));

    float light = 0.16 + 0.88 * diffuse + 0.12 * sweep + 0.10 * edge;
    int glyph = int(clamp(floor(light * 5.0), 0.0, 4.0));

    if (!glyphPixel(glyph, local)) {
        discard;
    }

    FragColor = vec4(0.96, 0.96, 0.94, 1.0);
}
