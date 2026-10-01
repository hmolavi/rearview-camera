#include "gl_utils.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

std::string readFile(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open " + path);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

void checkGlError(const char* where) {
  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    throw std::runtime_error(std::string("OpenGL error 0x") + std::to_string(err) + " at " + where);
  }
}

namespace {

GLuint compile(GLenum type, const std::string& path) {
  const std::string source = readFile(path);
  const char* src = source.c_str();

  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);

  GLint ok = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    GLint len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    std::vector<char> log(static_cast<size_t>(len) + 1);
    glGetShaderInfoLog(shader, len, nullptr, log.data());
    glDeleteShader(shader);
    throw std::runtime_error("shader compile failed (" + path + "):\n" + log.data());
  }
  return shader;
}

}  // namespace

ShaderProgram::ShaderProgram(const std::string& vertexPath, const std::string& fragmentPath) {
  GLuint vs = compile(GL_VERTEX_SHADER, vertexPath);
  GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentPath);

  id_ = glCreateProgram();
  glAttachShader(id_, vs);
  glAttachShader(id_, fs);
  glLinkProgram(id_);

  // Once linked, the program keeps its own copy; the stage objects can go.
  glDeleteShader(vs);
  glDeleteShader(fs);

  GLint ok = GL_FALSE;
  glGetProgramiv(id_, GL_LINK_STATUS, &ok);
  if (!ok) {
    GLint len = 0;
    glGetProgramiv(id_, GL_INFO_LOG_LENGTH, &len);
    std::vector<char> log(static_cast<size_t>(len) + 1);
    glGetProgramInfoLog(id_, len, nullptr, log.data());
    glDeleteProgram(id_);
    id_ = 0;
    throw std::runtime_error(std::string("program link failed:\n") + log.data());
  }
}

ShaderProgram::~ShaderProgram() {
  if (id_) glDeleteProgram(id_);
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept : id_(std::exchange(other.id_, 0)) {}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
  if (this != &other) {
    if (id_) glDeleteProgram(id_);
    id_ = std::exchange(other.id_, 0);
  }
  return *this;
}

VideoTexture::VideoTexture() {
  glGenTextures(1, &id_);
  glBindTexture(GL_TEXTURE_2D, id_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

VideoTexture::~VideoTexture() {
  if (id_) glDeleteTextures(1, &id_);
}

void VideoTexture::upload(const unsigned char* bgr, int width, int height) {
  glBindTexture(GL_TEXTURE_2D, id_);
  // OpenCV rows are tightly packed; a 3-byte pixel row is not 4-byte aligned in general.
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  if (width != width_ || height != height_) {
    // (Re)allocate storage. The BGR -> RGB swizzle happens in the fragment shader, which keeps
    // this path identical on desktop GL and GLES (GLES has no GL_BGR upload format).
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, bgr);
    width_ = width;
    height_ = height;
  } else {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, bgr);
  }
}

void VideoTexture::bind(GLuint unit) const {
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, id_);
}
