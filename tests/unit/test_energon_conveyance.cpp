#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonConveyance.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismInternal.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("mouth bite overflow routes to hub when wallet full", "[energon_conveyance]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.alive = true;

  evolab::SkeletonNode* mouth = organism.findNode(2);
  REQUIRE(mouth != nullptr);
  mouth->store.clear();

  for (int i = 0; i < static_cast<int>(evolab::kNeuronStoreMaxBytes); ++i) {
    evolab::neuronStorePush(organism, *mouth, 1);
  }
  REQUIRE(mouth->store.size() == evolab::kNeuronStoreMaxBytes);

  const std::size_t hubBefore = evolab::computerHubFuelBytes(organism);
  evolab::organism_detail::creditMouthStore(organism, *mouth, field, 7, 1);
  REQUIRE(mouth->store.size() == evolab::kNeuronStoreMaxBytes);
  REQUIRE(evolab::computerHubFuelBytes(organism) == hubBefore + 1);
  REQUIRE(field.activeCount() == 0);
}

TEST_CASE("mouth operational conveyance routes bytes to actuator before hub digest",
          "[energon_conveyance]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.alive = true;

  evolab::SkeletonNode* mouth = organism.findNode(2);
  evolab::SkeletonNode* actuator = organism.findNode(4);
  REQUIRE(mouth != nullptr);
  REQUIRE(actuator != nullptr);

  evolab::NeuralAxon* mToA = organism.findNeuralAxon(2, 4);
  REQUIRE(mToA != nullptr);
  mToA->trustFeed = evolab::kTrustBaseline;
  mToA->etaEnergy = 1.0f;
  mToA->etaSignal = 1.0f;

  for (int i = 0; i < static_cast<int>(evolab::kMouthConveyReserveBytes + 14); ++i) {
    evolab::neuronStorePush(organism, *mouth, 1);
  }
  mouth->storeBytesPriorTick = mouth->store.size();
  organism.equilibriumExportStartUnit = 0.45f;
  actuator->store.clear();

  evolab::conveyMouthDownstream(organism, field, 1);
  REQUIRE(actuator->store.size() > 0);
  REQUIRE(mouth->store.size() >= evolab::kMouthConveyReserveBytes);
}

TEST_CASE("conveyance applies eta energy loss on axon hops", "[energon_conveyance]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.alive = true;

  evolab::SkeletonNode* mouth = organism.findNode(2);
  evolab::SkeletonNode* actuator = organism.findNode(4);
  REQUIRE(mouth != nullptr);
  REQUIRE(actuator != nullptr);

  evolab::NeuralAxon* mToA = organism.findNeuralAxon(2, 4);
  REQUIRE(mToA != nullptr);
  mToA->trustFeed = evolab::kTrustBaseline;
  mToA->etaEnergy = 0.5f;
  mToA->etaSignal = 1.0f;

  for (int i = 0; i < 40; ++i) {
    evolab::neuronStorePush(organism, *mouth, 1);
  }
  mouth->storeBytesPriorTick = mouth->store.size();
  organism.equilibriumExportStartUnit = 0.45f;
  actuator->store.clear();

  const std::size_t actuatorBefore = actuator->store.size();
  evolab::conveyMouthDownstream(organism, field, 1);
  REQUIRE(actuator->store.size() > actuatorBefore);
}

TEST_CASE("returned axon bytes dissipate at mouth without field spam", "[energon_conveyance]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.alive = true;

  evolab::SkeletonNode* mouth = organism.findNode(2);
  evolab::SkeletonNode* perceptor = organism.findNode(1);
  REQUIRE(mouth != nullptr);
  REQUIRE(perceptor != nullptr);

  evolab::NeuralAxon* pToM = organism.findNeuralAxon(1, 2);
  REQUIRE(pToM != nullptr);
  pToM->trustFeed = evolab::kTrustBaseline;
  pToM->etaEnergy = 1.0f;
  pToM->etaSignal = 1.0f;

  for (int i = 0; i < 35; ++i) {
    evolab::neuronStorePush(organism, *perceptor, 2);
  }
  perceptor->storeBytesPriorTick = perceptor->store.size();
  organism.equilibriumExportStartUnit = 0.45f;

  const std::size_t mouthBefore = mouth->store.size();
  const int blobsBefore = field.activeCount();
  evolab::conveyCampEnergon(organism, field, 2);
  REQUIRE(mouth->store.size() == mouthBefore);
  REQUIRE(field.activeCount() == blobsBefore);
}

TEST_CASE("computer hub dispatch respects conservation export scale",
          "[energon_conveyance][stem]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.alive = true;
  evolab::assignComputerHubFuel(organism, 200000, 1);

  evolab::SkeletonNode* computer = organism.findNode(evolab::kCampComputerId);
  REQUIRE(computer != nullptr);
  computer->computerFeedGain = 1.0f;
  computer->storeBytesPriorTick = computer->store.size() + 8;

  const std::size_t hubBefore = evolab::computerHubFuelBytes(organism);
  evolab::conveyCampEnergon(organism, field, 1);
  REQUIRE(evolab::computerHubFuelBytes(organism) == hubBefore);
}

TEST_CASE("computer hub dispatch is capped per tick", "[energon_conveyance][stem]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.alive = true;
  evolab::assignComputerHubFuel(organism, 200000, 1);

  evolab::SkeletonNode* computer = organism.findNode(evolab::kCampComputerId);
  evolab::SkeletonNode* perceptor = organism.findNode(evolab::kCampPerceptorId);
  evolab::SkeletonNode* actuator = organism.findNode(evolab::kCampActuatorId);
  REQUIRE(computer != nullptr);
  REQUIRE(perceptor != nullptr);
  REQUIRE(actuator != nullptr);

  perceptor->store.clear();
  actuator->store.clear();
  computer->computerFeedGain = 1.0f;
  computer->storeBytesPriorTick = computer->store.size();
  organism.hubConservationExportScale = 1.0f;

  for (evolab::NeuralAxon& axon : organism.neuralAxons) {
    if (axon.srcNodeId == evolab::kCampComputerId) {
      axon.trustFeed = evolab::kTrustBaseline;
      axon.etaEnergy = 1.0f;
      axon.etaSignal = 1.0f;
    }
  }

  const std::size_t hubBefore = evolab::computerHubFuelBytes(organism);
  evolab::conveyCampEnergon(organism, field, 1);
  const std::size_t hubAfter = evolab::computerHubFuelBytes(organism);
  REQUIRE(hubBefore >= hubAfter);
  REQUIRE(hubBefore - hubAfter <= evolab::kComputerHubDispatchMaxPerTick);
}
