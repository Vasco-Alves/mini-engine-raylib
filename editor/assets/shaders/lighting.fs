#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

// Output pixel color
out vec4 finalColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 viewPos;

// --- MATERIAL UNIFORMS ---
uniform int useMaterial;
uniform vec4 matAlbedo;
uniform float matRoughness;
uniform float matMetallic;
uniform float matEmission;

// --- MULTIPLE LIGHTS SETUP ---
#define MAX_LIGHTS 8
struct Light {
    vec3 position;
    vec3 color;
    float intensity;
};

uniform int lightCount;
uniform Light lights[MAX_LIGHTS];

// --- DIRECTIONAL LIGHT UNIFORMS ---
uniform vec3 dirLightDir;
uniform vec3 dirLightColor;
uniform float dirLightIntensity;
uniform int hasDirLight;

void main() {
    vec4 texelColor = texture(texture0, fragTexCoord);
    
    // Core properties that will drive the lighting math
    vec3 baseColor;
    float roughness;
    float metallic;
    float emission;

    // ==========================================
    // EXACT MATCH TO RAYTRACER LOGIC
    // ==========================================
    if (useMaterial == 1) {
        // Material is King: Ignore Shape Color
        baseColor = matAlbedo.rgb * texelColor.rgb;
        roughness = matRoughness;
        metallic = matMetallic;
        emission = matEmission;
    } else {
        // Fallback: Use Shape Color (passed via fragColor)
        baseColor = fragColor.rgb * colDiffuse.rgb * texelColor.rgb;
        roughness = 1.0; // Default to Matte chalk
        metallic = 0.0;
        emission = 0.0;
    }

    // PBR Approximations for OpenGL
    float shininess = mix(4.0, 256.0, 1.0 - roughness); 
    vec3 specColor = mix(vec3(1.0), baseColor, metallic);

    vec3 totalLighting = vec3(0.0);
    vec3 norm = normalize(fragNormal);
    vec3 viewDir = normalize(viewPos - fragPosition);

    // --- LOOP POINT LIGHTS ---
    for (int i = 0; i < lightCount; i++) {
        float ambientStrength = 0.05;
        vec3 ambient = ambientStrength * lights[i].color;

        vec3 lightDir = normalize(lights[i].position - fragPosition);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lights[i].color * lights[i].intensity;

        float specularStrength = mix(1.0, 0.1, roughness); 
        vec3 reflectDir = reflect(-lightDir, norm);  
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
        vec3 specular = specularStrength * spec * specColor * lights[i].color * lights[i].intensity;  

        float distance = length(lights[i].position - fragPosition);
        float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));
        
        totalLighting += (ambient + diffuse + specular) * attenuation;
    }

    // --- DIRECTIONAL LIGHT MATH ---
    if (hasDirLight == 1) {
        vec3 lightDir = normalize(-dirLightDir);
        
        float ambientStrength = 0.15; 
        vec3 ambient = ambientStrength * dirLightColor;
        
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * dirLightColor * dirLightIntensity;
        
        float specularStrength = mix(1.0, 0.1, roughness);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
        vec3 specular = specularStrength * spec * specColor * dirLightColor * dirLightIntensity;
        
        totalLighting += (ambient + diffuse + specular); 
    }

    // Combine lighting with base color, then add the glowing emission!
    vec3 result = (totalLighting * baseColor) + (baseColor * emission);
    
    result = clamp(result, 0.0, 1.0);
    finalColor = vec4(result, 1.0);
}