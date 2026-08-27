#pragma once

#include "sim/Energon.hpp"
#include "sim/Organism.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace evolab {

// Shared stem-cell layer: fuel pools, basal metabolism, field expulsion, and CAMP signal emit
// used by all differentiated neuron types (P, M, C, A).

SkeletonNode* findNeuronNode(Organism& organism, NeuronType type, bool requireAlive = true);
const SkeletonNode* findNeuronNode(const Organism& organism, NeuronType type,
                                   bool requireAlive = true);

std::vector<std::uint8_t>* neuronFuelPool(Organism& organism, SkeletonNode& node);
const std::vector<std::uint8_t>* neuronFuelPool(const Organism& organism,
                                                const SkeletonNode& node);

void consumeFuelBack(std::vector<std::uint8_t>& storage, std::size_t count);

bool tryPayNeuronBasalCost(Organism& organism, SkeletonNode& node);

void expelByteAtNode(const SkeletonNode& node, EnergonField& field, std::uint8_t byte,
                     EnergonOrigin origin, float ttlScale, float zOffsetFactor = 0.0f);

void releaseFuelAtNode(const SkeletonNode& node, EnergonField& field,
                       std::vector<std::uint8_t>& storage, EnergonOrigin origin,
                       float ttlScale);

// CAMP pre-advect: M + C hub confidence (A emits at end of advect).
void emitCampPreAdvectSignals(Organism& organism, std::uint64_t simTick);

void emitCampActuatorSignals(Organism& organism, std::uint64_t simTick);

}  // namespace evolab
