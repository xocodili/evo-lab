#pragma once

namespace evolab::platform {

struct InputFrame {
  bool quit = false;
  bool keyR = false;
  bool keySpace = false;
  bool keyV = false;
  int scrollDelta = 0;
  bool mouseLeftDown = false;
  float moveForward = 0.0f;
  float moveRight = 0.0f;
  int mouseX = 0;
  int mouseY = 0;
  int mouseDeltaX = 0;
  int mouseDeltaY = 0;
};

}  // namespace evolab::platform
