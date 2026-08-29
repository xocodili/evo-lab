#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CloacaSignal.hpp"
#include "sim/Energon.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismComputer.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {

void seedComputerInbound(evolab::Organism& organism, std::uint32_t computerId, std::uint8_t p,
                         std::uint8_t m, std::uint8_t a, std::uint64_t tick) {
  evolab::NeuralAxon* pToC = organism.findNeuralAxon(1, computerId);
  evolab::NeuralAxon* mToC = organism.findNeuralAxon(2, computerId);
  evolab::NeuralAxon* aToC = organism.findNeuralAxon(4, computerId);
  REQUIRE(pToC != nullptr);
  REQUIRE(mToC != nullptr);
  REQUIRE(aToC != nullptr);
  pToC->lastReceived.valid = true;
  pToC->lastReceived.byte = p;
  pToC->lastReceived.tick = tick;
  mToC->lastReceived.valid = true;
  mToC->lastReceived.byte = m;
  mToC->lastReceived.tick = tick;
  aToC->lastReceived.valid = true;
  aToC->lastReceived.byte = a;
  aToC->lastReceived.tick = tick;
}

void seedDualComputerInbound(evolab::Organism& organism, std::uint8_t p, std::uint8_t m,
                             std::uint8_t a, std::uint64_t tick) {
  seedComputerInbound(organism, 3, p, m, a, tick);
  evolab::NeuralAxon* pToC2 = organism.findNeuralAxon(1, 4);
  evolab::NeuralAxon* mToC2 = organism.findNeuralAxon(2, 4);
  evolab::NeuralAxon* aToC2 = organism.findNeuralAxon(5, 4);
  REQUIRE(pToC2 != nullptr);
  REQUIRE(mToC2 != nullptr);
  REQUIRE(aToC2 != nullptr);
  pToC2->lastReceived.valid = true;
  pToC2->lastReceived.byte = p;
  pToC2->lastReceived.tick = tick;
  mToC2->lastReceived.valid = true;
  mToC2->lastReceived.byte = m;
  mToC2->lastReceived.tick = tick;
  aToC2->lastReceived.valid = true;
  aToC2->lastReceived.byte = a;
  aToC2->lastReceived.tick = tick;
}

float actuatorDispatchDrive(const evolab::SkeletonNode& computer) {
  return computer.computerFeedGain * evolab::confidenceToUnit(computer.computerRegister[6]);
}

}  // namespace

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
  REQUIRE(organism.bodyStorage.size() == 80);
  REQUIRE(organism.findNode(3)->computerRegister[0] == evolab::kNeuronConfidenceNeutral);
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
  evolab::SkeletonNode* computer = organism.findNode(3);
  REQUIRE(computer != nullptr);

  computer->computerRegister[0] = 6;
  computer->computerRegister[1] = 5;
  computer->computerRegister[2] = 4;
  seedComputerInbound(organism, 3, 6, 5, 4, 10);

  evolab::tickComputerPhase(organism, field, 10);
  REQUIRE(computer->lastComputerMatchScore > 0.9f);
  REQUIRE(computer->computerFeedGain > 0.9f);
  REQUIRE(organism.computerFeedGain == computer->computerFeedGain);
}

TEST_CASE("computer CTA RPE suppresses dispatch when P and M disagree", "[camp][computer]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, 1.0f);
  evolab::SkeletonNode* computer = organism.findNode(3);
  REQUIRE(computer != nullptr);

  computer->computerRegister[0] = 7;
  computer->computerRegister[1] = 7;
  computer->computerRegister[2] = 4;
  seedComputerInbound(organism, 3, 7, 1, 4, 10);

  evolab::tickComputerPhase(organism, field, 10);
  REQUIRE(computer->lastComputerPredictionError < -0.5f);
  REQUIRE(computer->computerFeedGain < 0.75f);
}

TEST_CASE("dual computer forage C drives A dispatch on food pattern", "[camp][computer]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeDualComputerCampOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, 1.0f);
  REQUIRE(campGenotypeLabel(organism) == "PMCCA");

  evolab::SkeletonNode* cForage = organism.findNode(3);
  evolab::SkeletonNode* cThreat = organism.findNode(4);
  REQUIRE(cForage != nullptr);
  REQUIRE(cThreat != nullptr);

  seedDualComputerInbound(organism, 7, 6, 5, 10);
  evolab::tickComputerPhase(organism, field, 10);

  REQUIRE(cForage->lastComputerMatchScore > 0.9f);
  REQUIRE(cThreat->lastComputerMatchScore < cForage->lastComputerMatchScore);
  REQUIRE(cForage->computerFeedGain > cThreat->computerFeedGain);
  REQUIRE(actuatorDispatchDrive(*cForage) > actuatorDispatchDrive(*cThreat) * 2.0f);
  REQUIRE(organism.computerFeedGain == cForage->computerFeedGain);
}

TEST_CASE("dual computer threat C drives A dispatch on threat pattern", "[camp][computer]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism =
      evolab::makeDualComputerCampOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, 1.0f);

  evolab::SkeletonNode* cForage = organism.findNode(3);
  evolab::SkeletonNode* cThreat = organism.findNode(4);
  REQUIRE(cForage != nullptr);
  REQUIRE(cThreat != nullptr);

  seedDualComputerInbound(organism, 1, 2, 3, 10);
  evolab::tickComputerPhase(organism, field, 10);

  REQUIRE(cThreat->lastComputerMatchScore > 0.9f);
  REQUIRE(cForage->lastComputerMatchScore < cThreat->lastComputerMatchScore);
  REQUIRE(cThreat->computerFeedGain > cForage->computerFeedGain);
  REQUIRE(actuatorDispatchDrive(*cThreat) > actuatorDispatchDrive(*cForage) * 2.0f);
  REQUIRE(organism.computerFeedGain == cThreat->computerFeedGain);
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
