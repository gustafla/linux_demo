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

uniform r_Light {
    vec3 color;
    vec3 direction;
} dlight;

uniform sampler2D u_FeedbackSampler;

#define PI 3.14159265
#define EPSILON 0.001

#include "rotation.glsl"
#include "sdf.glsl"

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
float fbm(vec2 st) {
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

const vec3 MTL_COLORS[] = vec3[](
        vec3(0.04, 0.033, 0.03) * 0.7, // Water absorptivity
        vec3(0.9),
        vec3(0.56, 0.57, 0.58),
        vec3(0.8, 0.14, 0.12)
    );

// x = roughness, y = metalness, z = reflectance
const vec3 MTL_PARAMS[] = vec3[](
        vec3(0.), // Unused
        vec3(0.1, 0., 0.9),
        vec3(0.9, 0., 0.1),
        vec3(0.4, 0.3, 0.2)
    );

vec2 sdSea(vec3 p) {
    p.y += sin(p.z * 0.225 + r_AnimationTime * 0.3) * 1.;
    p.y += sin(p.z * 0.15 + r_AnimationTime * 0.4) * 0.6;
    p.y += sin(p.z * 0.15 - r_AnimationTime * 0.6) * 0.6;
    p.y += sin(p.x * 0.125 + r_AnimationTime * 0.3) * 1.;
    p.xz = rotation2D(.1) * p.xz;
    p.y += abs(sin(p.x * 0.25 + r_AnimationTime * 0.8)) * 0.5;
    p.y += abs(sin(p.x * 0.22 - r_AnimationTime * 0.2)) * 0.5;
    p.xz = rotation2D(.6) * p.xz;
    p.y += abs(sin(p.x * 0.5 + r_AnimationTime * 0.7)) * 0.25;
    p.y += abs(sin(p.x * 0.22 - r_AnimationTime * 0.2)) * 0.5;
    p.xz = rotation2D(1.9) * p.xz;
    p.y += abs(sin(p.x + r_AnimationTime)) * 0.125;
    return vec2(p.y, 0.);
}

vec2 sdMtn(vec3 p) {
    p.y -= 20. + fbm(p.xz / 40.) * 3.4;
    p.y += length(p.xz) * 0.7 + sin(p.x / 10.) * 3.;
    float stripes = clamp(sin(p.x) * 1000., 0.0, 1.);
    return vec2(sdPlaneXZ(p), stripes + 1.);
}

vec2 sdBuoy(vec3 p, float f) {
    return opUnion(
        vec2(sdSphere(p, 5.), 3.),
        opUnion(
            vec2(sdSphere(p * 0.99 - vec3(0., 1.2, 0.), 5.), 1.),
            vec2(sdSphere(p * 0.99 - vec3(0., -1.2, 0.), 5.), 1.),
            f
        ),
        f
    );
}

vec2 sdf(vec3 p, float f) {
    return opUnion(
        opUnion(sdSea(p), sdMtn(p), f),
        sdBuoy(rotation3D(vec3(0.1, 1., 0.5), r_AnimationTime * 0.16) * (p - vec3(200, sin(r_AnimationTime) * 0.7 + sin(r_AnimationTime * 0.4), 0.)), f),
        f
    );
}

#include "march.glsl"

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
    // is a sum of diffuse and a specular part.
    // Specular is a product of Fresnel reflectance,
    // normal distribution function and a geomertry term (microfacet shadowing)
    // divided by the product of n dot l and n dot v.

    vec3 H = normalize(V + L);
    float NdotV = clamp(dot(N, V), EPSILON, 1.0);
    float NdotL = clamp(dot(N, L), EPSILON, 1.0);
    float NdotH = clamp(dot(N, H), EPSILON, 1.0);
    float VdotH = clamp(dot(V, H), EPSILON, 1.0);
    float LdotV = clamp(dot(L, V), EPSILON, 1.0);

    vec3 f0 = vec3(0.16 * (reflectance * reflectance));
    f0 = mix(f0, baseColor, metallic);

    vec3 F = fresnelSchlick(VdotH, f0);
    float D = D_GGX(NdotH, roughness);
    float G = G_Smith(NdotV, NdotL, roughness);

    vec3 spec = (F * D * G) / (4. * max(NdotV, EPSILON) * max(NdotL, EPSILON));

    vec3 rhoD = baseColor;
    rhoD *= vec3(1.) - F;
    rhoD *= (1. - metallic);
    //https://github.com/ranjak/opengl-tutorial/blob/master/shaders/illumination/diramb_orennayar_pcn.vert
    float sigma = roughness;
    float sigma2 = sigma * sigma;
    float termA = 1.0 - 0.5 * sigma2 / (sigma2 + 0.57);
    float termB = 0.45 * sigma2 / (sigma2 + 0.09);
    float cosAzimuthSinaTanb = (LdotV - NdotV * NdotL) / max(NdotV, NdotL);
    vec3 diff = rhoD * (termA + termB * max(0.0, cosAzimuthSinaTanb)) / PI;
    //vec3 diff = rhoD / PI;

    return diff + spec;
}

// Compute light output for a world position
// Rendering Equation:
// Radiance out to view = Emitted radiance to view
// + integral (sort of like sum) over the whole hemisphere:
// brdf(v, l) * incoming irradiance (radiance per area)
vec3 light(vec3 pos, vec3 dir, vec3 n, vec3 l, vec3 lc, vec3 ga, int mtlID, vec3 paramOffset) {
    // No emissive surfaces
    vec3 albedo = MTL_COLORS[mtlID];
    vec3 params = clamp(MTL_PARAMS[mtlID] + paramOffset, 0., 1.);
    // Trace shadow
    float shadow = clamp(march(pos, l, vec3(1., 1024., 30.), 1.).z, 0., 1.);
    // Light received by the surface
    vec3 irradiance = max(dot(l, n), 0.) * shadow * lc;
    // Add a bit of fake ambient light from the sky
    // (should be an integral of all light over fragment's hemisphere but isn't)
    irradiance += sky(n);
    // Attenuate irradiance (in this case, when water blocks light)
    irradiance *= ga;
    // Compute BRDF
    vec3 brd = brdf(l, -dir, n, params.y, params.x, albedo, params.z);
    return irradiance * brd;
}

vec3 water(vec3 pos, vec3 dir, vec3 n, vec3 l, vec3 lc) {
    // This is how much the water medium absorbs light (RGB)
    vec3 absorptivity = MTL_COLORS[0];

    // The water surface reflects some light,
    // we sample the reflection straight from the sky.
    // For realistic reflections, we would need to spheretrace in the
    // reflected direction.
    vec3 reflected = sky(reflect(dir, n));

    // The ratio between reflected and transmitted light is the
    // Fresnel factor, computed by Schlick's approximation
    vec3 fresnel = fresnelSchlick(dot(n, -dir), vec3(0.02));

    // The air-water -interface also refracts what is seen through
    vec3 rd = refract(dir, n, 1. / 1.333);

    // Some light is scattered in the medium. To model this correctly,
    // we would need a volumetric raymarching algorithm, but here
    // we are just doing a cheaper trick to add some color to the
    // crests of the waves
    vec3 scattered = vec3(0.);
    vec3 scatterAlbedo = vec3(0.1, 0.2, 0.1) * 0.005;
    for (int i = 0; i < 4; i++) {
        vec3 samplePos = pos - vec3(0., float(i) * 0.5, 0.);
        float shadow = clamp(march(samplePos, l, vec3(1., 256., 10.), 1.).z, 0., 1.);
        vec3 irradiance = max(dot(l, n), 0.) * lc * shadow;
        float t = max(-rd.y - samplePos.y, 0.) * 2.;
        scattered += exp(-absorptivity * t) * lc * scatterAlbedo * irradiance;
    }

    // Surface also transmits some light from below,
    // account for the surfaces underwater by marching into the water
    vec3 hit = march(pos, rd, vec3(EPSILON, 1024., 20.), 1.);
    pos = pos + rd * hit.x;
    n = normal(pos, 1.);
    // Deeper parts of the underwater surface receive less light
    // (Beer-Lambert law), also the surface reflection blocks some light
    vec3 fl = fresnelSchlick(dot(vec3(0., 1., 0), l), vec3(0.02));
    vec3 attenuation = max(exp(-absorptivity * -pos.y) - fl, 0.);
    vec3 transmitted = light(pos, rd, n, l, lc, attenuation, int(hit.y), vec3(1., 0., 0.));
    // The light transmitted by the water is attenuated exponentially
    // (Beer-Lambert law)
    transmitted *= exp(-absorptivity * hit.x);

    // Final radiance is a mix of reflected and transmitted light
    return mix(transmitted + scattered, reflected, fresnel);
}

void main() {
    vec3 ray = viewMatrix() * cameraRay();
    vec3 lightDir = normalize(dlight.direction);

    // Spheretrace all surfaces in view
    vec3 hit = march(cam.pos, ray, vec3(EPSILON, 1024., 20.), 0.);
    vec3 pos = cam.pos + ray * hit.x;
    vec3 n = normal(pos, 0.);
    int mtlID = int(hit.y);

    // Compute a mask for parts of the image that should be sky (ray didn't hit)
    // Ideally, this would be done with the shadow parameter (hit.z)
    // but a fog works well in this case for now
    float mask = clamp(hit.x / 350. - 1., 0., 1.);

    vec3 radiance = vec3(0.);

    // Material 0 is water, special case
    if (mtlID == 0) {
        radiance = water(pos, ray, n, -lightDir, dlight.color);
    } else {
        radiance = light(pos, ray, n, -lightDir, dlight.color, vec3(1.), int(hit.y), vec3(0.));
    }

    FragColor = vec4(mix(radiance, sky(ray), mask), 1.);
}
