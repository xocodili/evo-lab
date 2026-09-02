#pragma once

#include "sim/NeuronStem.hpp"
#include "sim/Organism.hpp"
#include "sim/WorldBinding.hpp"

#include <cstdint>
#include <random>
#include <vector>

namespace evolab {

// Assembly plans and organism build — stem bind operator lives in NeuronStem.hpp.

StemAssemblyPlan defaultCampStemAssemblyPlan();

StemAssemblyPlan extractStemAssemblyPlan(const Organism& organism);

void assignStemAssemblyPlan(Organism& organism, StemAssemblyPlan plan);

void closeStemNeuralGraphAmongLoci(Organism& organism);

Organism assembleOrganismFromStemPlan(std::uint32_t id, float wx, float wz, float wy,
                                      std::size_t storageBytes, std::uint64_t createdAtTick,
                                      float boneLength, const StemAssemblyPlan& plan,
                                      float heading);

bool organismStemBindGeometryMatchesCamp(const Organism& organism, float headingToleranceRad =
                                                                               0.05f);

bool organismUsesStemBindRecords(const Organism& organism);

std::uint32_t stemBindStepCount(const Organism& organism);

void reconcileStemAssemblyLinks(Organism& organism, float heading, std::mt19937& rng);

}  // namespace evolab
