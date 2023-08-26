// This shader gets run last for eveything we've rendered so far.
// It currently features bloom, tone mapping, chromatic aberration and noise.

out vec4 FragColor;

in vec2 FragCoord;

uniform sampler2D u_InputSampler;
uniform sampler2D u_NoiseSampler;
uniform sampler2D u_BloomSampler;
uniform int u_NoiseSize;
uniform float u_RocketRow;
uniform vec2 u_Resolution;

uniform r_Post {
    float aberration;
} post;

#define BLUR_SAMPLES 8

// https://64.github.io/tonemapping/
vec3 acesApprox(vec3 v) {
    v *= 0.6;
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((v*(a*v+b))/(v*(c*v+d)+e), 0., 1.);
}

// This is used to achieve chromatic aberration
vec3 radialSum(vec2 r) {
    vec3 color = vec3(0.);
    for (int i = 0; i < BLUR_SAMPLES; i++) {
        vec2 d = (r * float(i) * post.aberration) / BLUR_SAMPLES;
        vec2 uv = FragCoord * (0.5 - d) + 0.5;
        color += texture2D(u_InputSampler, uv).rgb;
    }
    return color;
}

void main() {
    // Input color with RGB aberration
    vec2 pixel = 1. / u_Resolution;
    vec3 color = vec3(
        radialSum(pixel * 3.).r,
        radialSum(pixel * 2.).g,
        radialSum(pixel * 1.).b
    ) / BLUR_SAMPLES;

    // Add bloom
    color += texture2D(u_BloomSampler, FragCoord * 0.5 + 0.5).rgb;

    // Tone mapping
    color = acesApprox(color);

    // Add noise
    color += texelFetch(u_NoiseSampler, ivec2(gl_FragCoord.xy) % u_NoiseSize, 0).rgb * 0.08 - 0.04;

    FragColor = vec4(color, 1.);
}
