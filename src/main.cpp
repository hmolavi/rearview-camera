#include "app.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

namespace {

void usage(const char* argv0) {
  std::printf(
      "usage: %s [options]\n"
      "  --camera N        webcam index (default 0)\n"
      "  --test            synthetic test pattern instead of a camera\n"
      "  --video PATH      play a video file (loops)\n"
      "  --size WxH        requested capture and window size (default 1280x720)\n"
      "  --steer DEG       initial steering angle\n"
      "  --fov DEG         horizontal field of view of the camera lens (default 100)\n"
      "  --screenshot PNG  render a few frames, save them to PNG, and exit\n"
      "\n"
      "keys: Left/Right steer, C center, Up/Down camera pitch, M mirror, G guidelines,\n"
      "      S screenshot, Q/Esc quit\n",
      argv0);
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto next = [&]() -> const char* {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "%s needs a value\n", arg.c_str());
        std::exit(2);
      }
      return argv[++i];
    };
    if (arg == "--camera") options.cameraIndex = std::atoi(next());
    else if (arg == "--test") options.cameraIndex = -1;
    else if (arg == "--video") options.videoPath = next();
    else if (arg == "--steer") options.steerDeg = static_cast<float>(std::atof(next()));
    else if (arg == "--fov") options.fovDeg = static_cast<float>(std::atof(next()));
    else if (arg == "--screenshot") options.screenshotPath = next();
    else if (arg == "--size") {
      if (std::sscanf(next(), "%dx%d", &options.width, &options.height) != 2) {
        std::fprintf(stderr, "--size expects WxH, e.g. 1280x720\n");
        return 2;
      }
    } else if (arg == "-h" || arg == "--help") {
      usage(argv[0]);
      return 0;
    } else {
      std::fprintf(stderr, "unknown option %s\n", arg.c_str());
      usage(argv[0]);
      return 2;
    }
  }

  try {
    App app(options);
    return app.run();
  } catch (const std::exception& e) {
    std::fprintf(stderr, "fatal: %s\n", e.what());
    return 1;
  }
}
