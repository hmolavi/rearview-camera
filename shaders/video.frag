#version 330 core

uniform sampler2D uFrame;

in vec2 vUv;
out vec4 fragColor;

void main() {
    // OpenCV hands us BGR bytes, uploaded as if they were RGB. Swizzle them back here: free on the
    // GPU, and it avoids a per-frame cvtColor pass on the CPU.
    fragColor = vec4(texture(uFrame, vUv).bgr, 1.0);
}
