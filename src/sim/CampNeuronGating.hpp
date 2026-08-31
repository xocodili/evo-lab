#pragma once

#include "sim/CellConstants.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismNeuron.hpp"

namespace evolab {

// Shared Go/NoGo helpers — source bytes carry neuron-specific semantics; fusion is subtractive.

inline float campHungerFromMouthUnit(float mouthUnit) {
  return clamp01(1.0f - mouthUnit * 0.5f);
}

inline float campMouthChewNoGo(float mouthUnit) {
  const float brake = confidenceToUnit(kMouthInhibitActuatorConfidence);
  if (mouthUnit < brake) {
    return 0.0f;
  }
  return mouthUnit;
}

inline void updateMouthChewPause(bool& paused, float mouthUnit) {
  const float stopBand = confidenceToUnit(kMouthInhibitActuatorConfidence);
  const float startBand = confidenceToUnit(kMouthChewResumeActuatorConfidence);
  if (paused) {
    if (mouthUnit <= startBand) {
      paused = false;
    }
  } else if (mouthUnit >= stopBand) {
    paused = true;
  }
}

inline bool campMouthChewRefuseActive(bool chewPaused, bool foodGuided, float foodApproach) {
  return chewPaused && foodGuided && foodApproach <= kMouthChewRefuseMaxApproach;
}

inline float campHubRepleteNoGo(float hubUnit) {
  const float brake = confidenceToUnit(kComputerSatiationConfidence);
  if (hubUnit < brake) {
    return 0.0f;
  }
  return hubUnit;
}

inline float campLocomotionFleeNoGo(float flee) {
  return flee * 0.85f;
}

inline float campFeedFleeNoGo(float flee) {
  return flee * 0.9f;
}

inline float campActuatorActivityNoGo(float activity) {
  return activity * 0.15f;
}

}  // namespace evolab
