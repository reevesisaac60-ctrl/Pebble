#version 330 core

layout (location = 0) in vec3 aPosition;

uniform mat4 uView;
uniform mat4 uProjection;

// World-space direction from the camera to this fragment, used to sample
// the cubemap.
out vec3 vWorldDirection;

void main() {
    // The view matrix has no translation (sky is infinitely far away) and is
    // orthonormal (rotation only), so its inverse is its transpose.
    vec4 worldDir = transpose(uView) * vec4(aPosition, 0.0);
    vWorldDirection = normalize(worldDir.xyz);
    gl_Position = uProjection * uView * vec4(aPosition, 1.0);
    gl_Position.z = gl_Position.w;
}
