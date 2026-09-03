#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellPopulation.hpp"
#include "sim/DayCycle.hpp"
#include "sim/Energon.hpp"
#include "sim/CellConstants.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/Organism.hpp"
#include "sim/SimConfig.hpp"
#include "sim/WorldConstants.hpp"

#include "engine/Camera.hpp"
#include "game/OrganismDrawer.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

namespace {

evolab::EnergonConfig makeVisualTankEnergonConfig(int nomCount) {
  evolab::EnergonConfig config;
  config.populationScaledRain = true;
  config.rainPopulationBaseline = nomCount;
  config.spawnRateMax = 10.0f;
  config.maxBlobs = std::max(4000, nomCount * 100);
  return config;
}

struct VisualTankHarness {
  evolab::SimConfig config;
  evolab::BarrenWorld world;
  evolab::DayCycle dayCycle;
  evolab::EnergonField energon;
  evolab::CellPopulation cells;

  explicit VisualTankHarness(std::uint64_t seed = 42, bool installFeedbag = false)
      : config(),
        world(config.seed, config.resolution, evolab::makeTideFromConfig(config)),
        dayCycle(evolab::kVisualDayCyclePeriodTicks),
        energon(seed, makeVisualTankEnergonConfig(config.nomCount)),
        cells() {
    config.seed = seed;
    cells.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, config.nomCount,
                   config.seed);
    if (installFeedbag) {
      cells.installFeedbagReproductionOracle(world, evolab::kWorldCellSize,
                                             evolab::kTerrainHeightScale, world.tickCount());
    }
  }

  void tick(int count) {
    for (int tick = 0; tick < count; ++tick) {
      world.tick();
      const float sun = dayCycle.sunIntensity(world.tickCount());
      energon.tick(world, sun, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                   cells.liveCampNomCount());
      cells.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, sun);
    }
  }
};

struct LocomotionSample {
  int ticks = 0;
  int strokeTicks = 0;
  int inhibitedTicks = 0;
  int lockedFoodTicks = 0;
  int zeroActuatorFuelTicks = 0;
  int hubRepleteTicks = 0;
  float displacementSum = 0.0f;
  float netDriveSum = 0.0f;
  std::size_t actuatorFuelSum = 0;
  std::size_t hubFuelSum = 0;
};

}  // namespace

TEST_CASE("visual tank locomotion diagnostic over 600 ticks", "[diagnostic][locomotion][kinematics]") {
  VisualTankHarness harness;
  LocomotionSample sample;
  float totalStartX = 0.0f;
  float totalStartZ = 0.0f;
  int startCount = 0;

  for (const evolab::Organism& organism : harness.cells.organisms()) {
    if (!organism.alive || !organism.isCampNom() || organism.createdAtTick != 0) {
      continue;
    }
    totalStartX += organism.rootWorldX();
    totalStartZ += organism.rootWorldZ();
    ++startCount;
  }

  harness.tick(600);

  for (const evolab::Organism& organism : harness.cells.organisms()) {
    if (!organism.alive || !organism.isCampNom() || organism.feedbagOracle ||
        organism.createdAtTick != 0) {
      continue;
    }
    ++sample.ticks;
    if (organism.lastStrokePaid) {
      ++sample.strokeTicks;
    }
    if (organism.lastActuatorInhibited) {
      ++sample.inhibitedTicks;
    }
    const evolab::SkeletonNode* perceptor =
        evolab::findNeuronNode(organism, evolab::NeuronType::Perceptor, true);
    if (perceptor != nullptr && perceptor->focusLocked &&
        organism.lastPerceptFocusKind == evolab::PerceptFocusKind::Food) {
      ++sample.lockedFoodTicks;
    }
    const evolab::SkeletonNode* actuator =
        evolab::findNeuronNode(organism, evolab::NeuronType::Actuator, true);
    const std::size_t actuatorFuel = actuator != nullptr ? actuator->store.size() : 0;
    if (actuatorFuel < evolab::kActuatorStrokeCostPerTick) {
      ++sample.zeroActuatorFuelTicks;
    }
    sample.actuatorFuelSum += actuatorFuel;
    sample.hubFuelSum += organism.computerHubFuelBytes();
    if (organism.lastActuatorInteroception.hubSatiation >=
        evolab::confidenceToUnit(evolab::kComputerSatiationConfidence)) {
      ++sample.hubRepleteTicks;
    }
    sample.displacementSum += organism.lastDisplacement;
    sample.netDriveSum += organism.lastActuatorNetDrive;
  }

  float totalEndX = 0.0f;
  float totalEndZ = 0.0f;
  int endCount = 0;
  for (const evolab::Organism& organism : harness.cells.organisms()) {
    if (!organism.alive || !organism.isCampNom() || organism.createdAtTick != 0) {
      continue;
    }
    totalEndX += organism.rootWorldX();
    totalEndZ += organism.rootWorldZ();
    ++endCount;
  }

  const float cohortDrift =
      endCount > 0 && startCount > 0
          ? std::hypot(totalEndX / endCount - totalStartX / startCount,
                       totalEndZ / endCount - totalStartZ / startCount)
          : 0.0f;

  const float strokeRate =
      sample.ticks > 0 ? static_cast<float>(sample.strokeTicks) / static_cast<float>(sample.ticks)
                       : 0.0f;
  const float avgDisp =
      sample.ticks > 0 ? sample.displacementSum / static_cast<float>(sample.ticks) : 0.0f;
  const float avgNetDrive =
      sample.ticks > 0 ? sample.netDriveSum / static_cast<float>(sample.ticks) : 0.0f;
  const float avgActuatorFuel =
      sample.ticks > 0 ? static_cast<float>(sample.actuatorFuelSum) / static_cast<float>(sample.ticks)
                       : 0.0f;
  const float avgHubFuel =
      sample.ticks > 0 ? static_cast<float>(sample.hubFuelSum) / static_cast<float>(sample.ticks)
                       : 0.0f;
  const float zeroActuatorFuelRate =
      sample.ticks > 0 ? static_cast<float>(sample.zeroActuatorFuelTicks) /
                             static_cast<float>(sample.ticks)
                       : 0.0f;

  INFO("seedCohort start=" << startCount << " end=" << endCount << " cohortMeanDrift=" << cohortDrift
                           << " strokeRate=" << strokeRate << " avgDispPerTick=" << avgDisp
                           << " avgNetDrive=" << avgNetDrive << " avgActuatorFuel=" << avgActuatorFuel
                           << " avgHubFuel=" << avgHubFuel
                           << " inhibitedRate="
                           << (sample.ticks > 0
                                   ? static_cast<float>(sample.inhibitedTicks) /
                                         static_cast<float>(sample.ticks)
                                   : 0.0f)
                           << " lockedFoodRate="
                           << (sample.ticks > 0
                                   ? static_cast<float>(sample.lockedFoodTicks) /
                                         static_cast<float>(sample.ticks)
                                   : 0.0f)
                           << " zeroActuatorFuelRate=" << zeroActuatorFuelRate
                           << " hubRepleteRate="
                           << (sample.ticks > 0
                                   ? static_cast<float>(sample.hubRepleteTicks) /
                                         static_cast<float>(sample.ticks)
                                   : 0.0f));

  REQUIRE(sample.ticks > 0);
  REQUIRE(strokeRate > 0.05f);
  REQUIRE(avgDisp > 0.001f);
  REQUIRE(cohortDrift >= 0.0f);
  REQUIRE(zeroActuatorFuelRate >= 0.0f);
}

TEST_CASE("single camp nom articulation stays finite after 250 ticks",
          "[nom][kinematics][finite]") {
  evolab::SimConfig config;
  config.nomCount = 1;
  evolab::BarrenWorld world(config.seed, config.resolution, evolab::makeTideFromConfig(config));
  evolab::DayCycle dayCycle(evolab::kVisualDayCyclePeriodTicks);
  evolab::EnergonField energon(config.seed, makeVisualTankEnergonConfig(config.nomCount));
  evolab::CellPopulation cells;
  cells.seedNoms(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, config.nomCount,
                 config.seed);

  REQUIRE(cells.organisms().size() == 1);
  for (int tick = 0; tick < 250; ++tick) {
    world.tick();
    const float sun = dayCycle.sunIntensity(world.tickCount());
    energon.tick(world, sun, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                 cells.liveCampNomCount());
    cells.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, sun);
  }

  const evolab::Organism& organism = cells.organisms().front();
  REQUIRE(organism.alive);
  REQUIRE(organism.isCampNom());
  REQUIRE(organismUsesMouthKinematicRoot(organism));
  REQUIRE(organism.rootNodeId == evolab::kCampMouthId);

  float minSeg = 1.0e9f;
  for (const evolab::SkeletonLink& link : organism.links) {
    if (!link.muscleBundle) {
      continue;
    }
    const evolab::SkeletonNode* parent = organism.findNode(link.parentNodeId);
    const evolab::SkeletonNode* child = organism.findNode(link.childNodeId);
    REQUIRE(parent != nullptr);
    REQUIRE(child != nullptr);
    minSeg = std::min(minSeg, std::hypot(child->worldX - parent->worldX,
                                           child->worldZ - parent->worldZ));
    for (const evolab::SkeletonNode* node : {parent, child}) {
      REQUIRE(std::isfinite(node->worldX));
      REQUIRE(std::isfinite(node->worldY));
      REQUIRE(std::isfinite(node->worldZ));
      REQUIRE(std::abs(node->worldX) < 5000.0f);
      REQUIRE(std::abs(node->worldZ) < 5000.0f);
      REQUIRE(node->worldY > -100.0f);
      REQUIRE(node->worldY < 500.0f);
    }
  }
  REQUIRE(minSeg > 0.05f);
}

TEST_CASE("camp nom draw batch stays non-empty after 250 ticks", "[diagnostic][locomotion][render]") {
  VisualTankHarness harness;
  harness.tick(250);

  evolab::engine::OrbitCamera camera;
  camera.distance = 140.0f;
  const evolab::engine::Mat4 proj =
      evolab::engine::mat4Perspective(60.0f * 3.1415926535f / 180.0f, 1280.0f / 720.0f, 0.1f,
                                      800.0f);
  const evolab::engine::Mat4 mvp =
      evolab::engine::mat4Multiply(proj, camera.viewMatrix());

  const evolab::game::OrganismDrawSpriteSupport allSprites{true, true, true, true};
  const evolab::game::OrganismDrawBatch withSprites = evolab::game::buildOrganismDrawBatch(
      harness.cells.organisms(), 0.0f, 80.0f, 120.0f, mvp, 1280, 720, harness.world.tickCount(),
      harness.config.fixedSimHz, evolab::kWorldCellSize, true, allSprites);
  const evolab::game::OrganismDrawBatch withoutSprites = evolab::game::buildOrganismDrawBatch(
      harness.cells.organisms(), 0.0f, 80.0f, 120.0f, mvp, 1280, 720, harness.world.tickCount(),
      harness.config.fixedSimHz, evolab::kWorldCellSize, true);

  INFO("cellVerts=" << withSprites.cellVerts.size()
                    << " sprites=" << withSprites.spriteInstances.size());
  REQUIRE(!withSprites.cellVerts.empty());
  REQUIRE(!withoutSprites.cellVerts.empty());
  REQUIRE(withSprites.cellVerts.size() >= withoutSprites.cellVerts.size());
}

TEST_CASE("camp nom draw batch stays non-empty after 760 ticks", "[diagnostic][locomotion][render]") {
  VisualTankHarness harness;
  harness.tick(760);

  int alive = 0;
  for (const evolab::Organism& organism : harness.cells.organisms()) {
    if (organism.alive) {
      ++alive;
    }
  }
  INFO("alive organisms=" << alive);

  evolab::engine::OrbitCamera camera;
  camera.distance = 140.0f;
  const evolab::engine::Mat4 proj =
      evolab::engine::mat4Perspective(60.0f * 3.1415926535f / 180.0f, 1280.0f / 720.0f, 0.1f,
                                      800.0f);
  const evolab::engine::Mat4 mvp =
      evolab::engine::mat4Multiply(proj, camera.viewMatrix());

  const evolab::game::OrganismDrawSpriteSupport allSprites{true, true, true, true};
  const evolab::game::OrganismDrawBatch batch = evolab::game::buildOrganismDrawBatch(
      harness.cells.organisms(), 0.0f, 80.0f, 120.0f, mvp, 1280, 720, harness.world.tickCount(),
      harness.config.fixedSimHz, evolab::kWorldCellSize, false, allSprites);

  float minSeg = 1.0e9f;
  for (const evolab::Organism& organism : harness.cells.organisms()) {
    if (!organism.alive) {
      continue;
    }
    for (const evolab::SkeletonLink& link : organism.links) {
      if (!link.muscleBundle) {
        continue;
      }
      const evolab::SkeletonNode* parent = organism.findNode(link.parentNodeId);
      const evolab::SkeletonNode* child = organism.findNode(link.childNodeId);
      if (parent == nullptr || child == nullptr) {
        continue;
      }
      minSeg = std::min(minSeg, std::hypot(child->worldX - parent->worldX,
                                           child->worldZ - parent->worldZ));
    }
  }

  INFO("cellVerts=" << batch.cellVerts.size() << " sprites=" << batch.spriteInstances.size()
                    << " minSeg=" << minSeg);
  REQUIRE(alive > 0);
  REQUIRE(!batch.cellVerts.empty());
  REQUIRE(minSeg > 0.05f);
}

TEST_CASE("metabolise does not re-step camp kinematics", "[diagnostic][locomotion][kinematics]") {
  evolab::BarrenWorld world(42, 32);
  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.alive = true;
  organism.disableTideAdvection = true;

  organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 32.0f);

  struct NodePose {
    float x;
    float y;
    float z;
  };
  std::vector<NodePose> before;
  before.reserve(organism.nodes.size());
  for (const evolab::SkeletonNode& node : organism.nodes) {
    if (node.alive) {
      before.push_back({node.worldX, node.worldY, node.worldZ});
    }
  }
  REQUIRE(!before.empty());

  organism.metabolise(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  std::size_t idx = 0;
  for (const evolab::SkeletonNode& node : organism.nodes) {
    if (!node.alive) {
      continue;
    }
    REQUIRE(node.worldX == Catch::Approx(before[idx].x));
    REQUIRE(node.worldY == Catch::Approx(before[idx].y));
    REQUIRE(node.worldZ == Catch::Approx(before[idx].z));
    ++idx;
  }
}

TEST_CASE("visual session campers stay drawable with feedbag oracle at tick 200",
          "[diagnostic][locomotion][render]") {
  VisualTankHarness harness(42, true);
  harness.tick(200);

  evolab::engine::OrbitCamera camera;
  camera.distance = 140.0f;
  float sumX = 0.0f;
  float sumZ = 0.0f;
  int alive = 0;
  for (const evolab::Organism& organism : harness.cells.organisms()) {
    if (!organism.alive) {
      continue;
    }
    sumX += organism.rootWorldX();
    sumZ += organism.rootWorldZ();
    ++alive;
  }
  REQUIRE(alive >= 40);
  camera.targetX = sumX / static_cast<float>(alive);
  camera.targetZ = sumZ / static_cast<float>(alive);

  const evolab::engine::Mat4 proj =
      evolab::engine::mat4Perspective(60.0f * 3.1415926535f / 180.0f, 1280.0f / 720.0f, 0.1f,
                                      800.0f);
  const evolab::engine::Mat4 mvp =
      evolab::engine::mat4Multiply(proj, camera.viewMatrix());
  const evolab::game::OrganismDrawSpriteSupport allSprites{true, true, true, true};

  int drawable = 0;
  int collapsed = 0;
  int spawned = 0;
  for (const evolab::Organism& organism : harness.cells.organisms()) {
    if (!organism.alive || organism.feedbagOracle) {
      continue;
    }
    if (organism.createdAtTick > 0) {
      ++spawned;
    }
    float minSeg = 1.0e9f;
    for (const evolab::SkeletonLink& link : organism.links) {
      if (!link.muscleBundle) {
        continue;
      }
      const evolab::SkeletonNode* parent = organism.findNode(link.parentNodeId);
      const evolab::SkeletonNode* child = organism.findNode(link.childNodeId);
      if (parent == nullptr || child == nullptr) {
        continue;
      }
      minSeg = std::min(minSeg, std::hypot(child->worldX - parent->worldX,
                                           child->worldZ - parent->worldZ));
    }
    if (minSeg < 0.05f) {
      ++collapsed;
    }
    const evolab::game::OrganismDrawBatch one = evolab::game::buildOrganismDrawBatch(
        {organism}, 0.0f, 80.0f, 120.0f, mvp, 1280, 720, harness.world.tickCount(),
        harness.config.fixedSimHz, evolab::kWorldCellSize, false, allSprites);
    if (!one.cellVerts.empty()) {
      ++drawable;
    }
  }

  INFO("alive=" << alive << " drawable=" << drawable << " collapsed=" << collapsed
                << " spawned=" << spawned);
  REQUIRE(drawable >= 35);
  REQUIRE(collapsed <= 5);
}

TEST_CASE("seeded campers drawable with startup camera at tick 200",
          "[diagnostic][locomotion][render]") {
  VisualTankHarness harness(42, true);

  float startX = 0.0f;
  float startZ = 0.0f;
  int startAlive = 0;
  for (const evolab::Organism& organism : harness.cells.organisms()) {
    if (!organism.alive) {
      continue;
    }
    startX += organism.rootWorldX();
    startZ += organism.rootWorldZ();
    ++startAlive;
  }
  REQUIRE(startAlive >= 40);
  const float startupCamX = startX / static_cast<float>(startAlive);
  const float startupCamZ = startZ / static_cast<float>(startAlive);

  harness.tick(200);

  evolab::engine::OrbitCamera camera;
  camera.distance = 140.0f;
  camera.targetX = startupCamX;
  camera.targetZ = startupCamZ;

  const evolab::engine::Mat4 proj =
      evolab::engine::mat4Perspective(60.0f * 3.1415926535f / 180.0f, 1280.0f / 720.0f, 0.1f,
                                      800.0f);
  const evolab::engine::Mat4 mvp =
      evolab::engine::mat4Multiply(proj, camera.viewMatrix());
  const evolab::game::OrganismDrawSpriteSupport allSprites{true, true, true, true};

  int inFrustum = 0;
  int exploded = 0;
  for (const evolab::Organism& organism : harness.cells.organisms()) {
    if (!organism.alive || organism.feedbagOracle) {
      continue;
    }
    const float rx = organism.rootWorldX();
    const float rz = organism.rootWorldZ();
    if (!std::isfinite(rx) || !std::isfinite(rz) || std::abs(rx) > 500.0f || std::abs(rz) > 500.0f) {
      ++exploded;
      continue;
    }
    const float dx = rx - startupCamX;
    const float dz = rz - startupCamZ;
    if (std::hypot(dx, dz) < 80.0f) {
      ++inFrustum;
    }
  }

  const evolab::game::OrganismDrawBatch batch = evolab::game::buildOrganismDrawBatch(
      harness.cells.organisms(), 0.0f, 80.0f, 120.0f, mvp, 1280, 720, harness.world.tickCount(),
      harness.config.fixedSimHz, evolab::kWorldCellSize, false, allSprites);

  INFO("startupCam=(" << startupCamX << "," << startupCamZ << ") inFrustum=" << inFrustum
                      << " exploded=" << exploded << " verts=" << batch.cellVerts.size());
  REQUIRE(exploded == 0);
  REQUIRE(inFrustum >= 20);
  REQUIRE(!batch.cellVerts.empty());
}

TEST_CASE("seeded campers drawable with startup camera at tick 600",
          "[diagnostic][locomotion][render]") {
  VisualTankHarness harness(42, true);

  float startX = 0.0f;
  float startZ = 0.0f;
  int startAlive = 0;
  for (const evolab::Organism& organism : harness.cells.organisms()) {
    if (!organism.alive) {
      continue;
    }
    startX += organism.rootWorldX();
    startZ += organism.rootWorldZ();
    ++startAlive;
  }
  const float startupCamX = startX / static_cast<float>(startAlive);
  const float startupCamZ = startZ / static_cast<float>(startAlive);

  harness.tick(600);

  evolab::engine::OrbitCamera camera;
  camera.distance = 140.0f;
  camera.targetX = startupCamX;
  camera.targetZ = startupCamZ;

  const evolab::engine::Mat4 proj =
      evolab::engine::mat4Perspective(60.0f * 3.1415926535f / 180.0f, 1280.0f / 720.0f, 0.1f,
                                      800.0f);
  const evolab::engine::Mat4 mvp =
      evolab::engine::mat4Multiply(proj, camera.viewMatrix());
  const evolab::game::OrganismDrawSpriteSupport allSprites{true, true, true, true};

  int drawable = 0;
  int collapsed = 0;
  int exploded = 0;
  for (const evolab::Organism& organism : harness.cells.organisms()) {
    if (!organism.alive || organism.feedbagOracle) {
      continue;
    }
    const float rx = organism.rootWorldX();
    const float rz = organism.rootWorldZ();
    if (!std::isfinite(rx) || !std::isfinite(rz) || std::abs(rx) > 500.0f || std::abs(rz) > 500.0f) {
      ++exploded;
      continue;
    }
    float minSeg = 1.0e9f;
    for (const evolab::SkeletonLink& link : organism.links) {
      if (!link.muscleBundle) {
        continue;
      }
      const evolab::SkeletonNode* parent = organism.findNode(link.parentNodeId);
      const evolab::SkeletonNode* child = organism.findNode(link.childNodeId);
      if (parent == nullptr || child == nullptr) {
        continue;
      }
      minSeg = std::min(minSeg, std::hypot(child->worldX - parent->worldX,
                                           child->worldZ - parent->worldZ));
    }
    if (minSeg < 0.05f) {
      ++collapsed;
    }
    const evolab::game::OrganismDrawBatch one = evolab::game::buildOrganismDrawBatch(
        {organism}, 0.0f, 80.0f, 120.0f, mvp, 1280, 720, harness.world.tickCount(),
        harness.config.fixedSimHz, evolab::kWorldCellSize, false, allSprites);
    if (!one.cellVerts.empty()) {
      ++drawable;
    }
  }

  INFO("drawable=" << drawable << " collapsed=" << collapsed << " exploded=" << exploded);
  REQUIRE(exploded == 0);
  REQUIRE(drawable >= 35);
  REQUIRE(collapsed <= 5);
}

TEST_CASE("articulated render poses sample all live nodes", "[diagnostic][locomotion][render]") {
  evolab::BarrenWorld world(42, 32);
  evolab::EnergonField energon(1, {});
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, evolab::kWorldCellSize);
  organism.alive = true;
  for (int tick = 0; tick < 30; ++tick) {
    organism.advectRoot(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 32.0f);
  }

  evolab::Organism::RenderNodePoseMap poses;
  REQUIRE(organism.sampleArticulatedRenderPoses(world, evolab::kWorldCellSize,
                                               evolab::kTerrainHeightScale, poses));

  std::size_t aliveNodes = 0;
  for (const evolab::SkeletonNode& node : organism.nodes) {
    if (node.alive) {
      ++aliveNodes;
      REQUIRE(poses.find(node.id) != poses.end());
      const auto& xyz = poses.at(node.id);
      REQUIRE(std::isfinite(xyz[0]));
      REQUIRE(std::isfinite(xyz[1]));
      REQUIRE(std::isfinite(xyz[2]));
    }
  }
  REQUIRE(poses.size() == aliveNodes);
  REQUIRE(aliveNodes >= 4);
}

TEST_CASE("fk render poses keep draw batch non-empty with world option",
          "[diagnostic][locomotion][render]") {
  VisualTankHarness harness(42, true);
  harness.tick(200);

  evolab::engine::OrbitCamera camera;
  camera.distance = 140.0f;
  const evolab::engine::Mat4 proj =
      evolab::engine::mat4Perspective(60.0f * 3.1415926535f / 180.0f, 1280.0f / 720.0f, 0.1f,
                                      800.0f);
  const evolab::engine::Mat4 mvp =
      evolab::engine::mat4Multiply(proj, camera.viewMatrix());
  const evolab::game::OrganismDrawSpriteSupport allSprites{true, true, true, true};
  evolab::game::OrganismDrawOptions withWorld;
  withWorld.world = &harness.world;

  const evolab::game::OrganismDrawBatch physicsBatch = evolab::game::buildOrganismDrawBatch(
      harness.cells.organisms(), 0.0f, 80.0f, 120.0f, mvp, 1280, 720, harness.world.tickCount(),
      harness.config.fixedSimHz, evolab::kWorldCellSize, false, allSprites);
  const evolab::game::OrganismDrawBatch fkBatch = evolab::game::buildOrganismDrawBatch(
      harness.cells.organisms(), 0.0f, 80.0f, 120.0f, mvp, 1280, 720, harness.world.tickCount(),
      harness.config.fixedSimHz, evolab::kWorldCellSize, false, allSprites, withWorld);

  INFO("physics cellVerts=" << physicsBatch.cellVerts.size()
                            << " fk cellVerts=" << fkBatch.cellVerts.size()
                            << " physics bones=" << physicsBatch.boneLineVerts.size()
                            << " fk bones=" << fkBatch.boneLineVerts.size());
  REQUIRE(!physicsBatch.cellVerts.empty());
  REQUIRE(!fkBatch.cellVerts.empty());
  REQUIRE(fkBatch.boneLineVerts.size() >= physicsBatch.boneLineVerts.size() / 2);

  float maxRootDelta = 0.0f;
  int poseChecked = 0;
  for (const evolab::Organism& organism : harness.cells.organisms()) {
    if (!organism.alive || organism.feedbagOracle) {
      continue;
    }
    evolab::Organism::RenderNodePoseMap poses;
    if (!organism.sampleArticulatedRenderPoses(harness.world, evolab::kWorldCellSize,
                                               evolab::kTerrainHeightScale, poses)) {
      continue;
    }
    const evolab::SkeletonNode* root = organism.findNode(organism.rootNodeId);
    if (root == nullptr) {
      continue;
    }
    const auto it = poses.find(root->id);
    if (it == poses.end()) {
      continue;
    }
    maxRootDelta = std::max(
        maxRootDelta, std::hypot(it->second[0] - root->worldX, it->second[2] - root->worldZ));
    ++poseChecked;
  }
  INFO("poseChecked=" << poseChecked << " maxRootDelta=" << maxRootDelta);
  REQUIRE(poseChecked >= 20);
  REQUIRE(maxRootDelta < 5.0f);
}
