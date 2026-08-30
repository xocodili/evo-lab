#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/NeuronCoordinator.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismComputer.hpp"
#include "sim/WorldConstants.hpp"

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
    REQUIRE(node.coordinatorDutyScale >= evolab::kCoordinatorMinDutyScale);
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
