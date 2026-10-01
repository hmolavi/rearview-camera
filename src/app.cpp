#include "app.h"

#include <glm/gtc/type_ptr.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {

std::string shaderPath(const char* name) { return std::string(SHADER_DIR) + "/" + name; }

const char* sourceName(FrameSource::Kind kind) {
  switch (kind) {
    case FrameSource::Kind::Webcam: return "camera";
    case FrameSource::Kind::VideoFile: return "video";
    case FrameSource::Kind::TestPattern: return "test pattern";
  }
  return "?";
}

}  // namespace

App::App(const Options& options) : options_(options), steerDeg_(options.steerDeg) {
  if (!glfwInit()) throw std::runtime_error("glfwInit failed");

  // Ask for a 3.3 core profile. On macOS, forward-compat core is the only way to get past GL 2.1,
  // and the driver hands back 4.1, the newest version Apple ships.
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  if (!options_.screenshotPath.empty()) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

  window_ = glfwCreateWindow(options_.width, options_.height, "Rear-View Camera", nullptr, nullptr);
  if (!window_) {
    glfwTerminate();
    throw std::runtime_error("could not create an OpenGL 3.3 core window");
  }
  glfwMakeContextCurrent(window_);
  glfwSwapInterval(1);  // vsync: present once per display refresh, no tearing

  glfwSetWindowUserPointer(window_, this);
  glfwSetKeyCallback(window_, [](GLFWwindow* w, int key, int, int action, int) {
    static_cast<App*>(glfwGetWindowUserPointer(w))->onKey(key, action);
  });

  std::printf("OpenGL %s | GLSL %s | %s\n", glGetString(GL_VERSION),
              glGetString(GL_SHADING_LANGUAGE_VERSION), glGetString(GL_RENDERER));

  videoShader_ = std::make_unique<ShaderProgram>(shaderPath("video.vert"), shaderPath("video.frag"));
  guideShader_ = std::make_unique<ShaderProgram>(shaderPath("guides.vert"), shaderPath("guides.frag"));
  texture_ = std::make_unique<VideoTexture>();

  glGenVertexArrays(1, &emptyVao_);

  // Guideline geometry: one VBO of interleaved {vec3 position, vec4 color}, described by one VAO.
  glGenVertexArrays(1, &guideVao_);
  glGenBuffers(1, &guideVbo_);
  glBindVertexArray(guideVao_);
  glBindBuffer(GL_ARRAY_BUFFER, guideVbo_);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GuideVertex),
                        reinterpret_cast<void*>(offsetof(GuideVertex, position)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GuideVertex),
                        reinterpret_cast<void*>(offsetof(GuideVertex, color)));
  glBindVertexArray(0);
  checkGlError("setup");

  if (options_.fovDeg > 0.0f) cameraModel_.horizontalFovDeg = options_.fovDeg;
  const int device = options_.cameraIndex;
  source_ = std::make_unique<FrameSource>(device, options_.videoPath, options_.width,
                                          options_.height, cameraModel_);
  std::printf("source: %s\n", sourceName(source_->kind()));
}

App::~App() {
  source_.reset();  // stop the capture thread before GL teardown
  // GL objects must be deleted while the context is still current.
  videoShader_.reset();
  guideShader_.reset();
  texture_.reset();
  if (guideVbo_) glDeleteBuffers(1, &guideVbo_);
  if (guideVao_) glDeleteVertexArrays(1, &guideVao_);
  if (emptyVao_) glDeleteVertexArrays(1, &emptyVao_);
  if (window_) glfwDestroyWindow(window_);
  glfwTerminate();
}

int App::run() {
  auto last = Clock::now();
  auto statsStart = last;
  int framesInWindow = 0;
  int framesWithVideo = 0;

  while (!glfwWindowShouldClose(window_)) {
    glfwPollEvents();
    const auto now = Clock::now();
    const float dt = std::chrono::duration<float>(now - last).count();
    last = now;
    handleContinuousInput(dt);

    // Only touch the GPU texture when the camera actually produced a new frame. The render loop
    // runs at display rate (60 Hz) and the camera at ~30 Hz; re-uploading duplicates is waste.
    if (source_->latest(frame_, frame_.sequence)) {
      texture_->upload(frame_.bgr.data, frame_.bgr.cols, frame_.bgr.rows);
    }

    const auto cpuStart = Clock::now();
    render();
    frameMs_ = std::chrono::duration<double, std::milli>(Clock::now() - cpuStart).count();

    if (texture_->width() > 0) ++framesWithVideo;
    if (!options_.screenshotPath.empty() && framesWithVideo == 10) {
      saveScreenshot(options_.screenshotPath);
      return 0;
    }
    if (screenshotRequested_) {
      saveScreenshot("rearcam_screenshot.png");
      screenshotRequested_ = false;
    }

    glfwSwapBuffers(window_);
    // Capture-to-present: how old the pixels are when they reach the screen.
    if (frame_.sequence) {
      latencyMs_ = std::chrono::duration<double, std::milli>(Clock::now() - frame_.captured).count();
    }

    ++framesInWindow;
    const double elapsed = std::chrono::duration<double>(Clock::now() - statsStart).count();
    if (elapsed >= 0.5) {
      renderFps_ = framesInWindow / elapsed;
      framesInWindow = 0;
      statsStart = Clock::now();
      updateTitle();
    }
  }
  return 0;
}

void App::handleContinuousInput(float dt) {
  // Held arrow keys turn the "steering wheel" at a fixed rate, like a real driver input.
  const float rateDegPerSec = 40.0f;
  // With the mirrored view, Right should bend the lines right on screen; un-mirrored it inverts.
  const float dir = mirror_ ? 1.0f : -1.0f;
  if (glfwGetKey(window_, GLFW_KEY_RIGHT) == GLFW_PRESS) steerDeg_ += dir * rateDegPerSec * dt;
  if (glfwGetKey(window_, GLFW_KEY_LEFT) == GLFW_PRESS) steerDeg_ -= dir * rateDegPerSec * dt;
  if (glfwGetKey(window_, GLFW_KEY_UP) == GLFW_PRESS) cameraModel_.pitchDeg -= 15.0f * dt;
  if (glfwGetKey(window_, GLFW_KEY_DOWN) == GLFW_PRESS) cameraModel_.pitchDeg += 15.0f * dt;
  steerDeg_ = std::clamp(steerDeg_, -vehicle_.maxSteerDeg, vehicle_.maxSteerDeg);
  cameraModel_.pitchDeg = std::clamp(cameraModel_.pitchDeg, 5.0f, 60.0f);
}

void App::onKey(int key, int action) {
  if (action != GLFW_PRESS) return;
  switch (key) {
    case GLFW_KEY_ESCAPE:
    case GLFW_KEY_Q: glfwSetWindowShouldClose(window_, GLFW_TRUE); break;
    case GLFW_KEY_C: steerDeg_ = 0.0f; break;
    case GLFW_KEY_M: mirror_ = !mirror_; break;
    case GLFW_KEY_G: showGuides_ = !showGuides_; break;
    case GLFW_KEY_S: screenshotRequested_ = true; break;
    default: break;
  }
}

void App::render() {
  int fbw = 0, fbh = 0;
  // Framebuffer size, not window size: on a Retina display the framebuffer has 2x the pixels.
  glfwGetFramebufferSize(window_, &fbw, &fbh);
  glViewport(0, 0, fbw, fbh);
  glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  if (texture_->width() == 0 || fbw == 0 || fbh == 0) return;  // no frame yet, or minimized

  // Letterbox: fit the camera image inside the window without stretching it.
  const float imageAspect = static_cast<float>(texture_->width()) / texture_->height();
  const float windowAspect = static_cast<float>(fbw) / fbh;
  const glm::vec2 fit = windowAspect > imageAspect ? glm::vec2(imageAspect / windowAspect, 1.0f)
                                                   : glm::vec2(1.0f, windowAspect / imageAspect);
  const float mirror = mirror_ ? -1.0f : 1.0f;

  // 1) The camera image.
  videoShader_->use();
  glUniform2f(videoShader_->uniform("uFit"), fit.x, fit.y);
  glUniform1f(videoShader_->uniform("uMirror"), mirror);
  glUniform1i(videoShader_->uniform("uFrame"), 0);
  texture_->bind(0);
  glBindVertexArray(emptyVao_);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  if (!showGuides_) return;

  // 2) The guidelines, rebuilt only when the steering angle actually changed.
  if (std::abs(steerDeg_ - builtSteerDeg_) > 0.05f) {
    const std::vector<GuideVertex> vertices = buildGuidelines(steerDeg_, vehicle_, style_);
    glBindBuffer(GL_ARRAY_BUFFER, guideVbo_);
    // GL_DYNAMIC_DRAW: the driver should expect us to rewrite this buffer often.
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(GuideVertex)),
                 vertices.data(), GL_DYNAMIC_DRAW);
    guideVertexCount_ = static_cast<GLsizei>(vertices.size());
    builtSteerDeg_ = steerDeg_;
  }

  // View (camera pose) and projection (lens) in one matrix; see camera_model.h.
  const glm::mat4 viewProj = cameraModel_.viewProjection(imageAspect);

  guideShader_->use();
  glUniformMatrix4fv(guideShader_->uniform("uViewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
  glUniform2f(guideShader_->uniform("uFit"), fit.x, fit.y);
  glUniform1f(guideShader_->uniform("uMirror"), mirror);

  // Alpha blending so the lines are slightly see-through over the video.
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindVertexArray(guideVao_);
  glDrawArrays(GL_TRIANGLES, 0, guideVertexCount_);
  glDisable(GL_BLEND);
  glBindVertexArray(0);
}

void App::updateTitle() {
  char title[256];
  std::snprintf(title, sizeof(title),
                "Rear-View Camera | %s %.0f fps | render %.0f fps | cpu %.2f ms | latency %.0f ms | "
                "steer %+.1f deg | pitch %.0f deg",
                sourceName(source_->kind()), source_->captureFps(), renderFps_, frameMs_, latencyMs_,
                steerDeg_, cameraModel_.pitchDeg);
  glfwSetWindowTitle(window_, title);
}

void App::saveScreenshot(const std::string& path) {
  int fbw = 0, fbh = 0;
  glfwGetFramebufferSize(window_, &fbw, &fbh);
  cv::Mat rgba(fbh, fbw, CV_8UC4);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  // Read the back buffer we just drew into, before it is swapped to the screen.
  glReadPixels(0, 0, fbw, fbh, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data);
  cv::Mat bgr;
  cv::cvtColor(rgba, bgr, cv::COLOR_RGBA2BGR);
  cv::flip(bgr, bgr, 0);  // GL's origin is bottom-left, image files are top-left
  cv::imwrite(path, bgr);
  std::printf("saved %s (%dx%d)\n", path.c_str(), fbw, fbh);
}
