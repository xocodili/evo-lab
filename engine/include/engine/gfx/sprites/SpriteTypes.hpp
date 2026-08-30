#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace evolab::engine::gfx::sprites {

struct SpriteFrame {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  float durationSec = 0.0f;
};

enum class SpriteLoopMode { Once, Loop, PingPong, HoldLast };

struct SpriteClip {
  std::string name;
  std::vector<SpriteFrame> frames;
  SpriteLoopMode loop = SpriteLoopMode::Loop;
  float defaultFps = 12.0f;
};

struct SpriteUvRect {
  float u0 = 0.0f;
  float v0 = 0.0f;
  float u1 = 1.0f;
  float v1 = 1.0f;
};

struct SpriteDrawInstance {
  float worldX = 0.0f;
  float worldY = 0.0f;
  float worldZ = 0.0f;
  float halfSizeWorld = 0.05f;
  float tintR = 1.0f;
  float tintG = 1.0f;
  float tintB = 1.0f;
  float tintA = 1.0f;
  const SpriteClip* clip = nullptr;
  float animTimeSec = 0.0f;
  int flipX = 0;
};

}  // namespace evolab::engine::gfx::sprites
