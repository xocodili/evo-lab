#pragma once

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
  float focusBearing = 0.0f;
  float gazeHeading = 0.0f;
};

struct MotorIntent {
  float netDrive = 0.0f;
  std::uint32_t strokeBytes = 0;
  float turnRateScale = 0.0f;
  float tumbleRateScale = 1.0f;
  bool motorSuppressed = false;
};

ActuatorInteroception gatherActuatorInteroception(const Organism& organism,
                                                  std::uint32_t actuatorId,
                                                  std::uint64_t simTick);

MotorIntent computeCampMotorIntent(const ActuatorInteroception& interoception,
                                  std::uint32_t actuatorFuelBytes);

void applyCampChemotaxisHeading(Organism& organism, const ActuatorInteroception& interoception,
                               const MotorIntent& intent);

}  // namespace evolab
