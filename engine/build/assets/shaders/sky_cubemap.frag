#version 330 core

in vec3 vWorldDirection;

uniform samplerCube uSkybox;

out vec4 FragColor;

void main() {
    vec3 color = texture(uSkybox, vWorldDirection).rgb;
    FragColor = vec4(color, 1.0);
}
