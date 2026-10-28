#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uViewPosition;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec2 vTexCoord;

void main() {
    // Column-vector convention: model first, then view, then projection.
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    gl_Position = uProjection * uView * worldPos;
    vWorldPosition = worldPos.xyz;
    // World-space normal (uniform scale assumed; normalized in the fragment
    // shader after interpolation).
    vNormal = mat3(uModel) * aNormal;
    vTexCoord = aTexCoord;
}
