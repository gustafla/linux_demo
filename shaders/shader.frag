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

uniform r_Cam {
    float fov;
    vec3 pos;
    vec3 target;
} cam;

uniform sampler2D u_FeedbackSampler;

#define PI 3.14159265
#define EPSILON 0.001

#include "shaders/sdf.glsl"

float aspectRatio() {
    return u_Resolution.x / u_Resolution.y;
}

mat3 viewMatrix() {
	vec3 f = normalize(cam.target - cam.pos);
	vec3 s = cross(f, vec3(0., 1., 0.));
	vec3 u = cross(s, f);
    return mat3(s, u, f);
}

vec3 cameraRay() {
    float c = tan((90. - cam.fov / 2.) * (PI / 180.));
    return normalize(vec3(FragCoord * vec2(aspectRatio(), 1.), c));
}

float random (in vec2 st) {
    return fract(sin(dot(st.xy,
                         vec2(12.9898,78.233)))*
        43758.5453123);
}

// Based on Morgan McGuire @morgan3d
// https://www.shadertoy.com/view/4dS3Wd
float noise (in vec2 st) {
    vec2 i = floor(st);
    vec2 f = fract(st);

    // Four corners in 2D of a tile
    float a = random(i);
    float b = random(i + vec2(1.0, 0.0));
    float c = random(i + vec2(0.0, 1.0));
    float d = random(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(a, b, u.x) +
            (c - a)* u.y * (1.0 - u.x) +
            (d - b) * u.x * u.y;
}

#define OCTAVES 8
float fbm (vec2 st) {
    // Initial values
    float value = 0.0;
    float amplitude = .5;

    // Loop of octaves
    for (int i = 0; i < OCTAVES; i++) {
        float n = noise(st);
        value += amplitude * n * n;
        st *= 2.;
        amplitude *= .5;
    }
    return value;
}    

float sdf(vec3 p) {
    p.y += pow(fbm(p.xz * 0.5), 0.1) * 10.;
    return sdPlaneXZ(p);
}

#include "shaders/march.glsl"

vec3 sky(vec3 v) {
    return mix(vec3(0.753, 0.718, 0.690), vec3(0.302, 0.439, 0.651), vec3(pow(clamp(v.y * 0.5 + 0.5, 0., 1.), 0.4)));
}

void main() {
    vec3 ray = viewMatrix() * cameraRay(); 
    vec3 l = vec3(0., -1., 0.);

    float t = march(cam.pos, ray);
    vec3 pos = cam.pos + ray * t;

    vec3 normal = normal(pos);
    float ndotl = max(dot(-normal, l), 0.);

    vec3 surfaceColor = vec3(1.) * ndotl;

    FragColor = vec4(mix(surfaceColor, sky(ray), clamp(t / 100., 0., 1.)), 1.);
    //FragColor = vec4(vec3(fbm(FragCoord * 10.)), 1.);
}

    //vec2 uv = FragCoord * 0.5 + 0.5;
    //vec3 previousFrameColor = texture2D(u_FeedbackSampler, uv).rgb;
    //FragColor = vec4(mix(finalColor, previousFrameColor, r_MotionBlur), 1.);
