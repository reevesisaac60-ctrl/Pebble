#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aUv;
uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;
out vec2 vUv;
out vec3 vWorldPosition;
void main() {
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    vWorldPosition = worldPosition.xyz;
    vUv = aUv;
    gl_Position = uProjection * uView * worldPosition;
}
