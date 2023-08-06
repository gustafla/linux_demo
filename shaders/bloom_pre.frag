out vec4 FragColor;

in vec2 FragCoord;

uniform float r_Post_BloomTreshold;
uniform sampler2D u_InputSampler;

void main() {
    vec3 c = texture2D(u_InputSampler, FragCoord * 0.5 + 0.5).rgb;
    float brightness = dot(c, vec3(0.2126, 0.7152, 0.0722));
    if (brightness < 1. + r_Post_BloomTreshold) {
        discard;
    }
    FragColor = vec4(c, 1.);
}
