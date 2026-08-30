#pragma once

#include "sim/Organism.hpp"

#include <cstddef>
#include <cstdint>

namespace evolab {

struct SkeletonNode;

// ---------------------------------------------------------------------------
// Mitochondria model: every node owns SkeletonNode.store.
//   Undifferentiated stem (None) — cap kStemCellStorageMaxBytes in root.store.
//   P / M / A — nominal cap kNeuronStoreMaxBytes × peripheralStoreCapFactor.
//   C (Computer)     — hub cap kComputerHubStoreMaxBytes × hubStoreCapFactor.
// mouthChewFill remains the pre-digestion chew zone (not wallet bytes).
// ---------------------------------------------------------------------------

SkeletonNode* findComputerHubNode(Organism& organism);
const SkeletonNode* findComputerHubNode(const Organism& organism);

std::size_t totalOrganismFuelBytes(const Organism& organism);
std::size_t computerHubFuelBytes(const Organism& organism);

std::size_t peripheralStoreCapBytes(const Organism& organism);
std::size_t hubStoreCapBytes(const Organism& organism);
std::size_t stemStoreCapBytes(const Organism& organism);
std::size_t nodeStoreNominalCap(const Organism& organism, const SkeletonNode& node);
std::size_t nodeStoreReserveBytes(const SkeletonNode& node);

void initStemNodeStore(SkeletonNode& node, std::size_t storageBytes);
void initPeripheralNodeStore(SkeletonNode& node, std::size_t storageBytes,
                             const Organism& organism);
void initComputerHubStore(SkeletonNode& computer, std::size_t storageBytes,
                          const Organism& organism);

// Move stem root fuel into a newly differentiated C hub (same bytes, expanded cap).
void promoteStemStoreToComputerHub(SkeletonNode& computer, SkeletonNode& stemRoot);

void neuronStorePush(Organism& organism, SkeletonNode& node, std::uint8_t byte);
void neuronConsumeBack(SkeletonNode& node, std::size_t count);
std::size_t neuronStoreSurplus(const Organism& organism, const SkeletonNode& node);
bool neuronPopBackForConvey(SkeletonNode& node, std::uint8_t& byte);
std::size_t neuronStoreAcceptanceRemaining(const Organism& organism, const SkeletonNode& node);

// C hub surplus / acceptance (same backing as computer->store).
void hubStorePush(Organism& organism, std::uint8_t byte);
bool hubStorePopBack(Organism& organism, std::uint8_t& byte);
bool hubStoreConsumeBack(Organism& organism, std::size_t count);
std::size_t hubStoreSurplus(const Organism& organism);
std::size_t hubStoreAcceptanceRemaining(const Organism& organism);

// Test / harness helpers.
void assignComputerHubFuel(Organism& organism, std::size_t bytes, std::uint8_t fillByte = 0);

}  // namespace evolab
