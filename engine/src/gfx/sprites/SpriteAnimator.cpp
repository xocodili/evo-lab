#include "engine/gfx/sprites/SpriteAnimator.hpp"

#include <algorithm>
#include <cmath>

namespace evolab::engine::gfx::sprites {

namespace {

float clipDurationSec(const SpriteClip& clip) {
  float total = 0.0f;
  for (int i = 0; i < static_cast<int>(clip.frames.size()); ++i) {
    total += frameDurationSec(clip, i);
  }
  return total;
}

}  // namespace

float frameDurationSec(const SpriteClip& clip, int frameIndex) {
  if (clip.frames.empty()) {
    return 0.0f;
  }
  const int idx = std::clamp(frameIndex, 0, static_cast<int>(clip.frames.size()) - 1);
  const SpriteFrame& frame = clip.frames[static_cast<std::size_t>(idx)];
  if (frame.durationSec > 0.0f) {
    return frame.durationSec;
  }
  const float fps = clip.defaultFps > 0.0f ? clip.defaultFps : 12.0f;
  return 1.0f / fps;
}

void SpriteAnimator::setClip(const SpriteClip* clip) {
  if (clip_ == clip) {
    return;
  }
  clip_ = clip;
  reset();
}

void SpriteAnimator::setTime(float timeSec) {
  localTime_ = std::max(0.0f, timeSec);
  recomputeFrameFromTime();
}

void SpriteAnimator::update(float dtSec) {
  if (clip_ == nullptr || clip_->frames.empty() || dtSec <= 0.0f) {
    return;
  }
  localTime_ += dtSec;
  recomputeFrameFromTime();
}

void SpriteAnimator::recomputeFrameFromTime() {
  frameIndex_ = 0;
  pingPongDir_ = 1;
  if (clip_ == nullptr || clip_->frames.empty()) {
    return;
  }

  const int lastIndex = static_cast<int>(clip_->frames.size()) - 1;
  const float totalDuration = clipDurationSec(*clip_);
  float t = localTime_;

  if (clip_->loop == SpriteLoopMode::Once || clip_->loop == SpriteLoopMode::HoldLast) {
    if (totalDuration > 0.0f && t >= totalDuration) {
      frameIndex_ = lastIndex;
      return;
    }
  } else if (clip_->loop == SpriteLoopMode::Loop && totalDuration > 0.0f) {
    t = std::fmod(t, totalDuration);
  }

  for (int i = 0; i < lastIndex; ++i) {
    const float duration = frameDurationSec(*clip_, i);
    if (duration <= 0.0f || t < duration) {
      frameIndex_ = i;
      return;
    }
    t -= duration;
  }
  frameIndex_ = lastIndex;
}

const SpriteFrame* SpriteAnimator::currentFrame() const {
  if (clip_ == nullptr || clip_->frames.empty()) {
    return nullptr;
  }
  return &clip_->frames[static_cast<std::size_t>(frameIndex_)];
}

bool SpriteAnimator::finished() const {
  if (clip_ == nullptr || clip_->loop != SpriteLoopMode::Once || clip_->frames.empty()) {
    return false;
  }
  return frameIndex_ >= static_cast<int>(clip_->frames.size()) - 1 &&
         localTime_ >= clipDurationSec(*clip_) - 1.0e-4f;
}

void SpriteAnimator::reset() {
  localTime_ = 0.0f;
  frameIndex_ = 0;
  pingPongDir_ = 1;
}

}  // namespace evolab::engine::gfx::sprites
