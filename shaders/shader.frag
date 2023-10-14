// Based on writing by Jamie Wong
// https://jamie-wong.com/2016/07/15/ray-marching-signed-distance-functions
// and Inigo Quilez
// https://iquilezles.org/articles/distfunctions/

precision highp float;

out vec4 FragColor;

in vec2 FragCoord;

uniform float u_RocketRow;
uniform vec2 u_Resolution;
uniform float r_MotionBlur;
uniform float r_AnimationTime;

uniform r_Cam {
    float fov;
    vec3 pos;
    vec3 target;
} cam;

uniform r_Sky {
    vec3 color1;
    vec3 color2;
    vec2 brightness;
} u_sky;

uniform sampler2D u_FeedbackSampler;

#define PI 3.14159265
#define EPSILON 0.001

mat2 rotation(float a) {
    return mat2(
        cos(a), -sin(a),
        sin(a), cos(a)
    );
}

#include "shaders/sdf.glsl"

float aspectRatio() {
    return u_Resolution.x / u_Resolution.y;
}

mat3 viewMatrix() {
	vec3 f = normalize(cam.target - cam.pos);
	vec3 s = normalize(cross(f, vec3(0., 1., 0.)));
	vec3 u = cross(s, f);
    return mat3(s, u, f);
}

vec3 cameraRay() {
    float c = tan((90. - cam.fov / 2.) * (PI / 180.));
    return normalize(vec3(FragCoord * vec2(aspectRatio(), 1.), c));
}

float motion(vec2 st, float phase) {
    st.x += sin(st.y + phase + 0.41) * 2.;
    return sin(st.x + phase);
}

#define OCTAVES 4
float fbm (vec2 st) {
    // Initial values
    float value = 0.0;
    float amplitude = .5;

    // Loop of octaves
    for (int i = 0; i < OCTAVES; i++) {
        value += pow(1. - abs(motion(st, amplitude * 3.14) * amplitude), 2.5);
        st *= 2.;
        amplitude *= .5;
    }
    return value;
}

float sdMtn(vec3 p) {
    p.y -= fbm(p.xz / 40.) * 3.4;
    p.y += length(p.xz) * 0.7 + sin(p.x / 10.) * 3.;
    return sdPlaneXZ(p);
}

float sdSea(vec3 p) {
    p.y -= sin(p.x * 0.125 + r_AnimationTime) * 1.;
    p.xz = rotation(.1) * p.xz;
    p.y -= sin(p.x * 0.25 + r_AnimationTime) * 0.5;
    p.xz = rotation(.5) * p.xz;
    p.y -= sin(p.x * 0.5 + r_AnimationTime) * 0.25;
    p.xz = rotation(.5) * p.xz;
    p.y -= sin(p.x * 2. + r_AnimationTime) * 0.125;
    p.xz = rotation(.1) * p.xz;
    p.y -= sin(p.x * 4. + r_AnimationTime) * 0.0625;
    return sdPlaneXZ(p);
}

float sdf(vec3 p) {
    return min(sdMtn(p - vec3(0., 20., 0)), sdSea(p));
}

#include "shaders/march.glsl"

vec3 sky(vec3 v) {
    return mix(u_sky.color1 * u_sky.brightness.x, u_sky.color2 * u_sky.brightness.y, vec3(pow(clamp(v.y * 0.5 + 0.5, 0., 1.), 0.4)));
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1. - F0) * pow(1.0 - cosTheta, 5.);
}

float D_GGX(float NdotH, float roughness) {
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float NdotH2 = NdotH * NdotH;
    float b = NdotH2 * (alpha2 - 1.) + 1.;
    return alpha2 / (PI * b * b);
}

float G1_GGX_Schlick(float NdotV, float k) {
    return max(NdotV, EPSILON) / (NdotV * (1. - k) + k);
}

float G_Smith(float NdotV, float NdotL, float roughness) {
    float alpha = roughness * roughness;
    float k = alpha / 2.;
    return G1_GGX_Schlick(NdotL, k) * G1_GGX_Schlick(NdotV, k);
}

vec3 brdf(vec3 L, vec3 V, vec3 N, float metallic, float roughness, vec3 baseColor, float reflectance) {
    // Cook-Torrance Microfacet BRDF
    // is a sum of diffuse (which is just material albedo divided by pi)
    // and a specular part, which is a product of Fresnel reflectance,
    // normal distribution function and a geomertry term (microfacet shadowing)
    // divided by the product of n dot l and n dot v
    
    vec3 H = normalize(V + L);
    float NdotV = clamp(dot(N, V), 0., 1.0);
    float NdotL = clamp(dot(N, L), 0., 1.0);
    float NdotH = clamp(dot(N, H), 0., 1.0);
    float VdotH = clamp(dot(V, H), 0., 1.0);

    vec3 f0 = vec3(0.16 * (reflectance * reflectance));
    f0 = mix(f0, baseColor, metallic);

    vec3 F = fresnelSchlick(VdotH, f0);
    float D = D_GGX(NdotH, roughness);
    float G = G_Smith(NdotV, NdotL, roughness);

    vec3 spec = (F * D * G) / (4. * max(NdotV, EPSILON) * max(NdotL, EPSILON));

    vec3 rhoD = baseColor;
    rhoD *= vec3(1.) - F;
    rhoD *= (1. - metallic);
    vec3 diff = rhoD / PI;

    return diff + spec;
}

void main() {
    vec3 ray = viewMatrix() * cameraRay(); 
    vec3 l = vec3(sin(r_AnimationTime), -1., cos(r_AnimationTime));

    // Spheretrace surface in view
    float t = march(cam.pos, ray);
    vec3 pos = cam.pos + ray * t;
    vec3 normal = normal(pos);

    // Compute direct lighting
    // Rendering Equation:
    // Radiance out to view = Emitted radiance to view
    // + integral (sort of like sum) over the whole hemisphere:
    // brdf(v, l) * incoming irradiance (radiance per area)
    
    vec3 lightDir = normalize(-l);
    vec3 viewDir = normalize(-ray);
    vec3 lightColor = vec3(6.);
    vec3 baseColor = vec3(1.022, 0.782, 0.344); // albedo for dielectrics or F0 for metals
    float roughness = 1.0;
    float metallic = 0.;
    float reflectance = 0.;

    vec3 radiance = vec3(0.); // No emissive surfaces
    float irradiance = max(dot(lightDir, normal), 0.); // Light received by the surface
    vec3 brd = brdf(lightDir, viewDir, normal, metallic, roughness, baseColor, reflectance);
    radiance += irradiance * brd * lightColor;

    FragColor = vec4(mix(radiance, sky(ray), clamp(t / 200. - 1., 0., 1.)), 1.);
}
