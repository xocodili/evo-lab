#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Energon.hpp"
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

TEST_CASE("satiated computer expels blue signal byte", "[camp][computer]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, 1.0f);
  organism.bodyStorage.assign(evolab::kComputerHubStoreMaxBytes, 1);

  const int blobsBefore = field.activeCount();
  evolab::tickComputerPhase(organism, field, 1);
  REQUIRE(field.activeCount() > blobsBefore);
  REQUIRE(organism.bodyStorage.size() == evolab::kComputerHubStoreMaxBytes - 1);
}
