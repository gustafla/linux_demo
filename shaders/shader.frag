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

void main() {
    vec3 ray = viewMatrix() * cameraRay(); 
    vec3 l = vec3(0., -1., 0.);

    float t = march(cam.pos, ray);
    vec3 pos = cam.pos + ray * t;

    vec3 normal = normal(pos);
    float ndotl = max(dot(-normal, l), 0.);

    vec3 surfaceColor = vec3(1.) * ndotl;

    FragColor = vec4(mix(surfaceColor, sky(ray), clamp(t / 200. - 1., 0., 1.)), 1.);
    //FragColor = vec4(surfaceColor, 1.);
    //FragColor = vec4(vec3(fbm(FragCoord * 10.)), 1.);
}

    //vec2 uv = FragCoord * 0.5 + 0.5;
    //vec3 previousFrameColor = texture2D(u_FeedbackSampler, uv).rgb;
    //FragColor = vec4(mix(finalColor, previousFrameColor, r_MotionBlur), 1.);
