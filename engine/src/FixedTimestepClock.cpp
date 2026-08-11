#include "engine/FixedTimestepClock.hpp"

#include <algorithm>
#include <cmath>

namespace evolab::engine {

FixedTimestepClock::FixedTimestepClock(float fixedHz) {
  setFixedHz(fixedHz);
}

void FixedTimestepClock::setFixedHz(float fixedHz) {
  fixedHz_ = std::max(1.0f, fixedHz);
  fixedDt_ = 1.0f / fixedHz_;
}

int FixedTimestepClock::advance(float realDeltaSeconds) {
  if (realDeltaSeconds <= 0.0f) {
    return 0;
  }

  accumulator_ += std::min(realDeltaSeconds, fixedDt_ * 5.0f);
  int steps = 0;
  while (accumulator_ >= fixedDt_) {
    accumulator_ -= fixedDt_;
    ++steps;
  }
  return steps;
}

}  // namespace evolab::engine
