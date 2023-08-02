#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform float u_RocketRow;
uniform float r_TestValue;
uniform vec3 r_CamPos;

void main() {
    FragColor = vec4(TexCoord, sin(u_RocketRow * 6.283 / 8.) * r_TestValue, 1.);
    FragColor += vec4(0, 1, 1, 0);
}
