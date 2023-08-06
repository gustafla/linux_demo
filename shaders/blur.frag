out vec4 FragColor;

uniform sampler2D u_InputSampler;

const float kernel[30] = float[30](0.026596, 0.026537, 0.026361, 0.026070, 0.025667, 0.025159, 0.024551, 0.023852, 0.023070, 0.022215, 0.021297, 0.020326, 0.019313, 0.018269, 0.017205, 0.016131, 0.015058, 0.013993, 0.012946, 0.011924, 0.010934, 0.009982, 0.009072, 0.008209, 0.007395, 0.006632, 0.005921, 0.005263, 0.004658, 0.004104);

void main() {
    vec3 color = texelFetch(u_InputSampler, ivec2(gl_FragCoord.xy), 0).rgb * kernel[0];
    for (int i = 1; i < 30; i++) {
        ivec2 offset = ivec2(0);
#ifdef HORIZONTAL
        offset.x = i;
#else
        offset.y = i;
#endif
        color += texelFetch(u_InputSampler, ivec2(gl_FragCoord.xy) + offset, 0).rgb * kernel[i];
        color += texelFetch(u_InputSampler, ivec2(gl_FragCoord.xy) - offset, 0).rgb * kernel[i];
    }
    FragColor = vec4(color, 1.);
}
