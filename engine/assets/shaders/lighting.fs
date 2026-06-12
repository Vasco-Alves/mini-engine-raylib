#version 330

// ── Inputs from vertex shader ──────────────────────────────────────────────
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

out vec4 finalColor;

// ── Raylib built-ins ────────────────────────────────────────────────────────
uniform sampler2D texture0;
uniform vec4      colDiffuse;

// ── Custom uniforms ─────────────────────────────────────────────────────────
uniform vec3 viewPos;

uniform int   useMaterial;
uniform vec4  matAlbedo;
uniform float matRoughness;
uniform float matMetallic;
uniform float matEmission;

#define MAX_LIGHTS 8
struct Light {
    vec3  position;
    vec3  color;
    float intensity;
};
uniform int   lightCount;
uniform Light lights[MAX_LIGHTS];

uniform vec3  dirLightDir;
uniform vec3  dirLightColor;
uniform float dirLightIntensity;
uniform int   hasDirLight;

// ============================================================
//  PBR HELPERS  (Cook-Torrance BRDF)
// ============================================================
const float PI = 3.14159265359;

// GGX (Trowbridge-Reitz) Normal Distribution Function
float D_GGX(float NdotH, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// Schlick-GGX visibility term (one side)
float V_SchlickGGX(float NdotX, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) * 0.125;          // k = r² / 8
    return NdotX / (NdotX * (1.0 - k) + k);
}

// Smith's combined visibility
float V_Smith(float NdotV, float NdotL, float roughness) {
    return V_SchlickGGX(NdotV, roughness) * V_SchlickGGX(NdotL, roughness);
}

// Fresnel-Schlick approximation
vec3 F_Schlick(float HdotV, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - HdotV, 0.0, 1.0), 5.0);
}

// ============================================================
//  COOK-TORRANCE BRDF  —  evaluates a single light direction
// ============================================================
vec3 cook_torrance(vec3 N, vec3 V, vec3 L,
                   vec3 baseColor, float r, float metallic, vec3 F0)
{
    vec3  H     = normalize(V + L);
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    if (NdotL <= 0.0) return vec3(0.0);

    float NDF  = D_GGX(NdotH, r);
    float G    = V_Smith(NdotV, NdotL, r);
    vec3  F    = F_Schlick(HdotV, F0);

    vec3  spec = (NDF * G * F) / max(4.0 * NdotV * NdotL, 0.001);

    // Energy-conserving diffuse: metals have no Lambertian term
    vec3  kD   = (vec3(1.0) - F) * (1.0 - metallic);

    return (kD * baseColor / PI + spec) * NdotL;
}

// ============================================================
//  SKY AMBIENT  — horizon-to-zenith gradient
//  Approximates the hemisphere irradiance of the raytracer sky.
// ============================================================
vec3 sky_irradiance(vec3 N) {
    float t      = clamp(N.y * 0.5 + 0.5, 0.0, 1.0); // [-1,1] → [0,1]
    vec3 horizon = vec3(0.87, 0.87, 0.88);             // warm grey horizon
    vec3 zenith  = vec3(0.35, 0.52, 0.82);             // sky blue
    return mix(horizon, zenith, t);
}

// ============================================================
//  ACES FILMIC TONEMAPPER  (matches the raytracer's curve)
// ============================================================
vec3 aces_tonemap(vec3 x) {
    return clamp(
        (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14),
        0.0, 1.0
    );
}

// ============================================================
//  MAIN
// ============================================================
void main() {
    vec4 texelColor = texture(texture0, fragTexCoord);

    // ── Resolve material properties ─────────────────────────
    vec3  baseColor;
    float roughness;
    float metallic;
    float emissionPower;

    if (useMaterial == 1) {
        // MaterialComponent overrides shape color (matches raytracer)
        baseColor     = matAlbedo.rgb * texelColor.rgb;
        roughness     = clamp(matRoughness, 0.04, 1.0);
        metallic      = clamp(matMetallic,  0.0,  1.0);
        emissionPower = matEmission;
    } else {
        // Fallback: use vertex/shape color
        baseColor     = fragColor.rgb * colDiffuse.rgb * texelColor.rgb;
        roughness     = 1.0;
        metallic      = 0.0;
        emissionPower = 0.0;
    }

    // Dielectrics: ~4 % reflectance at normal incidence (F0 = 0.04)
    // Metals: use albedo as the tinted F0
    vec3  F0    = mix(vec3(0.04), baseColor, metallic);
    vec3  N     = normalize(fragNormal);
    vec3  V     = normalize(viewPos - fragPosition);
    float NdotV = max(dot(N, V), 0.0);

    // Clamp roughness away from 0 to prevent NDF singularity
    float r = max(roughness, 0.04);

    vec3 Lo = vec3(0.0);   // accumulated direct radiance

    // ── Point lights ────────────────────────────────────────
    for (int i = 0; i < lightCount; i++) {
        vec3  L       = normalize(lights[i].position - fragPosition);
        float dist    = length(lights[i].position - fragPosition);
        // Inverse-square falloff; +0.5 avoids singularity at dist ≈ 0
        float atten   = lights[i].intensity / (dist * dist + 0.5);
        vec3  radiance = lights[i].color * atten;
        Lo += cook_torrance(N, V, L, baseColor, r, metallic, F0) * radiance;
    }

    // ── Directional light ───────────────────────────────────
    if (hasDirLight == 1) {
        vec3 L        = normalize(-dirLightDir);
        vec3 radiance = dirLightColor * dirLightIntensity;
        Lo += cook_torrance(N, V, L, baseColor, r, metallic, F0) * radiance;
    }

    // ── Sky-based ambient (IBL approximation) ───────────────
    // Uses the surface normal to sample a horizon-zenith gradient,
    // giving the same warm/cool split the raytracer produces from sky.
    vec3 skyColor  = sky_irradiance(N);
    vec3 F_amb     = F_Schlick(NdotV, F0);
    vec3 kD_amb    = (vec3(1.0) - F_amb) * (1.0 - metallic);
    // Lambertian diffuse + tinted specular from sky for metals
    vec3 ambient   = kD_amb * baseColor * skyColor * 0.22
                   + F_amb  * skyColor  * 0.18;

    // ── Emissive ────────────────────────────────────────────
    vec3 emissive = baseColor * emissionPower;

    // ── Combine → ACES tonemap ───────────────────────────────
    // NOTE: No explicit gamma correction here.
    // Raylib does not enable GL_FRAMEBUFFER_SRGB, so the framebuffer
    // is treated as raw linear storage and gamma is not applied by the
    // driver. Adding pow(1/2.2) here would double-apply gamma in debug
    // contexts (where some drivers enable sRGB output implicitly) and
    // mismatch the default Raylib shader used for the grid and UI.
    // ACES tonemapping already provides the film-like look on its own.
    vec3 color = ambient + Lo + emissive;
    color = aces_tonemap(color);

    finalColor = vec4(color, 1.0);
}
