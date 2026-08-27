#pragma once

#include "sim/OrganismActuator.hpp"
#include "sim/OrganismMouth.hpp"
#include "sim/PerceptorFocus.hpp"

#include <cstdint>
#include <vector>

namespace evolab {

class NeuralAxon;
class Organism;

struct ComputerInteroception;

struct MouthTrustEvent {
  bool hadFoodContact = false;
  bool ate = false;
  bool feedSuppressed = false;
};

struct PerceptorTrustEvent {
  bool scanPaid = false;
  bool hadFoodCandidate = false;
  bool focusLocked = false;
  PerceptFocusKind focusKind = PerceptFocusKind::None;
  std::uint8_t confidence = 0;
};

struct ComputerTrustEvent {
  float matchScore = 0.0f;
  bool expelled = false;
};

// Post-feed: reward-modulated updates on inbound P→M and A→M axons.
void applyCampMouthTrustLearning(Organism& organism, std::uint32_t mouthId,
                                const MouthTrustEvent& event, std::uint64_t simTick);

// Post-advect: reward-modulated updates on inbound P→A and M→A axons.
void applyCampActuatorTrustLearning(Organism& organism, std::uint32_t actuatorId,
                                   const ActuatorInteroception& interoception,
                                   const MotorIntent& intent, float displacement,
                                   std::uint64_t simTick);

// Post-perceive: reward-modulated updates on inbound M→P and A→P axons.
void applyCampPerceptorTrustLearning(Organism& organism, std::uint32_t perceptorId,
                                    const PerceptorTrustEvent& event, std::uint64_t simTick);

// Post-digest: reward-modulated updates on inbound P→C, M→C, and A→C axons.
void applyCampComputerTrustLearning(Organism& organism, std::uint32_t computerId,
                                    const ComputerInteroception& interoception,
                                    const ComputerTrustEvent& event, std::uint64_t simTick);

// Post-transfer: feed-channel plasticity when an axon successfully delivered bytes.
void applyFeedTrustFromTransfer(NeuralAxon& axon, int bytesMoved, std::uint64_t simTick,
                                std::uint32_t organismId);

}  // namespace evolab
