#version 330 core

out vec4 FragColor;

in vec2 FragCoord;

uniform sampler2D u_InputSampler;
uniform sampler2D u_NoiseSampler;
uniform int u_NoiseSize;
uniform float u_RocketRow;
uniform vec2 u_Resolution;
uniform float r_PostAberration;

#define BLUR_SAMPLES 8

vec3 acesApprox(vec3 v) {
    v *= 0.6;
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((v*(a*v+b))/(v*(c*v+d)+e), 0., 1.);
}

vec3 radialSum(vec2 r) {
    vec3 color = vec3(0.);
    for (int i = 0; i < BLUR_SAMPLES; i++) {
        vec2 d = (r * float(i) * r_PostAberration) / BLUR_SAMPLES;
        vec2 uv = FragCoord * (0.5 + d) + 0.5;
        color += texture2D(u_InputSampler, uv).rgb;
    }
    return color;
}

void main() {
    // Input color with RGB aberration
    vec2 pixel = 1. / u_Resolution;
    vec3 color = vec3(
        radialSum(pixel).r,
        radialSum(pixel * 2.).g,
        radialSum(pixel * 3.).b
    ) / BLUR_SAMPLES;

    // Tone mapping
    color = acesApprox(color);

    // Add noise
    color += texelFetch(u_NoiseSampler, ivec2(gl_FragCoord.xy) % u_NoiseSize, 0).rgb * 0.08 - 0.04;

    FragColor = vec4(color, 1.);
}
