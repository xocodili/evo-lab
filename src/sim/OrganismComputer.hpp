#pragma once

#include "sim/CampTopology.hpp"
#include "sim/Energon.hpp"
#include "sim/Organism.hpp"

#include <cstdint>

namespace evolab {

struct ComputerInteroception {
  std::uint8_t fromPerceptor = kNeuronConfidenceNeutral;
  std::uint8_t fromMouth = kNeuronConfidenceNeutral;
  std::uint8_t fromActuator = kNeuronConfidenceNeutral;
};

ComputerInteroception gatherComputerInteroception(const Organism& organism,
                                                  std::uint32_t computerId,
                                                  std::uint64_t simTick);

// After feed: move mouth buffer surplus into the C hub store (digest).
void digestMouthToComputer(Organism& organism);

// Pattern match inbound believe bytes vs register; satiation expulsion; dispatch gain.
void tickComputerPhase(Organism& organism, EnergonField& field, std::uint64_t simTick);

}  // namespace evolab
