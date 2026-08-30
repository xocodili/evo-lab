#pragma once

#include "platform/InputFrame.hpp"

#include <SDL.h>

#include <string>
#include <vector>

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
  void pollMovementKeys(InputFrame& input);
  void pumpEvents();
  void swap();
  void windowSize(int& width, int& height) const;

  bool shouldClose() const { return shouldClose_; }
  std::string basePath() const;

private:
  void handleEvent(const SDL_Event& event, InputFrame& input, bool mouseLeftHeld);

  SDL_Window* window_ = nullptr;
  void* glContext_ = nullptr;
  bool sdlInitialized_ = false;
  bool shouldClose_ = false;
  bool keyWHeld_ = false;
  bool keyAHeld_ = false;
  bool keySHeld_ = false;
  bool keyDHeld_ = false;
  std::vector<SDL_Event> pendingEvents_;
};

}  // namespace evolab::platform
