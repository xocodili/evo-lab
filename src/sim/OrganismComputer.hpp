#pragma once

#include "sim/CampTopology.hpp"
#include "sim/CloacaSignal.hpp"
#include "sim/Energon.hpp"
#include "sim/Organism.hpp"

#include <cstdint>

namespace evolab {

struct ComputerInteroception {
  std::uint8_t fromPerceptor = kNeuronConfidenceNeutral;
  std::uint8_t fromMouth = kNeuronConfidenceNeutral;
  std::uint8_t fromActuator = kNeuronConfidenceNeutral;
  // Proprioceptive hub/body state felt at C (not inbound axon bytes).
  float hubFuelUnit = 0.0f;
  std::uint32_t hubFuelBytes = 0;
  bool hubAtReserveFloor = false;
  float hubSatiationUnit = 0.0f;
  float conservationExportScale = 0.0f;
  bool distress = false;
  bool mateReady = false;
  bool distressVentAffordable = false;
  bool baselineVentAffordable = false;
};

ComputerInteroception gatherComputerInteroception(const Organism& organism,
                                                  std::uint32_t computerId,
                                                  std::uint64_t simTick);

// Proprioceptive hub/body fields for C (also used by cloaca band choice).
void seedComputerProprioInteroception(const Organism& organism, std::uint64_t simTick,
                                      ComputerInteroception& prior);

CloacaBand chooseCloacaBandFromInteroception(const ComputerInteroception& interoception);

// Spatial P valence vs postingestive M valence in [-1, 1] (positive = M confirms P).
float campComputerCtaPredictionError(const ComputerInteroception& interoception);

void initComputerNodeRegister(SkeletonNode& computer);
void guardComputerNodeRegister(SkeletonNode& computer);

// Fuse per-C match/gain onto organism telemetry (max gain, like actuator inbound fuse).
void syncOrganismComputerTelemetry(Organism& organism);

// After feed: move mouth buffer surplus into the C hub store (digest).
void digestMouthToComputer(Organism& organism);

// Pattern match inbound believe bytes vs register; satiation expulsion; dispatch gain.
void tickComputerPhase(Organism& organism, EnergonField& field, std::uint64_t simTick);

}  // namespace evolab
