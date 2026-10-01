#version 330 core
// Draws the camera frame as a fullscreen quad. No vertex buffer: the four corners are generated
// from gl_VertexID, drawn as a GL_TRIANGLE_STRIP.

uniform vec2 uFit;     // letterbox scale so the image keeps its aspect ratio in the window
uniform float uMirror; // -1 mirrors horizontally (rear cameras are shown mirror-image), +1 not

out vec2 vUv;

void main() {
    vec2 corner = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1)); // (0,0) (1,0) (0,1) (1,1)
    vec2 ndc = corner * 2.0 - 1.0;
    // Image row 0 is the top of the picture, but texture v = 0 is the bottom of the screen.
    vUv = vec2(corner.x, 1.0 - corner.y);
    gl_Position = vec4(ndc * uFit * vec2(uMirror, 1.0), 0.0, 1.0);
}
