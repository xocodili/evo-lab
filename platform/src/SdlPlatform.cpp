#include "platform/SdlPlatform.hpp"

#include <iostream>

#define SDL_MAIN_HANDLED
#include <SDL.h>

namespace evolab::platform {

SdlPlatform::SdlPlatform() = default;

SdlPlatform::~SdlPlatform() { shutdown(); }

bool SdlPlatform::init(int width, int height, const char* title) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
    return false;
  }
  sdlInitialized_ = true;

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

  window_ = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!window_) {
    std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
    return false;
  }

  glContext_ = SDL_GL_CreateContext(window_);
  if (!glContext_) {
    std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << '\n';
    return false;
  }

  if (SDL_GL_MakeCurrent(window_, glContext_) != 0) {
    std::cerr << "SDL_GL_MakeCurrent failed: " << SDL_GetError() << '\n';
    return false;
  }

  // Immediate mode avoids blocking the UI thread on a first-frame vsync stall.
  SDL_GL_SetSwapInterval(0);
  return true;
}

void SdlPlatform::shutdown() {
  if (glContext_) {
    SDL_GL_DeleteContext(glContext_);
    glContext_ = nullptr;
  }
  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  if (sdlInitialized_) {
    SDL_Quit();
    sdlInitialized_ = false;
  }
  shouldClose_ = false;
  pendingEvents_.clear();
}

void SdlPlatform::handleEvent(const SDL_Event& event, InputFrame& input, bool mouseLeftHeld) {
  switch (event.type) {
    case SDL_QUIT:
      shouldClose_ = true;
      input.quit = true;
      break;
    case SDL_KEYDOWN:
      if (event.key.repeat) {
        break;
      }
      if (event.key.keysym.sym == SDLK_ESCAPE) {
        shouldClose_ = true;
        input.quit = true;
      } else if (event.key.keysym.sym == SDLK_r) {
        input.keyR = true;
      } else if (event.key.keysym.sym == SDLK_SPACE) {
        input.keySpace = true;
      }
      break;
    case SDL_MOUSEWHEEL:
      input.scrollDelta += event.wheel.y;
      break;
    case SDL_MOUSEBUTTONDOWN:
      if (event.button.button == SDL_BUTTON_LEFT) {
        input.mouseLeftDown = true;
      }
      break;
    case SDL_MOUSEBUTTONUP:
      if (event.button.button == SDL_BUTTON_LEFT) {
        input.mouseLeftDown = false;
      }
      break;
    case SDL_MOUSEMOTION:
      if (mouseLeftHeld) {
        input.mouseDeltaX += event.motion.xrel;
        input.mouseDeltaY += event.motion.yrel;
      }
      break;
    default:
      break;
  }
}

void SdlPlatform::pumpEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      shouldClose_ = true;
    } else {
      pendingEvents_.push_back(event);
    }
  }
  SDL_Delay(0);
}

void SdlPlatform::poll(InputFrame& input, bool mouseLeftHeld) {
  input.quit = false;
  input.keyR = false;
  input.keySpace = false;
  input.scrollDelta = 0;
  input.moveForward = 0.0f;
  input.moveRight = 0.0f;

  for (const SDL_Event& event : pendingEvents_) {
    handleEvent(event, input, mouseLeftHeld);
  }
  pendingEvents_.clear();

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    handleEvent(event, input, mouseLeftHeld);
  }

  SDL_PumpEvents();
  const Uint8* keys = SDL_GetKeyboardState(nullptr);
  if (keys != nullptr) {
    if (keys[SDL_SCANCODE_W]) {
      input.moveForward += 1.0f;
    }
    if (keys[SDL_SCANCODE_S]) {
      input.moveForward -= 1.0f;
    }
    if (keys[SDL_SCANCODE_D]) {
      input.moveRight += 1.0f;
    }
    if (keys[SDL_SCANCODE_A]) {
      input.moveRight -= 1.0f;
    }
  }

  int mx = 0;
  int my = 0;
  SDL_GetMouseState(&mx, &my);
  input.mouseX = mx;
  input.mouseY = my;
}

void SdlPlatform::swap() {
  if (window_) {
    SDL_GL_SwapWindow(window_);
  }
}

void SdlPlatform::windowSize(int& width, int& height) const {
  if (window_) {
    SDL_GetWindowSize(window_, &width, &height);
  } else {
    width = 0;
    height = 0;
  }
}

std::string SdlPlatform::basePath() const {
  char* base = SDL_GetBasePath();
  if (!base) {
    return {};
  }
  std::string path(base);
  SDL_free(base);
  return path;
}

}  // namespace evolab::platform
