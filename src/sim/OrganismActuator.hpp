#pragma once

#include "sim/PerceptorFocus.hpp"

#include <cstdint>

namespace evolab {

class Organism;

// Inbound axon state fused at the actuator (mirrors P's InteroceptionPrior pattern).
struct ActuatorInteroception {
  float approach = 0.0f;
  float flee = 0.0f;
  float satiation = 0.0f;
  float hubSatiation = 0.0f;
  float mouthConfidence = 0.0f;
  float perceptorSalience = 0.0f;
  bool perceptorLocked = false;
  PerceptFocusKind focusKind = PerceptFocusKind::None;
  float focusBearing = 0.0f;
  float gazeHeading = 0.0f;
  float mouthTasteApproach = 0.0f;
  float mouthTasteBearing = 0.0f;
  float mouthTasteGradient = 0.0f;
  bool mouthTasteSymmetricAmbiguity = false;
  // Temporal Δ of M→A satiation byte (unit space); A ignores absolute satiation level.
  float mouthSignalDelta = 0.0f;
};

struct MotorIntent {
  float netDrive = 0.0f;
  std::uint32_t strokeBytes = 0;
  float turnRateScale = 0.0f;
  float tumbleRateScale = 1.0f;
  bool motorSuppressed = false;
};

// Post-kinematics trust learning: snapshot taken when a CAMP stroke is queued.
struct CampActuatorProprioSnapshot {
  bool pending = false;
  float startX = 0.0f;
  float startZ = 0.0f;
  std::uint32_t actuatorId = 0;
  ActuatorInteroception interoception;
  MotorIntent motorIntent;
};

ActuatorInteroception gatherActuatorInteroception(const Organism& organism,
                                                  std::uint32_t actuatorId,
                                                  std::uint64_t simTick);

MotorIntent computeCampMotorIntent(const ActuatorInteroception& interoception,
                                  std::uint32_t actuatorFuelBytes);

// Energon anchors locomotion: P food/threat lock or directional M taste suppress tumble.
bool campLocomotionAnchored(const ActuatorInteroception& interoception);

void applyCampChemotaxisHeading(Organism& organism, const ActuatorInteroception& interoception,
                               const MotorIntent& intent);

// Store M→A satiation sample for next tick's gradient (after advect).
void commitActuatorMouthInboundPrior(Organism& organism, const ActuatorInteroception& interoception,
                                     std::uint64_t simTick);

}  // namespace evolab
