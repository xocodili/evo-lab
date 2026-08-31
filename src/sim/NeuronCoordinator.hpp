#pragma once

#include "sim/Organism.hpp"

#include <cstdint>

namespace evolab {

// ---------------------------------------------------------------------------
// Stem-cell mini-C (Hz / energon coordinator)
//
// Evolutionary layering on each SkeletonNode:
//   1. mini-C  — coordinatorRegister + coordinatorDutyScale (ALL cells, including stem)
//   2. full C  — computerRegister + dispatch (Computer nodes only; mini-C still runs inside)
//
// mini-C pattern-matches local fuel + sense + Δ → dutyScale (cell-autonomous throttle).
// full C pattern-matches inbound P/M/A believe bytes → organ dispatch; dispatch gain is
// multiplied by the same node's mini-C dutyScale (recursive containment).
// ---------------------------------------------------------------------------

struct CoordinatorInteroception {
  float fuelUnit = 0.0f;
  float senseDrive = 0.0f;
  std::uint8_t observedByte = kNeuronConfidenceNeutral;
  float patternMatch = 0.0f;
  float excitation = 0.0f;
  float delta = 0.0f;
};

CoordinatorInteroception gatherCoordinatorInteroception(const Organism& organism,
                                                        const SkeletonNode& node);

void initCoordinatorNodeRegister(SkeletonNode& node);

void tickCoordinatorPhase(Organism& organism, std::uint64_t simTick);

float coordinatorDutyScaleForNode(const Organism& organism, std::uint32_t nodeId);

// full C organ gain × mini-C duty (clamped to organ minimum dispatch).
float applyMiniCToComputerDispatch(float organDispatchGain, float coordinatorDutyScale,
                                   float hubConservationExportScale = 1.0f);

float computeOrganismFamineUnit(const Organism& organism, std::uint64_t simTick);

float coordinatorMinDutyForNeuron(NeuronType neuron);

}  // namespace evolab
