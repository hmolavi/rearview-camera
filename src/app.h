#pragma once

#include "camera.h"
#include "camera_model.h"
#include "gl_utils.h"
#include "guidelines.h"

#include <memory>
#include <string>

struct Options {
  int cameraIndex = 0;     // -1 forces the test pattern
  std::string videoPath;   // play a file instead of a live camera
  int width = 1280;
  int height = 720;
  float steerDeg = 0.0f;
  float fovDeg = 0.0f;     // horizontal lens FOV override; 0 keeps the CameraModel default
  std::string screenshotPath;  // render a few frames, save a PNG, and exit
};

class App {
 public:
  explicit App(const Options& options);
  ~App();

  App(const App&) = delete;
  App& operator=(const App&) = delete;

  int run();

 private:
  void handleContinuousInput(float dt);
  void onKey(int key, int action);
  void render();
  void updateTitle();
  void saveScreenshot(const std::string& path);

  Options options_;
  GLFWwindow* window_ = nullptr;

  std::unique_ptr<FrameSource> source_;
  std::unique_ptr<VideoTexture> texture_;
  std::unique_ptr<ShaderProgram> videoShader_;
  std::unique_ptr<ShaderProgram> guideShader_;

  GLuint emptyVao_ = 0;  // core profile needs a bound VAO even for attribute-less draws
  GLuint guideVao_ = 0;
  GLuint guideVbo_ = 0;
  GLsizei guideVertexCount_ = 0;

  Vehicle vehicle_;
  GuidelineStyle style_;
  CameraModel cameraModel_;
  float steerDeg_ = 0.0f;
  float builtSteerDeg_ = 1e9f;  // forces the first build
  bool mirror_ = true;
  bool showGuides_ = true;
  bool screenshotRequested_ = false;

  Frame frame_;
  // Stats for the title bar.
  double renderFps_ = 0.0;
  double frameMs_ = 0.0;
  double latencyMs_ = 0.0;
};
