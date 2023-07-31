#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform float u_RocketRow;

void main() {
    FragColor = vec4(TexCoord, sin(u_RocketRow * 6.283 / 8.), 1.);
}