#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D u_InputSampler;
uniform sampler2D u_NoiseSampler;
uniform int u_NoiseSize;
uniform float u_RocketRow;

vec3 aces_approx(vec3 v) {
    v *= 0.6;
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((v*(a*v+b))/(v*(c*v+d)+e), 0., 1.);
}

void main() {
    // Input color
    vec3 color = texture2D(u_InputSampler, TexCoord).rgb;

    // Add bloom
    //color += texture2D(u_BloomSampler, uv).rgb;

    // Tone mapping
    color = aces_approx(color);

    // Add noise
    color += texelFetch(u_NoiseSampler, ivec2(gl_FragCoord.xy) % u_NoiseSize, 0).rgb * 0.08 - 0.04;

    FragColor = vec4(color, 1.);
}
