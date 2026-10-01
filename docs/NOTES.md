# How it works

A walk through the code in the order a frame travels, with the OpenGL concepts each step uses.

## 1. Capture (src/camera.cpp)

`FrameSource` owns a `cv::VideoCapture` and a background thread. The thread blocks on
`read()` (about 33 ms per frame at 30 fps), then swaps the new frame into a mutex-guarded slot and
bumps a sequence number. The render thread asks "anything newer than sequence N?" and copies it out.

- **Latest-frame mailbox, not a queue.** A backup camera must show the present. If rendering
  stalls, old frames are dropped instead of piling up as latency.
- **`cv::swap`, not copy, on the producer side.** The previous buffer comes back for reuse, so the
  steady state allocates nothing.
- The test pattern draws a parking stall by projecting ground points through the same
  `CameraModel` the overlay uses. That is why the guidelines sit inside the painted stall.

## 2. Upload (src/gl_utils.cpp, `VideoTexture`)

The frame becomes a 2D texture. First frame (or a size change): `glTexImage2D` allocates storage.
Every frame after: `glTexSubImage2D` overwrites it in place, which is cheaper.

- `GL_UNPACK_ALIGNMENT = 1`: GL assumes rows start on 4-byte boundaries by default. A 3-byte BGR
  pixel row of odd width breaks that and shears the image diagonally.
- Uploads only happen when the sequence number changed. Display runs at 60 Hz, the camera at 30.
- This upload is synchronous: the CPU waits while the driver copies. The fix is a **pixel buffer
  object (PBO)**, which lets the copy happen via DMA while the CPU moves on (roadmap item).

## 3. Draw the video (shaders/video.vert, video.frag)

A fullscreen quad with **no vertex buffer**: the vertex shader derives the four corners from
`gl_VertexID` and the app calls `glDrawArrays(GL_TRIANGLE_STRIP, 0, 4)`. Core profile still
requires *a* VAO to be bound, hence `emptyVao_`.

- `uFit` letterboxes the image to keep its aspect ratio in any window shape.
- `uMirror` flips x. Rear cameras display a mirror image so left on screen is left behind you.
- The fragment shader swizzles `.bgr`: OpenCV delivers BGR, and swapping on the GPU costs nothing,
  where a CPU `cvtColor` would touch every pixel every frame.

## 4. Build the guidelines (src/guidelines.cpp)

Pure geometry on the CPU, in metres on the ground plane.

- **Bicycle model.** With front wheels at angle d, the car rotates around a point on the rear
  axle line, `R = wheelbase / tan(d)` to the side. Every point of the car traces a circle around it.
  The guidelines are the arcs traced by the two rear corners. d = 0 is the straight-line special case.
- **Ribbons, not lines.** Each guideline is a strip of triangles with real width (8 cm). Core
  profile caps `glLineWidth` at 1 px, and a strip lying on the ground gets perspective for free:
  it thins and converges with distance.
- Bands are generated separately so the colour change lands exactly at 1 m and 2 m.
- Rebuilt only when the steering angle changes, uploaded with `glBufferData(..., GL_DYNAMIC_DRAW)`.

## 5. Project and draw the guidelines (src/camera_model.h, shaders/guides.vert)

One VBO of interleaved `{vec3 position, vec4 color}`. The VAO records the layout:
attribute 0 = 3 floats at offset 0, attribute 1 = 4 floats at offset 12, stride 28 bytes.

The vertex shader multiplies each ground point by `uViewProj`:

- **View matrix** (`glm::lookAt`): moves the world so the camera is at the origin looking down
  -z. Camera 1 m up, pitched 30 degrees down, looking back from the bumper.
- **Projection matrix** (`glm::perspective`): the lens. Field of view and aspect must match the
  real camera or the lines will not sit on the real ground. This is **extrinsic** (pose) vs
  **intrinsic** (lens) calibration, which is what camera tuning on a real car pins down.
- The GPU then divides by w (the **perspective divide**), which is what makes far things small.
- `uFit` and `uMirror` are applied in clip space, the same as the video, so the overlay stays glued
  to the image.

Alpha blending (`GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`) makes the lines slightly see-through.

## 6. Present (src/app.cpp)

`glfwSwapBuffers` with vsync on (`glfwSwapInterval(1)`). We draw into the back buffer while the
display shows the front buffer, then swap: no tearing. Title bar stats:

- **capture fps**: frames the camera actually delivered.
- **render fps**: loop iterations, locked to the display refresh by vsync.
- **cpu ms**: time to issue the draw calls. Not GPU time: GL calls are queued and run later.
- **latency**: from capture to just after the swap. The real figure also includes sensor exposure
  and display scan-out, which software cannot see.

## The pipeline, in one breath

Vertex data (VBO, described by a VAO) -> **vertex shader** (per vertex: transform to clip space) ->
primitive assembly and clipping -> perspective divide and viewport transform -> **rasterization**
(triangles to fragments) -> **fragment shader** (per pixel: colour, here a texture lookup) ->
blending -> framebuffer -> swap.

## Questions worth being able to answer

- Vertex vs fragment shader: what runs per vertex, what runs per pixel, what gets interpolated
  between them (`out`/`in` varyings like `vUv` and `vColor`).
- VAO vs VBO: the VBO is the bytes, the VAO is the description of how to read them.
- Uniform vs attribute: same for the whole draw call vs different per vertex.
- Why `glTexSubImage2D` over `glTexImage2D` every frame; what a PBO would add.
- Why the image is mirrored, and why the overlay has to be mirrored the same way.
- What happens to latency if the camera thread used a queue instead of a mailbox.
- What changes for OpenGL ES on an embedded GPU: `#version 300 es`, precision qualifiers,
  no `GL_BGR`, EGL instead of GLFW for the context, and QNX Screen or DRM/KMS for the display.
- On QNX the display side would use QNX Screen to get a window or a hardware overlay plane, and
  the camera side would come from the camera framework instead of V4L2/OpenCV.
