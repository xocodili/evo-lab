#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/NeuronCoordinator.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismActuator.hpp"
#include "sim/OrganismComputer.hpp"
#include "sim/OrganismNeuron.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("stem cell coordinator throttles duty when hub replete", "[coordinator][stemcell]") {
  evolab::Organism organism =
      evolab::makeUndifferentiatedOrganism(1, 0.0f, 0.0f, 1.0f, evolab::kComputerHubStoreMaxBytes, 0);
  organism.alive = true;
  evolab::SkeletonNode* root = organism.findNode(organism.rootNodeId);
  REQUIRE(root != nullptr);
  root->coordinatorRegister[0] = evolab::kNeuronConfidenceMax;

  evolab::tickCoordinatorPhase(organism, 1);
  REQUIRE(organism.coordinatorDutyScale < evolab::kCoordinatorMaxDutyScale);
  REQUIRE(organism.coordinatorDutyScale >= evolab::kCoordinatorMinDutyScale);
}

TEST_CASE("camp nom coordinator runs on every neuron node", "[coordinator][camp]") {
  evolab::BarrenWorld world(3, 32);
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, evolab::kTicksPerStemCellDay, 0,
                                  evolab::kWorldCellSize);
  camper.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  evolab::tickCoordinatorPhase(camper, 42);
  int liveNodes = 0;
  for (const evolab::SkeletonNode& node : camper.nodes) {
    if (!node.alive) {
      continue;
    }
    ++liveNodes;
    REQUIRE(node.coordinatorDutyScale >= evolab::coordinatorMinDutyForNeuron(node.neuron));
    REQUIRE(node.coordinatorDutyScale <= evolab::kCoordinatorMaxDutyScale);
  }
  REQUIRE(liveNodes == 4);
  REQUIRE(camper.coordinatorMinNodeDuty <= camper.coordinatorMaxNodeDuty);
}

TEST_CASE("computer register seeds from mini-C proto template", "[coordinator][camp]") {
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, evolab::kWorldCellSize);
  const evolab::SkeletonNode* computer = camper.findNode(evolab::kCampRootNodeId);
  REQUIRE(computer != nullptr);
  REQUIRE(computer->computerRegister[0] == computer->coordinatorRegister[0]);
  REQUIRE(computer->computerRegister[1] == computer->coordinatorRegister[0]);
  REQUIRE(computer->computerRegister[2] == computer->coordinatorRegister[0]);
}

TEST_CASE("recursive mini-C throttles full C dispatch after coordinator tick", "[coordinator][camp]") {
  evolab::BarrenWorld world(3, 32);
  evolab::EnergonField energon(1, {});
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, evolab::kTicksPerStemCellDay, 0,
                                  evolab::kWorldCellSize);
  evolab::SkeletonNode* computer = camper.findNode(evolab::kCampRootNodeId);
  REQUIRE(computer != nullptr);

  computer->coordinatorRegister[0] = evolab::kNeuronConfidenceMax;
  evolab::assignComputerHubFuel(camper, evolab::kComputerHubStoreMaxBytes, 1);

  evolab::tickCoordinatorPhase(camper, 1);
  REQUIRE(computer->coordinatorDutyScale < 1.0f);

  evolab::tickComputerPhase(camper, energon, 1);
  const float unthrottled = evolab::applyMiniCToComputerDispatch(1.0f, 1.0f);
  REQUIRE(computer->computerFeedGain < unthrottled);
}

TEST_CASE("symmetric mouth taste marks ambiguity for tumble bias", "[coordinator][chemotaxis]") {
  const float salience = 0.6f;
  const float temporalDelta = 0.0f;
  const float avgVectorMagSq = 0.01f;
  const bool symmetricAmbiguity =
      salience >= evolab::kMouthTasteSalienceFloor &&
      avgVectorMagSq <= evolab::kMouthTasteSymmetryVectorEpsilonSq &&
      std::abs(temporalDelta) <= evolab::kOrganismCampReflexMinValence;
  REQUIRE(symmetricAmbiguity);
}

TEST_CASE("famine stress lowers duty when hub empty and field barren", "[coordinator][famine][regulation]") {
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, evolab::kTicksPerStemCellDay, 0,
                                  evolab::kWorldCellSize);
  evolab::SkeletonNode* computer = camper.findNode(evolab::kCampRootNodeId);
  REQUIRE(computer != nullptr);
  computer->store.clear();

  for (evolab::SkeletonNode& node : camper.nodes) {
    if (node.neuron == evolab::NeuronType::Mouth) {
      node.mouthTasteSalience = 0.0f;
      node.mouthTasteGradient = 0.0f;
    }
    if (node.neuron == evolab::NeuronType::Perceptor) {
      node.focusKind = evolab::PerceptFocusKind::None;
      node.focusLocked = false;
      node.focusSalience = 0.0f;
      node.perceptPriorFoodSalienceValid = false;
    }
  }

  evolab::tickCoordinatorPhase(camper, 10);
  REQUIRE(camper.famineUnit > 0.35f);

  const evolab::SkeletonNode* actuator =
      evolab::findFirstNeuronNode(camper, evolab::NeuronType::Actuator, true);
  REQUIRE(actuator != nullptr);
  REQUIRE(actuator->coordinatorDutyScale < 0.65f);
  REQUIRE(actuator->coordinatorDutyScale >= evolab::kCoordinatorMinDutyActuator);
}

TEST_CASE("feast suppresses famine when hub and field food are abundant", "[coordinator][famine]") {
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, evolab::kWorldCellSize);
  evolab::assignComputerHubFuel(camper, evolab::kComputerHubStoreMaxBytes, 1);
  for (evolab::SkeletonNode& node : camper.nodes) {
    if (node.neuron == evolab::NeuronType::Mouth) {
      node.mouthTasteSalience = 0.85f;
    }
  }

  evolab::tickCoordinatorPhase(camper, 11);
  REQUIRE(camper.famineUnit == Catch::Approx(0.0f));
}

TEST_CASE("computer outbound reflects famine abundance", "[coordinator][famine]") {
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, evolab::kWorldCellSize);
  const evolab::SkeletonNode* computer = camper.findNode(evolab::kCampRootNodeId);
  REQUIRE(computer != nullptr);
  camper.famineUnit = 0.9f;
  camper.famineConfidence = evolab::famineAbundanceConfidence(camper.famineUnit);

  const std::uint8_t outbound =
      evolab::encodeNeuronOutboundConfidence(camper, evolab::NeuronType::Computer, *computer);
  REQUIRE(static_cast<int>(outbound) <= 1);
}

TEST_CASE("actuator motor intent scales with coordinator duty", "[coordinator][famine]") {
  evolab::ActuatorInteroception interoception;
  const evolab::MotorIntent fullDuty = evolab::computeCampMotorIntent(interoception, 8, 1.0f);
  const evolab::MotorIntent torpor = evolab::computeCampMotorIntent(interoception, 8, 0.15f);
  REQUIRE(torpor.strokeBytes <= fullDuty.strokeBytes);
  REQUIRE(torpor.tumbleRateScale <= fullDuty.tumbleRateScale);
}
