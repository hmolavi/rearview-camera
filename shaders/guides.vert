#version 330 core
// Projects guideline vertices from the ground plane (metres) into the camera image.

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec4 aColor;

uniform mat4 uViewProj; // camera pose (view) and lens (projection) in one matrix
uniform vec2 uFit;
uniform float uMirror;

out vec4 vColor;

void main() {
    vColor = aColor;
    vec4 clip = uViewProj * vec4(aPosition, 1.0);
    // Same letterbox and mirror as the video quad, applied in clip space (before the perspective
    // divide, which is fine because both are plain scales) so the lines stay glued to the image.
    clip.xy *= uFit * vec2(uMirror, 1.0);
    gl_Position = clip;
}
