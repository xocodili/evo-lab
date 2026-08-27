#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CloacaSignal.hpp"
#include "sim/Energon.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismComputer.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("camp factory builds P-M-C-A chain with hub and register", "[camp][computer]") {
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, 1.0f);
  REQUIRE(organism.isCampNom());
  REQUIRE(organismHasCampTopology(organism));
  REQUIRE(organism.nodes.size() == 4);
  REQUIRE(organism.links.size() == 3);
  REQUIRE(organism.neuralAxons.size() == 12);
  REQUIRE(organism.findNode(1)->neuron == evolab::NeuronType::Perceptor);
  REQUIRE(organism.findNode(2)->neuron == evolab::NeuronType::Mouth);
  REQUIRE(organism.findNode(3)->neuron == evolab::NeuronType::Computer);
  REQUIRE(organism.findNode(4)->neuron == evolab::NeuronType::Actuator);
  REQUIRE(organism.bodyStorage.size() == 60);
  REQUIRE(organism.computerRegister[0] == evolab::kNeuronConfidenceNeutral);
}

TEST_CASE("digest moves mouth surplus into computer hub", "[camp][computer]") {
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, 1.0f);
  evolab::SkeletonNode* mouth = organism.findNode(2);
  REQUIRE(mouth != nullptr);
  mouth->store.assign(evolab::kNeuronStoreMaxBytes + 10, 1);
  organism.bodyStorage.clear();

  evolab::digestMouthToComputer(organism);
  REQUIRE(mouth->store.size() == evolab::kNeuronStoreMaxBytes);
  REQUIRE(organism.bodyStorage.size() == 10);
}

TEST_CASE("computer pattern match sets feed gain", "[camp][computer]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, 1.0f);

  evolab::NeuralAxon* pToC = organism.findNeuralAxon(1, 3);
  evolab::NeuralAxon* mToC = organism.findNeuralAxon(2, 3);
  evolab::NeuralAxon* aToC = organism.findNeuralAxon(4, 3);
  REQUIRE(pToC != nullptr);
  REQUIRE(mToC != nullptr);
  REQUIRE(aToC != nullptr);

  organism.computerRegister[0] = 6;
  organism.computerRegister[1] = 5;
  organism.computerRegister[2] = 4;
  pToC->lastReceived.valid = true;
  pToC->lastReceived.byte = 6;
  pToC->lastReceived.tick = 10;
  mToC->lastReceived.valid = true;
  mToC->lastReceived.byte = 5;
  mToC->lastReceived.tick = 10;
  aToC->lastReceived.valid = true;
  aToC->lastReceived.byte = 4;
  aToC->lastReceived.tick = 10;

  evolab::tickComputerPhase(organism, field, 10);
  REQUIRE(organism.lastComputerMatchScore > 0.9f);
  REQUIRE(organism.computerFeedGain > 0.9f);
}

TEST_CASE("replete computer expels green baseline cloaca byte", "[camp][computer][cloaca]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, 1.0f);
  organism.bodyStorage.assign(evolab::kComputerHubStoreMaxBytes, 1);
  organism.createdAtTick = 0;

  const std::size_t hubBefore = organism.bodyStorage.size();
  const int blobsBefore = field.activeCount();
  evolab::tickComputerPhase(organism, field, 1);
  REQUIRE(field.activeCount() > blobsBefore);
  REQUIRE(organism.bodyStorage.size() == hubBefore - evolab::kCloacaVentCostBaseline);
  REQUIRE(organism.lastCloacaBandExpelled == evolab::CloacaBand::Baseline);
  REQUIRE(organism.lastHubSignalExpelledThisTick);

  const evolab::EnergonBlob& blob = field.blobs().back();
  REQUIRE(blob.origin == evolab::EnergonOrigin::Cloaca);
  REQUIRE(blob.remaining == evolab::kCloacaVentCostBaseline);
  REQUIRE(evolab::cloacaBandFromBlob(blob) == evolab::CloacaBand::Baseline);
}

TEST_CASE("mate-ready computer expels red cloaca trail", "[camp][computer][cloaca]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, 1.0f);
  organism.bodyStorage.assign(evolab::kComputerHubStoreMaxBytes, 1);
  organism.createdAtTick = 0;

  evolab::SkeletonNode* mouth = organism.findNode(2);
  evolab::SkeletonNode* perceptor = organism.findNode(1);
  evolab::SkeletonNode* actuator = organism.findNode(4);
  REQUIRE(mouth != nullptr);
  REQUIRE(perceptor != nullptr);
  REQUIRE(actuator != nullptr);
  mouth->store.assign(evolab::kNeuronStoreMaxBytes, 1);
  perceptor->store.assign(evolab::kPerceptorScanCostPerTick + 4, 1);
  actuator->store.assign(evolab::kActuatorStrokeCostPerTick + 4, 1);

  const std::size_t hubBefore = organism.bodyStorage.size();
  evolab::tickComputerPhase(organism, field, evolab::kMateMinAgeTicks + 10);
  REQUIRE(organism.lastCloacaBandExpelled == evolab::CloacaBand::Mate);
  REQUIRE(organism.bodyStorage.size() == hubBefore - evolab::kCloacaVentCostMate);

  const evolab::EnergonBlob& blob = field.blobs().back();
  REQUIRE(blob.origin == evolab::EnergonOrigin::Cloaca);
  REQUIRE(blob.remaining == evolab::kCloacaVentCostMate);
  REQUIRE(evolab::cloacaBandFromBlob(blob) == evolab::CloacaBand::Mate);
}

TEST_CASE("distressed computer expels blue cloaca alarm", "[camp][computer][cloaca]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, 1.0f);
  organism.bodyStorage.assign(evolab::kComputerHubReserveBytes + 5, 1);
  organism.findNode(1)->basalArrearsTicks = 1;

  evolab::tickComputerPhase(organism, field, 1);
  REQUIRE(organism.lastCloacaBandExpelled == evolab::CloacaBand::Distress);

  const evolab::EnergonBlob& blob = field.blobs().back();
  REQUIRE(blob.origin == evolab::EnergonOrigin::Cloaca);
  REQUIRE(evolab::cloacaBandFromBlob(blob) == evolab::CloacaBand::Distress);
}
