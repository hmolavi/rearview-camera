#pragma once

#include "camera_model.h"

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

using Clock = std::chrono::steady_clock;

struct Frame {
  cv::Mat bgr;               // 8-bit, 3 channels, tightly packed
  uint64_t sequence = 0;     // increments once per captured frame
  Clock::time_point captured;
};

// Captures frames on a background thread and keeps only the newest one ("latest frame mailbox").
//
// Why a thread: VideoCapture::read() blocks until the sensor delivers a frame (~33 ms at 30 fps).
// Doing that on the render thread would tie the display rate to the camera and add a full frame of
// latency. Why keep only the newest: a rear camera must show *now*; queueing old frames only adds lag.
class FrameSource {
 public:
  enum class Kind { Webcam, VideoFile, TestPattern };

  // device >= 0 opens that webcam; a non-empty path opens a video file; otherwise a synthetic pattern.
  // `model` is only used to render the test pattern, so its ground lines sit where the overlay
  // expects the ground to be.
  FrameSource(int device, const std::string& videoPath, int width, int height,
              const CameraModel& model);
  ~FrameSource();

  FrameSource(const FrameSource&) = delete;
  FrameSource& operator=(const FrameSource&) = delete;

  // Copies the newest frame into `out` if it is newer than `lastSequence`. Returns true if it did.
  bool latest(Frame& out, uint64_t lastSequence);

  Kind kind() const { return kind_; }
  double captureFps() const { return captureFps_.load(); }

 private:
  void run();
  bool grab(cv::Mat& out);
  void drawTestPattern(cv::Mat& out);

  Kind kind_ = Kind::TestPattern;
  cv::VideoCapture cap_;
  int width_;
  int height_;
  CameraModel model_;
  uint64_t patternTick_ = 0;

  std::mutex mutex_;
  Frame shared_;  // guarded by mutex_

  std::atomic<bool> running_{true};
  std::atomic<double> captureFps_{0.0};
  std::thread worker_;
};
