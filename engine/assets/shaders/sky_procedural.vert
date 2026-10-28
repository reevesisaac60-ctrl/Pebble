#version 330 core

layout (location = 0) in vec3 aPosition;

uniform mat4 uView;
uniform mat4 uProjection;

// View-space direction from the camera to this fragment. Used to compute
// the gradient based on elevation.
out vec3 vViewDirection;

void main() {
    // The sky cube is centered at the camera (translation stripped from
    // the view matrix), so the vertex position is the view direction.
    vViewDirection = aPosition;
    gl_Position = uProjection * uView * vec4(aPosition, 1.0);
    // Force depth to the far plane so the sky is always behind geometry.
    gl_Position.z = gl_Position.w;
}
