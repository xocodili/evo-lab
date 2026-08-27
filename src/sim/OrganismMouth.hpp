#pragma once

#include "sim/PerceptorFocus.hpp"

#include <cstdint>

namespace evolab {

class Organism;
struct SkeletonNode;

struct MouthInteroception {
  float approach = 0.0f;
  float flee = 0.0f;
  float localSatiation = 0.0f;
  float actuatorActivity = 0.0f;
  float perceptorSalience = 0.0f;
  bool perceptorLocked = false;
  PerceptFocusKind focusKind = PerceptFocusKind::None;
};

struct FeedIntent {
  float biteDrive = 0.0f;
  bool allowFoodBite = false;
  bool feedSuppressed = false;
};

MouthInteroception gatherMouthInteroception(const Organism& organism, std::uint32_t mouthId,
                                            const SkeletonNode& mouth, std::uint64_t simTick);

FeedIntent computeCampFeedIntent(const MouthInteroception& interoception);

}  // namespace evolab
