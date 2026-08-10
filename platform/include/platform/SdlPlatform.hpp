#pragma once

#include "platform/InputFrame.hpp"

#include <string>

struct SDL_Window;

namespace evolab::platform {

class SdlPlatform {
public:
  SdlPlatform();
  ~SdlPlatform();

  SdlPlatform(const SdlPlatform&) = delete;
  SdlPlatform& operator=(const SdlPlatform&) = delete;

  bool init(int width, int height, const char* title);
  void shutdown();

  void poll(InputFrame& input, bool mouseLeftHeld);
  void swap();
  void windowSize(int& width, int& height) const;

  bool shouldClose() const { return shouldClose_; }
  std::string basePath() const;

private:
  SDL_Window* window_ = nullptr;
  void* glContext_ = nullptr;
  bool sdlInitialized_ = false;
  bool shouldClose_ = false;
};

}  // namespace evolab::platform
