#pragma once

#include "engine/gfx/sprites/SpriteTypes.hpp"

namespace evolab::engine::gfx::sprites {

class SpriteAnimator {
public:
  void setClip(const SpriteClip* clip);
  void setTime(float timeSec);
  void update(float dtSec);

  const SpriteFrame* currentFrame() const;
  int frameIndex() const { return frameIndex_; }
  bool finished() const;
  void reset();

private:
  void recomputeFrameFromTime();

  const SpriteClip* clip_ = nullptr;
  float localTime_ = 0.0f;
  int frameIndex_ = 0;
  int pingPongDir_ = 1;
};

float frameDurationSec(const SpriteClip& clip, int frameIndex);

}  // namespace evolab::engine::gfx::sprites
