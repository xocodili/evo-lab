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
// Kinematic root is the primary (lowest-id) live mouth — first component in the dev pipeline.
inline constexpr std::uint32_t kCampRootNodeId = kCampMouthId;

inline constexpr std::pair<std::uint32_t, std::uint32_t> kCampTorpedoChainLinks[] = {
    {kCampActuatorId, kCampComputerId},
    {kCampComputerId, kCampPerceptorId},
    {kCampPerceptorId, kCampMouthId},
};
inline constexpr std::size_t kCampTorpedoChainSegmentCount =
    sizeof(kCampTorpedoChainLinks) / sizeof(kCampTorpedoChainLinks[0]);

inline constexpr std::pair<std::uint32_t, std::uint32_t> kCampDevelopmentalAxons[] = {
    {1, 2}, {1, 3}, {1, 4}, {2, 1}, {2, 3}, {2, 4},
    {3, 1}, {3, 2}, {3, 4}, {4, 1}, {4, 2}, {4, 3},
};
inline constexpr std::size_t kCampDevelopmentalAxonCount = sizeof(kCampDevelopmentalAxons) / sizeof(kCampDevelopmentalAxons[0]);

bool organismHasCampTopology(const Organism& organism);

// All twelve CAMP developmental axon edges present between canonical node ids.
bool organismHasCampDevelopmentalAxons(const Organism& organism);

bool isCampDevelopmentalAxonEdge(std::uint32_t srcId, std::uint32_t dstId);

bool isCampTorpedoChainLinkEdge(std::uint32_t parentId, std::uint32_t childId);

// Hub (C) has a skeletal link to each live P, M, and A arm (legacy Y-star).
bool organismHasCampHubArms(const Organism& organism);

// Colinear chain links present (may be degraded — dead nodes still count as absent for floor).
bool organismHasCampTorpedoSkeleton(const Organism& organism);

// Canonical torpedo: A→C→P→M colinear chain along heading (M ram nose, A tail).
bool organismHasCampTorpedoChain(const Organism& organism);

// At least one live P, M, C, and A (topology may be non-canonical).
bool organismHasCampNeuronFloor(const Organism& organism);

// Run CAMP neuron tick phases (perceive, camp feed intent, digest, computer, convey).
bool organismUsesCampNeuronPhases(const Organism& organism);

// Four-node hub-and-spoke camp skeleton (muscle arms), including degraded campers.
bool organismUsesCampSkeletonVisual(const Organism& organism);

// Developmental locus string in node-vector order (e.g. "PMCA", "PPMCAA").
std::string campGenotypeLabel(const Organism& organism);

// Nose-to-tail torpedo morphology (e.g. "MPCA" for canonical ram chain); falls back to genotype.
std::string campTorpedoMorphologyLabel(const Organism& organism);

// Inspector / HUD type string: torpedo morphology when applicable, else genotype.
std::string campDisplayTypeLabel(const Organism& organism);

// Lowest-id live mouth — stable kinematic root across duplicate-mouth mutations.
const SkeletonNode* findPrimaryMouthNode(const Organism& organism);

// Sync rootNodeId to primary mouth when one exists; otherwise keep a live fallback.
void ensureKinematicRootNodeId(Organism& organism);

// Camp torpedo / hub-arm / articulated organisms integrate at the primary mouth.
bool organismUsesMouthKinematicRoot(const Organism& organism);

// Articulated body dynamics (not the same as canonical isCampNom() topology gate).
bool organismUsesArticulatedLocomotion(const Organism& organism);

}  // namespace evolab
