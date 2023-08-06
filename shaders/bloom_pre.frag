out vec4 FragColor;

uniform float r_PostBloomTreshold;
uniform sampler2D u_InputSampler;

void main() {
    vec3 c = texelFetch(u_InputSampler, ivec2(gl_FragCoord.xy), 0).rgb;
    float brightness = dot(c, vec3(0.2126, 0.7152, 0.0722));
    if (brightness < 1. + r_PostBloomTreshold) {
        discard;
    }
    FragColor = vec4(c, 1.);
}
