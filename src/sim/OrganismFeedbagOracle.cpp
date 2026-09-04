#include "sim/OrganismFeedbagOracle.hpp"

#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/EnergonString.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/OrganismParthenogenesis.hpp"

namespace evolab {

namespace {

bool mouthHasFoodInRange(const EnergonField& field, float wx, float wz, float radius) {
  bool found = false;
  field.forEachBlobNear(wx, wz, radius, [&](const EnergonBlob& blob) {
    if (blob.remaining > 0) {
      found = true;
    }
  });
  return found;
}

float feedbagOracleMinHubCapFactor() {
  const std::size_t targetHubBytes =
      estimateParthenogenesisRequiredHubBytes() + kTicksPerStemCellDay;
  return clampWalletCapFactor(static_cast<float>(targetHubBytes) /
                              static_cast<float>(kComputerHubStoreMaxBytes));
}

void ensureFeedbagOracleHubSolvent(Organism& organism) {
  const std::size_t required = estimateParthenogenesisRequiredHubBytes();
  if (computerHubFuelBytes(organism) >= required) {
    return;
  }
  const std::size_t target =
      std::min(hubStoreCapBytes(organism), required + kTicksPerStemCellDay);
  assignComputerHubFuel(organism, target, 1);
}

}  // namespace

void prepareFeedbagOracleAxons(Organism& organism) {
  for (NeuralAxon& axon : organism.neuralAxons) {
    axon.trustFeed = kTrustBaseline;
    axon.etaEnergy = 1.0f;
    axon.etaSignal = 1.0f;
  }
}

void ensureAbundantFoodAtMouth(EnergonField& field, const SkeletonNode& mouth, float cellSize) {
  const float radius = cellSize * kMouthContactRadiusFactor;
  if (mouthHasFoodInRange(field, mouth.worldX, mouth.worldZ, radius)) {
    return;
  }
  field.injectBlob(makeWetSunfallBlob(mouth.worldX, mouth.worldZ, kChompFieldBytes, 0xAA));
}

void installFeedbagReproductionOracle(Organism& organism, std::uint64_t simTick) {
  if (!organism.isCampNom()) {
    return;
  }
  organism.feedbagOracle = true;
  organism.createdAtTick = simTick;
  organism.hubStoreCapFactor =
      clampWalletCapFactor(std::max(organism.hubStoreCapFactor, feedbagOracleMinHubCapFactor()));
  const std::size_t targetHubBytes =
      estimateParthenogenesisRequiredHubBytes() + kTicksPerStemCellDay;
  for (SkeletonNode& node : organism.nodes) {
    if (node.neuron == NeuronType::Computer) {
      continue;
    }
    node.store.assign(kNeuronStoreMaxBytes, 1);
    node.basalArrearsTicks = 0;
  }
  SkeletonNode* computer = findComputerHubNode(organism);
  if (computer != nullptr) {
    initComputerHubStore(*computer, targetHubBytes, organism);
    for (std::uint8_t& byte : computer->store) {
      byte = 1;
    }
    computer->basalArrearsTicks = 0;
  }
  organism.computerFeedGain = 1.0f;
  prepareFeedbagOracleAxons(organism);
}

bool tickFeedbagOracleHooks(Organism& organism, EnergonField& energon, float cellSize) {
  if (!organism.feedbagOracle || !organism.alive) {
    return false;
  }
  const SkeletonNode* mouth = findNeuronNode(organism, NeuronType::Mouth);
  if (mouth != nullptr) {
    ensureAbundantFoodAtMouth(energon, *mouth, cellSize);
  }
  organism.computerFeedGain = 1.0f;
  ensureFeedbagOracleHubSolvent(organism);
  return true;
}

}  // namespace evolab
