#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform float uTime;
uniform vec2  uResolution;
uniform vec2  uMouse;

void main() {
    FragColor = vec4(vUV, 0.0, 1.0);
}
