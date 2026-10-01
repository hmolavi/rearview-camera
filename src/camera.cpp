#include "camera.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

FrameSource::FrameSource(int device, const std::string& videoPath, int width, int height,
                         const CameraModel& model)
    : width_(width), height_(height), model_(model) {
  if (!videoPath.empty()) {
    if (cap_.open(videoPath)) kind_ = Kind::VideoFile;
    else std::fprintf(stderr, "could not open video %s, using test pattern\n", videoPath.c_str());
  } else if (device >= 0) {
    if (cap_.open(device)) {
      cap_.set(cv::CAP_PROP_FRAME_WIDTH, width);
      cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height);
      kind_ = Kind::Webcam;
    } else {
      std::fprintf(stderr,
                   "could not open camera %d (on macOS, allow camera access for your terminal in "
                   "System Settings > Privacy & Security > Camera). Using test pattern.\n",
                   device);
    }
  }
  worker_ = std::thread(&FrameSource::run, this);
}

FrameSource::~FrameSource() {
  running_ = false;
  if (worker_.joinable()) worker_.join();
}

bool FrameSource::latest(Frame& out, uint64_t lastSequence) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (shared_.sequence == lastSequence || shared_.bgr.empty()) return false;
  // Copy under the lock so the capture thread can immediately start writing the next frame.
  shared_.bgr.copyTo(out.bgr);
  out.sequence = shared_.sequence;
  out.captured = shared_.captured;
  return true;
}

void FrameSource::run() {
  cv::Mat frame;
  uint64_t sequence = 0;
  auto windowStart = Clock::now();
  int framesInWindow = 0;

  while (running_) {
    if (!grab(frame)) break;
    const auto now = Clock::now();

    {
      std::lock_guard<std::mutex> lock(mutex_);
      // swap, not copy: the old buffer comes back to us for reuse on the next grab.
      cv::swap(shared_.bgr, frame);
      shared_.sequence = ++sequence;
      shared_.captured = now;
    }

    ++framesInWindow;
    const double elapsed = std::chrono::duration<double>(now - windowStart).count();
    if (elapsed >= 1.0) {
      captureFps_ = framesInWindow / elapsed;
      framesInWindow = 0;
      windowStart = now;
    }
  }
}

bool FrameSource::grab(cv::Mat& out) {
  switch (kind_) {
    case Kind::Webcam:
      return cap_.read(out) && !out.empty();

    case Kind::VideoFile: {
      if (!cap_.read(out) || out.empty()) {
        cap_.set(cv::CAP_PROP_POS_FRAMES, 0);  // loop the clip
        if (!cap_.read(out) || out.empty()) return false;
      }
      // Pace file playback to its native rate; the file would otherwise decode as fast as possible.
      double fps = cap_.get(cv::CAP_PROP_FPS);
      if (fps <= 1.0 || fps > 240.0) fps = 30.0;
      std::this_thread::sleep_for(std::chrono::duration<double>(1.0 / fps));
      return true;
    }

    case Kind::TestPattern:
      drawTestPattern(out);
      std::this_thread::sleep_for(std::chrono::milliseconds(33));  // ~30 fps, like a real camera
      return true;
  }
  return false;
}

// A synthetic parking stall, rendered through the same CameraModel as the overlay, so the app is
// demoable with no camera attached and the guidelines visibly line up with the painted lines.
void FrameSource::drawTestPattern(cv::Mat& out) {
  out.create(height_, width_, CV_8UC3);
  const glm::mat4 vp = model_.viewProjection(static_cast<float>(width_) / height_);

  // Ground point (x, z) in metres -> pixel. Returns false if it is behind the camera.
  auto toPixel = [&](float x, float z, cv::Point& px) {
    const glm::vec4 clip = vp * glm::vec4(x, 0.0f, z, 1.0f);
    if (clip.w <= 0.01f) return false;
    const float nx = clip.x / clip.w, ny = clip.y / clip.w;
    px = cv::Point(static_cast<int>((nx + 1.0f) * 0.5f * width_),
                   static_cast<int>((1.0f - ny) * 0.5f * height_));
    return true;
  };
  auto groundLine = [&](float x0, float z0, float x1, float z1, const cv::Scalar& color, int thick) {
    cv::Point a, b;
    if (toPixel(x0, z0, a) && toPixel(x1, z1, b)) cv::line(out, a, b, color, thick, cv::LINE_AA);
  };

  // Asphalt, with sky above the horizon if the horizon is in frame.
  out.setTo(cv::Scalar(70, 70, 72));
  cv::Point horizon;
  if (toPixel(0.0f, 1000.0f, horizon) && horizon.y > 0) {
    cv::rectangle(out, cv::Rect(0, 0, width_, std::min(horizon.y, height_)), cv::Scalar(190, 150, 100),
                  cv::FILLED);
  }

  // Stalls 2.6 m wide and 5.5 m deep, the car backing straight into the middle one.
  const cv::Scalar paint(235, 235, 235);
  for (float x : {-3.9f, -1.3f, 1.3f, 3.9f}) groundLine(x, 0.2f, x, 5.5f, paint, 5);
  groundLine(-3.9f, 5.5f, 3.9f, 5.5f, paint, 5);
  // A curb behind the stalls.
  groundLine(-8.0f, 6.5f, 8.0f, 6.5f, cv::Scalar(120, 180, 200), 9);

  // A moving bar so it is obvious the feed is live.
  const int x = static_cast<int>((patternTick_++ * 6) % static_cast<uint64_t>(width_));
  cv::rectangle(out, cv::Rect(x, 10, 40, 10), cv::Scalar(0, 200, 255), cv::FILLED);
  cv::putText(out, "TEST PATTERN", cv::Point(20, 40), cv::FONT_HERSHEY_SIMPLEX, 0.8,
              cv::Scalar(255, 255, 255), 2, cv::LINE_AA);
}
