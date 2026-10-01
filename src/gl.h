#pragma once

// One place to pull in OpenGL function declarations for each platform.
//   macOS: the system framework exposes the 4.1 core profile directly, no loader needed.
//   Linux (Mesa): core-profile prototypes, resolved by libGL at link time.
// An OpenGL ES 3.0 port (what embedded/automotive GPUs run) would swap this header and the
// `#version 330 core` lines in shaders/ for `#version 300 es`; everything else is common.
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
