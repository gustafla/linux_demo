// This shader is used to separate bright parts from the input image.
// Read more about the bloom effect from
// https://learnopengl.com/Advanced-Lighting/Bloom

precision highp float;

out vec4 FragColor;

in vec2 FragCoord;

uniform r_Post {
    float bloomTreshold;
} post;

uniform sampler2D u_InputSampler;

void main() {
    vec3 c = texture2D(u_InputSampler, FragCoord * 0.5 + 0.5).rgb;
    float brightness = dot(c, vec3(0.2126, 0.7152, 0.0722));
    if (brightness < 1. + post.bloomTreshold) {
        discard;
    }
    FragColor = vec4(c, 1.);
}
