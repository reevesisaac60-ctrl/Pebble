#version 330 core

// Maximum number of point lights the shader supports per draw. The renderer
// clamps the scene's point-light count to this value; extra point lights are
// ignored with no undefined behaviour.
#define MAX_POINT_LIGHTS 4

in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vTexCoord;

uniform vec3 uViewPosition;

// Material properties.
uniform vec3 uMaterialBaseColor;
uniform float uMaterialMetallic;
uniform float uMaterialRoughness;
uniform int uMaterialHasTexture;
uniform sampler2D uMaterialTexture;

// Directional light (single, always evaluated).
uniform vec3 uDirLightDirection;
uniform vec3 uDirLightColor;
uniform float uDirLightIntensity;
uniform float uAmbientIntensity;

// Point lights (array, clamped to MAX_POINT_LIGHTS).
uniform int uPointLightCount;
uniform vec3 uPointLightPosition[MAX_POINT_LIGHTS];
uniform vec3 uPointLightColor[MAX_POINT_LIGHTS];
uniform float uPointLightIntensity[MAX_POINT_LIGHTS];
uniform float uPointLightRange[MAX_POINT_LIGHTS];

// Fog.
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
uniform int uFogEnabled;

out vec4 FragColor;

void main() {
    vec3 normal = normalize(vNormal);
    vec3 viewDir = normalize(uViewPosition - vWorldPosition);

    // Base color from material, optionally modulated by texture.
    vec3 baseColor = uMaterialBaseColor;
    if (uMaterialHasTexture != 0) {
        vec4 texColor = texture(uMaterialTexture, vTexCoord);
        baseColor = baseColor * texColor.rgb;
    }

    // Base ambient term.
    vec3 lighting = uAmbientIntensity * uDirLightColor;

    // Directional light: diffuse + specular (Blinn-Phong style, modulated by
    // material metallic/roughness for a simple PBR-ish response).
    vec3 dirLightDir = normalize(-uDirLightDirection);
    float diff = max(dot(normal, dirLightDir), 0.0);
    vec3 reflectDir = reflect(-dirLightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    float metallicSpec = mix(0.04f, uMaterialBaseColor.r, uMaterialMetallic);
    lighting += uDirLightIntensity * uDirLightColor * (diff + metallicSpec * spec * (1.0f - uMaterialRoughness));

    // Point lights with distance attenuation.
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        if (i >= uPointLightCount) {
            break;
        }
        vec3 lightDir = normalize(uPointLightPosition[i] - vWorldPosition);
        float distance = length(uPointLightPosition[i] - vWorldPosition);
        float attenuation =
            clamp(1.0 - (distance * distance) /
                        (uPointLightRange[i] * uPointLightRange[i]),
                  0.0, 1.0);
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        lighting += uPointLightIntensity[i] * uPointLightColor[i] *
                    (diff + metallicSpec * spec * (1.0f - uMaterialRoughness)) * attenuation;
    }

    vec3 finalColor = baseColor * lighting;

    // Distance fog: linear interpolation between fog end (no fog) and fog
    // start (full fog), based on camera-to-fragment distance. When the range
    // is degenerate (fogEnd <= fogStart), fog is disabled.
    if (uFogEnabled != 0 && uFogEnd > uFogStart) {
        float fogDistance = length(vWorldPosition - uViewPosition);
        float fogFactor =
            clamp((uFogEnd - fogDistance) / (uFogEnd - uFogStart), 0.0, 1.0);
        finalColor = mix(uFogColor, finalColor, fogFactor);
    }

    FragColor = vec4(finalColor, 1.0);
}
