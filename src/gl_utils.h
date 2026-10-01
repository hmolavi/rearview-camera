#pragma once

#include "gl.h"

#include <string>

// Small RAII wrappers so every GL object is released exactly once.
// Copy is deleted (two owners of one GL name would double-delete); move transfers ownership.

class ShaderProgram {
 public:
  ShaderProgram() = default;
  ShaderProgram(const std::string& vertexPath, const std::string& fragmentPath);
  ~ShaderProgram();

  ShaderProgram(const ShaderProgram&) = delete;
  ShaderProgram& operator=(const ShaderProgram&) = delete;
  ShaderProgram(ShaderProgram&& other) noexcept;
  ShaderProgram& operator=(ShaderProgram&& other) noexcept;

  void use() const { glUseProgram(id_); }
  GLint uniform(const char* name) const { return glGetUniformLocation(id_, name); }
  GLuint id() const { return id_; }

 private:
  GLuint id_ = 0;
};

// A 2D texture that holds the latest camera frame. Reallocated only when the frame size changes;
// every other frame is a glTexSubImage2D into the existing storage.
class VideoTexture {
 public:
  VideoTexture();
  ~VideoTexture();

  VideoTexture(const VideoTexture&) = delete;
  VideoTexture& operator=(const VideoTexture&) = delete;

  // Uploads tightly packed 8-bit BGR pixels (OpenCV's native layout).
  void upload(const unsigned char* bgr, int width, int height);
  void bind(GLuint unit) const;

  int width() const { return width_; }
  int height() const { return height_; }

 private:
  GLuint id_ = 0;
  int width_ = 0;
  int height_ = 0;
};

std::string readFile(const std::string& path);
void checkGlError(const char* where);
