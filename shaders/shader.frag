// Based on writing by Jamie Wong
// https://jamie-wong.com/2016/07/15/ray-marching-signed-distance-functions
// and Inigo Quilez
// https://iquilezles.org/articles/distfunctions/

out vec4 FragColor;

in vec2 FragCoord;

uniform float u_RocketRow;
uniform vec2 u_Resolution;
uniform float r_CamFov;
uniform vec3 r_CamPos;
uniform vec3 r_CamTarget;
uniform float r_MotionBlur;

uniform sampler2D u_FeedbackSampler;

#define PI 3.14159265
#define EPSILON 0.001
#define SAMPLES 4

float aspectRatio() {
    return u_Resolution.x / u_Resolution.y;
}

mat3 viewMatrix() {
	vec3 f = normalize(r_CamTarget - r_CamPos);
	vec3 s = normalize(cross(f, vec3(0., 1., 0.)));
	vec3 u = cross(s, f);
    return mat3(s, u, f);
}

vec3 cameraRay() {
    float c = tan((90. - r_CamFov / 2.) * (PI / 180.));
    return normalize(vec3(FragCoord * vec2(aspectRatio(), 1.), c));
}

float sdSphere( vec3 p, float s ) {
    return length(p)-s;
}

float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz)-t.x,p.y);
    return length(q)-t.y;
}

float sdf(vec3 p) {
    return min(
        sdTorus(p, vec2(1., 0.3)),
        sdSphere(p - vec3(1, 0, 1), 0.5)
    );
}

vec3 normal(vec3 p) {
    return normalize(vec3(
        sdf(vec3(p.x + EPSILON, p.y, p.z)) - sdf(vec3(p.x - EPSILON, p.y, p.z)),
        sdf(vec3(p.x, p.y + EPSILON, p.z)) - sdf(vec3(p.x, p.y - EPSILON, p.z)),
        sdf(vec3(p.x, p.y, p.z  + EPSILON)) - sdf(vec3(p.x, p.y, p.z - EPSILON))
    ));
}

float march(vec3 o, vec3 d) {
    float t = EPSILON;
    float dist = 0.;
    for (int i = 0; i < 128; i++) {
        dist = sdf(o + d * t);
        t += dist * (1. - EPSILON);
        if (dist < EPSILON) {
            break;
        }
    }
    return t;
}

void main() {
    vec3 ray = viewMatrix() * cameraRay(); 

    float t = march(r_CamPos, ray);
    vec3 pos = r_CamPos + ray * t;

    vec3 normal = normal(pos);
    float ndotl = max(dot(-normal, vec3(0., -1., 0)), 0.);

    vec3 finalColor = vec3(ndotl) * 1.2;
    vec2 uv = FragCoord * 0.5 + 0.5;
    vec3 previousFrameColor = texture2D(u_FeedbackSampler, uv).rgb;
    FragColor = vec4(mix(finalColor, previousFrameColor, r_MotionBlur), 1.);
}
