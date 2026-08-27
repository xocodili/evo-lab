#pragma once

#include "sim/OrganismMouth.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/TideAdvection.hpp"

#include <cstdint>
#include <vector>

namespace evolab {

class BarrenWorld;
class EnergonField;
class Organism;
struct SkeletonNode;

namespace organism_detail {

void consumeBytes(std::vector<std::uint8_t>& storage, std::uint32_t count);
void creditMouthStore(SkeletonNode& node, EnergonField& field, std::uint8_t byte,
                      std::uint32_t units);
void tickMouthNode(Organism& organism, SkeletonNode& node, EnergonField& field, float radius,
                   std::uint64_t simTick, const FeedIntent* pmaFeedIntent);
void tickNeuronViability(Organism& organism, EnergonField& field);
void updateOrganismHeading(Organism& organism, const AdvectionVelocity& velocity,
                           const EnergonField& energon, float cellSize);
void tickActuatorOrganism(Organism& organism, const BarrenWorld& world, float cellSize,
                          float halfExtent, std::uint64_t simTick);
void runMouthSignalPhase(Organism& organism, EnergonField& field, std::uint64_t simTick);

}  // namespace organism_detail

}  // namespace evolab
