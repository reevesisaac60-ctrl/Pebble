#version 330 core

in vec3 vViewDirection;

uniform vec3 uSkyColor;
uniform vec3 uHorizonColor;
uniform float uSkyIntensity;

out vec4 FragColor;

void main() {
    vec3 dir = normalize(vViewDirection);
    // Elevation-based blend: looking up (dir.y > 0) shows sky color,
    // looking down (dir.y < 0) shows horizon color.
    float t = smoothstep(-0.2, 0.8, dir.y);
    vec3 color = mix(uHorizonColor, uSkyColor, t);
    color *= uSkyIntensity;
    FragColor = vec4(color, 1.0);
}
