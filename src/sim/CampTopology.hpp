#pragma once

#include "sim/Organism.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace evolab {

inline constexpr std::uint32_t kCampPerceptorId = 1;
inline constexpr std::uint32_t kCampMouthId = 2;
inline constexpr std::uint32_t kCampComputerId = 3;
inline constexpr std::uint32_t kCampActuatorId = 4;
inline constexpr std::uint32_t kCampRootNodeId = kCampComputerId;

inline constexpr std::pair<std::uint32_t, std::uint32_t> kCampDevelopmentalAxons[] = {
    {1, 2}, {1, 3}, {1, 4}, {2, 1}, {2, 3}, {2, 4},
    {3, 1}, {3, 2}, {3, 4}, {4, 1}, {4, 2}, {4, 3},
};
inline constexpr std::size_t kCampDevelopmentalAxonCount = sizeof(kCampDevelopmentalAxons) / sizeof(kCampDevelopmentalAxons[0]);

bool organismHasCampTopology(const Organism& organism);

// At least one live P, M, C, and A (topology may be non-canonical).
bool organismHasCampNeuronFloor(const Organism& organism);

// Run CAMP neuron tick phases (perceive, camp feed intent, digest, computer, convey).
bool organismUsesCampNeuronPhases(const Organism& organism);

// Ordered genotype label over live nodes (e.g. "PMCA", "PPMCAA").
std::string campGenotypeLabel(const Organism& organism);

}  // namespace evolab
