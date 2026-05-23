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

// --- LIGHTING UNIFORMS ---
uniform vec3 viewPos;       // Camera position
uniform vec3 lightPos;      // Position of the light
uniform vec3 lightColor;    // Color of the light
uniform float lightIntensity;

void main()
{
    // Base texture color
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 baseColor = texelColor.rgb * colDiffuse.rgb * fragColor.rgb;

    // 1. Ambient Light (So shadows aren't pitch black)
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    // 2. Diffuse Light (Direct hit from the light source)
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(lightPos - fragPosition);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * lightIntensity;

    // 3. Specular Light (Shiny highlights)
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - fragPosition);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0); // 32 is the "shininess"
    vec3 specular = specularStrength * spec * lightColor;  

    // Combine them all!
    vec3 result = (ambient + diffuse + specular) * baseColor;
    
    finalColor = vec4(result, texelColor.a * colDiffuse.a);
}