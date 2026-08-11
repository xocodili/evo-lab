#pragma once

namespace evolab::engine {

class FixedTimestepClock {
public:
  explicit FixedTimestepClock(float fixedHz = 60.0f);

  void setFixedHz(float fixedHz);
  float fixedHz() const { return fixedHz_; }

  int advance(float realDeltaSeconds);

private:
  float fixedHz_ = 60.0f;
  float fixedDt_ = 1.0f / 60.0f;
  float accumulator_ = 0.0f;
};

}  // namespace evolab::engine
