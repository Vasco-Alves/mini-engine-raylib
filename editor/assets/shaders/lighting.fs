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

void main()
{
    // Base texture color
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 baseColor = texelColor.rgb * colDiffuse.rgb * fragColor.rgb;

    vec3 totalLighting = vec3(0.0);
    vec3 norm = normalize(fragNormal);
    vec3 viewDir = normalize(viewPos - fragPosition);

    // Loop through every active point light in the scene
    for (int i = 0; i < lightCount; i++) {
        
        // 1. Ambient
        float ambientStrength = 0.1;
        vec3 ambient = ambientStrength * lights[i].color;

        // 2. Diffuse
        vec3 lightDir = normalize(lights[i].position - fragPosition);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lights[i].color * lights[i].intensity;

        // 3. Specular
        float specularStrength = 0.5;
        vec3 reflectDir = reflect(-lightDir, norm);  
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        vec3 specular = specularStrength * spec * lights[i].color * lights[i].intensity;  

        // Distance attenuation
        float distance = length(lights[i].position - fragPosition);
        float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));
        
        totalLighting += (ambient + diffuse + specular) * attenuation;
    }

    // --- DIRECTIONAL LIGHT MATH ---
    if (hasDirLight == 1) {
        vec3 lightDir = normalize(-dirLightDir);
        
        // 1. Ambient (Simulates scattered sky light)
        float ambientStrength = 0.15; 
        vec3 ambient = ambientStrength * dirLightColor;
        
        // 2. Diffuse
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * dirLightColor * dirLightIntensity;
        
        // 3. Specular
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        vec3 specular = 0.5 * spec * dirLightColor * dirLightIntensity;
        
        // Add to total
        totalLighting += (ambient + diffuse + specular); 
    }

    // Combine the accumulated lighting with the base object color
    vec3 result = totalLighting * baseColor;
    result = clamp(result, 0.0, 1.0);
    finalColor = vec4(result, texelColor.a * colDiffuse.a);
}
