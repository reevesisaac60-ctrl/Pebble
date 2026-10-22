#version 330 core
in vec2 vUv;
in vec3 vWorldPosition;
uniform float uTime;
out vec4 FragColor;

vec3 hueToRgb(float hue) {
    vec3 channel = abs(fract(vec3(hue) + vec3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
    return clamp(channel - 1.0, 0.0, 1.0);
}

void main() {
    float hue = fract((vUv.x + vUv.y) * 0.5 + uTime * 0.12);
    vec3 rainbow = hueToRgb(hue);

    vec3 normal = normalize(cross(dFdx(vWorldPosition), dFdy(vWorldPosition)));
    float diffuse = max(dot(normal, normalize(vec3(-0.4, 0.8, 0.6))), 0.0);
    float lighting = 0.62 + 0.38 * diffuse;

    vec2 toEdge = min(vUv, 1.0 - vUv);
    float seam = smoothstep(0.0, 0.035, min(toEdge.x, toEdge.y));

    FragColor = vec4(rainbow * lighting * mix(0.55, 1.0, seam), 1.0);
}
