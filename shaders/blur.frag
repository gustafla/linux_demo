// This shader is used to blur from the input image in X or Y direction.
// It is currently only used for the bloom effect.
// Read more about the bloom effect from
// https://learnopengl.com/Advanced-Lighting/Bloom

precision highp float;

out vec4 FragColor;

uniform sampler2D u_InputSampler;

#include "shaders/blur_kernel.glsl"

void main() {
    vec3 color = texelFetch(u_InputSampler, ivec2(gl_FragCoord.xy), 0).rgb * kernel[0];
    for (int i = 1; i < KERNEL_SIZE; i++) {
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
