#pragma once

#include "sim/PerceptorFocus.hpp"

#include <cstdint>

namespace evolab {

class EnergonField;
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
  float tasteSalience = 0.0f;
  float tasteGradient = 0.0f;
  bool mouthChewPaused = false;
};

struct FeedIntent {
  float biteDrive = 0.0f;
  bool allowFoodBite = false;
  bool feedSuppressed = false;
  bool mouthChewPaused = false;
};

MouthInteroception gatherMouthInteroception(const Organism& organism, std::uint32_t mouthId,
                                            const SkeletonNode& mouth, std::uint64_t simTick);

FeedIntent computeCampFeedIntent(const MouthInteroception& interoception);

void runMouthTastePhase(Organism& organism, const EnergonField& energon, float cellSize,
                        std::uint64_t simTick);

}  // namespace evolab
