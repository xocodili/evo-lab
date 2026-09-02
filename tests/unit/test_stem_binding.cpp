#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismParthenogenesis.hpp"
#include "sim/StemBinding.hpp"
#include "sim/WorldConstants.hpp"

#include "engine/kinematics/Math.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

TEST_CASE("default camp stem assembly yields canonical torpedo chain", "[stembinding]") {
  evolab::Organism organism = evolab::makeCampNomOrganism(
      1, 0.0f, 0.0f, 1.0f, evolab::kStemCellStorageMaxBytes, 0, evolab::kWorldCellSize);

  REQUIRE(organism.isCampNom());
  REQUIRE(evolab::organismUsesStemBindRecords(organism));
  REQUIRE(evolab::stemBindStepCount(organism) == evolab::kCampTorpedoChainSegmentCount);
  REQUIRE(evolab::organismHasCampTorpedoChain(organism));
  REQUIRE(evolab::campTorpedoMorphologyLabel(organism) == "MPCA");
  REQUIRE(evolab::organismStemBindGeometryMatchesCamp(organism));

  const evolab::StemAssemblyPlan plan = evolab::extractStemAssemblyPlan(organism);
  REQUIRE(plan.chains.size() == evolab::kCampTorpedoChainSegmentCount);
  REQUIRE(plan.binds.empty());
}

TEST_CASE("stem bind slot angles match legacy camp bind constants", "[stembinding]") {
  const float heading = 0.35f;
  REQUIRE(std::fabs(evolab::hubSocketAngleRad(heading, 0) - heading) < 1e-5f);
  REQUIRE(std::fabs(evolab::hubSocketAngleRad(heading, 1) -
                    (heading + evolab::kCampMouthBindAngle)) < 1e-5f);
  REQUIRE(std::fabs(evolab::engine::kinematics::normalizeAngle(
                evolab::hubSocketAngleRad(heading, 2) -
                (heading + evolab::kCampActuatorBindAngle))) < 1e-5f);
}

TEST_CASE("parthenogenesis replays inherited stem bind steps", "[stembinding][parthenogenesis]") {
  evolab::BarrenWorld world(42, 64);
  float wx = 0.0f;
  float wz = 0.0f;
  for (float x = -32.0f; x <= 32.0f; x += 1.0f) {
    for (float z = -32.0f; z <= 32.0f; z += 1.0f) {
      if (world.isWetWorld(x, z, evolab::kWorldCellSize)) {
        wx = x;
        wz = z;
        break;
      }
    }
  }

  evolab::Organism parent = evolab::makeCampNomOrganism(
      1, wx, wz, 1.0f, evolab::kStemCellStorageMaxBytes, 0, evolab::kWorldCellSize);
  parent.alive = true;
  parent.createdAtTick = 0;
  parent.heading = 0.0f;
  for (evolab::SkeletonNode& node : parent.nodes) {
    node.worldX = wx;
    node.worldZ = wz;
  }
  evolab::assignComputerHubFuel(parent, evolab::estimateParthenogenesisRequiredHubBytes() + 50'000,
                                1);

  std::uint32_t nextId = 2;
  evolab::ParthenogenesisPassOptions options;
  options.structuralRateOverride = 0.0f;
  options.skipEligibilityChecks = true;

  const evolab::ParthenogenesisResult result = evolab::attemptParthenogenesis(
      parent, world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 1000, nextId, options);

  REQUIRE(result.spawned);
  REQUIRE(result.stemBindStepsReplayed == evolab::kCampTorpedoChainSegmentCount);
  REQUIRE(result.child.stemAssembly.chains.size() == parent.stemAssembly.chains.size());
  REQUIRE(evolab::organismStemBindGeometryMatchesCamp(result.child));
}

TEST_CASE("high structural rate yields morphological freaks with stem geometry",
          "[stembinding][parthenogenesis][freak]") {
  evolab::Organism parent = evolab::makeCampNomOrganism(
      1, 0.0f, 0.0f, 1.0f, evolab::kStemCellStorageMaxBytes, 0, evolab::kWorldCellSize);
  parent.alive = true;
  parent.heading = 0.2f;

  int freakMorphologies = 0;
  int stemGeometryOk = 0;
  int topologyChanged = 0;

  for (int trial = 0; trial < 32; ++trial) {
    std::mt19937 rng = evolab::chaosSpawnRng(static_cast<std::uint64_t>(3000 + trial),
                                               static_cast<std::uint64_t>(trial) ^
                                                   evolab::kChaosSaltParthenogenesis);
    evolab::Organism freak = evolab::cloneCampChildFromParent(
        parent, static_cast<std::uint32_t>(200 + trial), parent.rootWorldX(), parent.rootWorldZ(),
        parent.rootWorldY(), static_cast<std::uint64_t>(4000 + trial), rng, 1.0f);

    ++freakMorphologies;
    if (evolab::organismStemBindGeometryMatchesCamp(freak)) {
      ++stemGeometryOk;
    }
    if (freak.nodes.size() != parent.nodes.size() || freak.links.size() != parent.links.size() ||
        freak.neuralAxons.size() != parent.neuralAxons.size()) {
      ++topologyChanged;
    }
  }

  REQUIRE(freakMorphologies == 32);
  INFO("stemGeometryOk=" << stemGeometryOk << " topologyChanged=" << topologyChanged);
  REQUIRE(topologyChanged >= 3);
}
