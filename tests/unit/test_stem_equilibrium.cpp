#include "sim/CellConstants.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonConveyance.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/Organism.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("stem equilibrium blocks perceptor export while store drains", "[stem][equilibrium]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.equilibriumExportStartUnit = 0.45f;

  evolab::SkeletonNode* perceptor = organism.findNode(1);
  REQUIRE(perceptor != nullptr);

  for (int i = 0; i < 36; ++i) {
    evolab::neuronStorePush(organism, *perceptor, 1);
  }
  perceptor->storeBytesPriorTick = perceptor->store.size() + 8;
  REQUIRE(evolab::stemNodeEquilibriumExportScale(organism, *perceptor) == 0.0f);

  const std::size_t pBefore = perceptor->store.size();
  evolab::conveyCampEnergon(organism, field, 1);
  REQUIRE(perceptor->store.size() == pBefore);

  perceptor->storeBytesPriorTick = perceptor->store.size();
  REQUIRE(evolab::stemNodeEquilibriumExportScale(organism, *perceptor) > 0.0f);
  evolab::conveyCampEnergon(organism, field, 2);
  REQUIRE(perceptor->store.size() <= pBefore);
}

TEST_CASE("stem hub dispatch allows minimum export below ramp knee when stable", "[stem][equilibrium]") {
  evolab::StemEquilibriumParams params;
  params.currentBytes = 180000;
  params.priorBytes = 180000;
  params.cap = 345600;
  params.reserveBytes = evolab::kComputerHubReserveBytes;
  params.slackBytes = evolab::kComputerHubConservationSlackBytes;
  params.exportStartUnit = 0.55f;
  params.exportFullUnit = evolab::confidenceToUnit(evolab::kComputerSatiationConfidence);

  REQUIRE(evolab::stemHubDispatchExportScale(params) >= evolab::kStemEquilibriumMinExportScale);
  REQUIRE(evolab::stemHubDispatchExportScale(params) < 0.5f);
}

TEST_CASE("stem equilibrium allows minimum export below ramp knee when stable", "[stem][equilibrium]") {
  evolab::StemEquilibriumParams params;
  params.currentBytes = 180000;
  params.priorBytes = 180000;
  params.cap = 345600;
  params.reserveBytes = evolab::kComputerHubReserveBytes;
  params.slackBytes = evolab::kComputerHubConservationSlackBytes;
  params.exportStartUnit = 0.55f;
  params.exportFullUnit = evolab::confidenceToUnit(evolab::kComputerSatiationConfidence);

  const float scale = evolab::stemEquilibriumExportScale(params);
  REQUIRE(scale >= evolab::kStemEquilibriumMinExportScale);
  REQUIRE(scale < 0.5f);
}

TEST_CASE("stem equilibrium export start is jittered at spawn", "[stem][equilibrium]") {
  std::mt19937 rng(9001);
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.finalizeSpawn(rng);
  REQUIRE(organism.equilibriumExportStartUnit >= evolab::kStemEquilibriumExportStartMin);
  REQUIRE(organism.equilibriumExportStartUnit <= evolab::kStemEquilibriumExportStartMax);
}

TEST_CASE("stem equilibrium scales all camp neuron types consistently", "[stem][equilibrium]") {
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.equilibriumExportStartUnit = 0.50f;

  for (evolab::SkeletonNode& node : organism.nodes) {
    if (node.neuron == evolab::NeuronType::None) {
      continue;
    }
    node.storeBytesPriorTick = node.store.size();
    if (node.neuron == evolab::NeuronType::Computer) {
      evolab::assignComputerHubFuel(organism, static_cast<std::size_t>(evolab::hubStoreCapBytes(organism) * 0.7), 1);
    } else if (node.neuron == evolab::NeuronType::Mouth) {
      node.store.assign(evolab::kNeuronStoreMaxBytes, 1);
    } else {
      node.store.assign(evolab::kNeuronStoreMaxBytes + 4, 1);
    }
  }

  evolab::refreshCampEquilibriumExportScales(organism);
  for (const evolab::SkeletonNode& node : organism.nodes) {
    if (node.neuron == evolab::NeuronType::None) {
      continue;
    }
    REQUIRE(node.equilibriumExportScale > 0.0f);
  }
  REQUIRE(organism.hubConservationExportScale > 0.0f);
}
