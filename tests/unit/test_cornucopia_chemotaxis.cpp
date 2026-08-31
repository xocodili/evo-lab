#include "sim/BarrenWorld.hpp"
#include "sim/CampTraceLog.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/CloacaSignal.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonConveyance.hpp"
#include "sim/EnergonRain.hpp"
#include "sim/EnergonString.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/NeuronCoordinator.hpp"
#include "sim/NeuronTick.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismComputer.hpp"
#include "sim/OrganismParthenogenesis.hpp"
#include "sim/PerceptorFocus.hpp"
#include "sim/Tide.hpp"
#include "sim/TideAdvection.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <algorithm>
#include <random>
#include <vector>

namespace {

constexpr float kMinChemotaxisDepthFactor = 1.5f;
constexpr float kChemotaxisSurroundRadiusFactor = 2.5f;
constexpr float kChemotaxisCorridorSamples = 12;
// Marathon feedbag oracle reaches hub vent steady state in ~27 visual days; nursery cornucopia
// matches that horizon so crawl → eat → puke can complete without parthenogenesis pass.
constexpr int kNurseryVisualDaysForPuke =
    27 * static_cast<int>(std::lround(evolab::kVisualDayCyclePeriodTicks));
// After organic birth, run the child until parthenogenesis-eligible age plus one visual day.
constexpr int kNurseryTeenagerTicks =
    static_cast<int>(evolab::kParthenogenesisMinAgeTicks) +
    static_cast<int>(std::lround(evolab::kVisualDayCyclePeriodTicks));

float minChemotaxisDepth(float cellSize) {
  return cellSize * kMinChemotaxisDepthFactor;
}

float worldHalfExtent(const evolab::BarrenWorld& world, float cellSize) {
  const int res = world.heightmap().resolution;
  if (res <= 1 || cellSize <= 0.0f) {
    return 0.0f;
  }
  return static_cast<float>(res - 1) * cellSize * 0.5f;
}

bool isDeepWet(const evolab::BarrenWorld& world, float wx, float wz, float cellSize,
               float minDepth) {
  return world.depthAtWorld(wx, wz, cellSize) >= minDepth;
}

bool allCampNeuronsDeepWet(const evolab::BarrenWorld& world, const evolab::Organism& organism,
                           float cellSize, float minDepth) {
  for (const evolab::SkeletonNode& node : organism.nodes) {
    if (node.neuron == evolab::NeuronType::None) {
      continue;
    }
    if (!world.isWetWorld(node.worldX, node.worldZ, cellSize)) {
      return false;
    }
    if (world.depthAtWorld(node.worldX, node.worldZ, cellSize) < minDepth) {
      return false;
    }
  }
  return true;
}

bool wetSegmentDeep(const evolab::BarrenWorld& world, float ax, float az, float bx, float bz,
                    float cellSize, float minDepth, int samples) {
  if (samples < 1) {
    return isDeepWet(world, ax, az, cellSize, minDepth) &&
           isDeepWet(world, bx, bz, cellSize, minDepth);
  }
  for (int i = 0; i <= samples; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(samples);
    const float x = ax + (bx - ax) * t;
    const float z = az + (bz - az) * t;
    if (!isDeepWet(world, x, z, cellSize, minDepth)) {
      return false;
    }
  }
  return true;
}

bool wetDiscDeep(const evolab::BarrenWorld& world, float cx, float cz, float radius,
                 float cellSize, float minDepth) {
  constexpr int kDiscSamples = 16;
  for (int i = 0; i < kDiscSamples; ++i) {
    const float angle =
        6.2831853f * static_cast<float>(i) / static_cast<float>(kDiscSamples);
    const float x = cx + std::sin(angle) * radius;
    const float z = cz + std::cos(angle) * radius;
    if (!isDeepWet(world, x, z, cellSize, minDepth)) {
      return false;
    }
  }
  return isDeepWet(world, cx, cz, cellSize, minDepth);
}

bool forwardConeDeepWet(const evolab::BarrenWorld& world, float cellSize, float halfExtent,
                        float wx, float wz, float heading, float senseRadius, float minDepth) {
  const float fx = std::sin(heading);
  const float fz = std::cos(heading);
  const float samples[] = {0.35f, 0.6f, 0.85f, 1.0f};
  for (float fraction : samples) {
    const float probeX = wx + fx * senseRadius * fraction;
    const float probeZ = wz + fz * senseRadius * fraction;
    float clampedX = probeX;
    float clampedZ = probeZ;
    evolab::clampWorldPosition(clampedX, clampedZ, halfExtent, cellSize * 0.25f);
    const bool atBoundary =
        std::abs(clampedX - probeX) > 1.0e-3f || std::abs(clampedZ - probeZ) > 1.0e-3f;
    if (atBoundary || !isDeepWet(world, clampedX, clampedZ, cellSize, minDepth)) {
      return false;
    }
  }
  return true;
}

bool findDeepWetChemotaxisSite(const evolab::BarrenWorld& world, float cellSize, float heading,
                               float senseRadius, float cornucopiaDistance, float minDepth,
                               float& wx, float& wz) {
  const float half = worldHalfExtent(world, cellSize);
  const float surroundRadius = cellSize * kChemotaxisSurroundRadiusFactor;
  float bestMinDepth = -1.0f;
  float bestX = 0.0f;
  float bestZ = 0.0f;

  for (float x = -half; x <= half; x += evolab::kWorldCellSize * 0.5f) {
    for (float z = -half; z <= half; z += evolab::kWorldCellSize * 0.5f) {
      if (!isDeepWet(world, x, z, cellSize, minDepth)) {
        continue;
      }

      evolab::Organism probe =
          evolab::makeCampNomOrganism(999, x, z, 1.0f, 120, 0, cellSize);
      probe.heading = heading;
      probe.updateKinematics(world, cellSize, evolab::kTerrainHeightScale);

      if (!allCampNeuronsDeepWet(world, probe, cellSize, minDepth)) {
        continue;
      }

      const evolab::SkeletonNode* perceptor = probe.findNode(evolab::kCampPerceptorId);
      const evolab::SkeletonNode* hub = probe.findNode(evolab::kCampComputerId);
      if (perceptor == nullptr || hub == nullptr) {
        continue;
      }

      const float cornucopiaX = perceptor->worldX + std::sin(heading) * cornucopiaDistance;
      const float cornucopiaZ = perceptor->worldZ + std::cos(heading) * cornucopiaDistance;
      if (!isDeepWet(world, cornucopiaX, cornucopiaZ, cellSize, minDepth)) {
        continue;
      }
      if (!wetSegmentDeep(world, perceptor->worldX, perceptor->worldZ, cornucopiaX, cornucopiaZ,
                          cellSize, minDepth, kChemotaxisCorridorSamples)) {
        continue;
      }
      if (!wetDiscDeep(world, hub->worldX, hub->worldZ, surroundRadius, cellSize, minDepth)) {
        continue;
      }
      if (!forwardConeDeepWet(world, cellSize, half, perceptor->worldX, perceptor->worldZ, heading,
                              senseRadius, minDepth)) {
        continue;
      }

      float siteMinDepth = world.depthAtWorld(x, z, cellSize);
      for (const evolab::SkeletonNode& node : probe.nodes) {
        if (node.neuron == evolab::NeuronType::None) {
          continue;
        }
        siteMinDepth =
            std::min(siteMinDepth, world.depthAtWorld(node.worldX, node.worldZ, cellSize));
      }
      siteMinDepth = std::min(siteMinDepth, world.depthAtWorld(cornucopiaX, cornucopiaZ, cellSize));

      if (siteMinDepth > bestMinDepth) {
        bestMinDepth = siteMinDepth;
        bestX = x;
        bestZ = z;
      }
    }
  }

  if (bestMinDepth < minDepth) {
    return false;
  }
  wx = bestX;
  wz = bestZ;
  return true;
}

float hubDistanceTo(float rootX, float rootZ, float targetX, float targetZ) {
  const float dx = targetX - rootX;
  const float dz = targetZ - rootZ;
  return std::sqrt(dx * dx + dz * dz);
}

float normalizeHeading(float radians) {
  while (radians > 3.14159265f) {
    radians -= evolab::kTwoPi;
  }
  while (radians < -3.14159265f) {
    radians += evolab::kTwoPi;
  }
  return radians;
}

bool cornucopiaInPerceptorFocusCone(float perceptorX, float perceptorZ, float heading,
                                    float senseRadius, float foodX, float foodZ) {
  const float dx = foodX - perceptorX;
  const float dz = foodZ - perceptorZ;
  const float distSq = dx * dx + dz * dz;
  if (distSq < 1.0e-8f || distSq > senseRadius * senseRadius) {
    return false;
  }
  const float relBearing = normalizeHeading(std::atan2(dx, dz) - heading);
  return std::abs(relBearing) <= evolab::kPerceptorFocusHalfAngle;
}

float pickRandomBlindHeading(float perceptorX, float perceptorZ, float foodX, float foodZ,
                             float senseRadius, std::uint64_t seed) {
  for (int attempt = 0; attempt < 64; ++attempt) {
    std::mt19937 rng(evolab::chaosSpawnRng(seed, static_cast<std::uint64_t>(attempt) ^
                                                      0xB1ADC0DEULL));
    const float heading = evolab::chaosSpawnHeading(rng);
    if (!cornucopiaInPerceptorFocusCone(perceptorX, perceptorZ, heading, senseRadius, foodX,
                                        foodZ)) {
      return heading;
    }
  }
  return normalizeHeading(std::atan2(perceptorX - foodX, perceptorZ - foodZ));
}

evolab::BarrenWorld makeNurseryWorld(int seed, int resolution = 31) {
  return evolab::BarrenWorld(seed, resolution, evolab::Tide(evolab::TideConfig{}));
}

void tickNurseryEnvironment(evolab::BarrenWorld& world, evolab::EnergonField& energon,
                            float sunIntensity) {
  world.tick();
  energon.tick(world, sunIntensity, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
}

bool cornucopiaWorldPos(const evolab::EnergonField& energon, float& outX, float& outZ) {
  for (const evolab::EnergonBlob& blob : energon.blobs()) {
    if (blob.cornucopia && blob.remaining > 0) {
      outX = blob.x;
      outZ = blob.z;
      return true;
    }
  }
  return false;
}

void anchorCornucopiaToMouth(evolab::EnergonField& energon, const evolab::Organism& organism,
                             const evolab::BarrenWorld& world, float cellSize, float heightScale) {
  (void)energon;
  (void)organism;
  (void)world;
  (void)cellSize;
  (void)heightScale;
}

void finishNurseryEnergonAttachments(evolab::Organism& organism, evolab::BarrenWorld& world,
                                     evolab::EnergonField& energon) {
  energon.syncMouthAttachments({organism}, world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  energon.pruneMouthAnchors({organism}, evolab::kWorldCellSize);
}

void prepareFeedbagOracleAxons(evolab::Organism& organism) {
  for (evolab::NeuralAxon& axon : organism.neuralAxons) {
    axon.trustFeed = evolab::kTrustBaseline;
    axon.etaEnergy = 1.0f;
    axon.etaSignal = 1.0f;
  }
}

void runMouthStickyBeforeFeed(evolab::Organism& organism, evolab::BarrenWorld& world,
                              evolab::EnergonField& energon, float cellSize, float heightScale) {
  energon.applyMouthStickiness({organism}, cellSize);
  energon.syncMouthAttachments({organism}, world, cellSize, heightScale);
}

void tickSingleCamperFeedbagGraze(evolab::Organism& organism, evolab::BarrenWorld& world,
                                 evolab::EnergonField& energon, float cellSize,
                                 float heightScale, float sunIntensity) {
  const float halfExtent = worldHalfExtent(world, cellSize);
  energon.prepareSpatialQueries(cellSize, halfExtent, world);
  if (!organism.disableNurseryLocomotion) {
    organism.perceive(world, energon, cellSize, halfExtent, {organism}, world.tickCount(),
                      sunIntensity);
  }
  runMouthStickyBeforeFeed(organism, world, energon, cellSize, heightScale);
  organism.feed(energon, cellSize, world.tickCount());
  if (evolab::SkeletonNode* mouth = organism.findNode(evolab::kCampMouthId)) {
    evolab::tickMouthChewMetabolism(*mouth, mouth->ateThisTick);
  }
  evolab::conveyMouthDownstream(organism, energon, world.tickCount());
  evolab::digestMouthToComputer(organism);
  const evolab::OrganismTickContext ctx{world,     energon,     cellSize,
                                        heightScale, halfExtent, world.tickCount()};
  evolab::runOrganismPreAdvectHooks(organism, ctx);
  evolab::tickCoordinatorPhase(organism, world.tickCount());
  evolab::tickComputerPhase(organism, energon, world.tickCount());
  if (!organism.disableNurseryLocomotion) {
    organism.advectRoot(world, energon, cellSize, heightScale, halfExtent);
  }
  organism.metabolise(world, cellSize, heightScale);
  organism.tickNeuronViability(energon);
  energon.purgeDepletedBlobs();
  organism.transferEnergy(energon, cellSize, world.tickCount());
  organism.signal(energon, world.tickCount());
  organism.pruneNeuralAxons();
  energon.syncMouthAttachments({organism}, world, cellSize, heightScale);
  energon.pruneMouthAnchors({organism}, cellSize);
}

void tickSingleCamper(evolab::Organism& organism, evolab::BarrenWorld& world,
                      evolab::EnergonField& energon, float cellSize, float heightScale,
                      float sunIntensity) {
  const float halfExtent = worldHalfExtent(world, cellSize);
  energon.prepareSpatialQueries(cellSize, halfExtent, world);
  if (!organism.disableNurseryLocomotion) {
    organism.perceive(world, energon, cellSize, halfExtent, {organism}, world.tickCount(),
                      sunIntensity);
  }
  runMouthStickyBeforeFeed(organism, world, energon, cellSize, heightScale);
  organism.feed(energon, cellSize, world.tickCount());
  if (evolab::SkeletonNode* mouth = organism.findNode(evolab::kCampMouthId)) {
    evolab::tickMouthChewMetabolism(*mouth, mouth->ateThisTick);
  }
  evolab::conveyMouthDownstream(organism, energon, world.tickCount());
  evolab::digestMouthToComputer(organism);
  const evolab::OrganismTickContext ctx{world,     energon,     cellSize,
                                        heightScale, halfExtent, world.tickCount()};
  evolab::runOrganismPreAdvectHooks(organism, ctx);
  evolab::tickCoordinatorPhase(organism, world.tickCount());
  evolab::tickComputerPhase(organism, energon, world.tickCount());
  if (!organism.disableNurseryLocomotion) {
    organism.advectRoot(world, energon, cellSize, heightScale, halfExtent);
  }
  organism.metabolise(world, cellSize, heightScale);
  organism.tickNeuronViability(energon);
  organism.tickAxonTransitBasal();
  energon.purgeDepletedBlobs();
  organism.transferEnergy(energon, cellSize, world.tickCount());
  organism.signal(energon, world.tickCount());
  organism.pruneNeuralAxons();
  energon.syncMouthAttachments({organism}, world, cellSize, heightScale);
  energon.pruneMouthAnchors({organism}, cellSize);
}

void tickNurseryCamper(evolab::Organism& organism, evolab::BarrenWorld& world,
                       evolab::EnergonField& energon, float sunIntensity, bool feedbagGraze) {
  tickNurseryEnvironment(world, energon, sunIntensity);
  if (feedbagGraze) {
    if (evolab::SkeletonNode* mouth = organism.findNode(evolab::kCampMouthId)) {
      mouth->mouthChewPaused = false;
    }
    tickSingleCamperFeedbagGraze(organism, world, energon, evolab::kWorldCellSize,
                                 evolab::kTerrainHeightScale, sunIntensity);
  } else {
    tickSingleCamper(organism, world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                     sunIntensity);
  }
  finishNurseryEnergonAttachments(organism, world, energon);
}

bool mouthAteThisTick(const evolab::Organism& organism) {
  const evolab::SkeletonNode* mouth = organism.findNode(evolab::kCampMouthId);
  return mouth != nullptr && mouth->ateThisTick;
}


evolab::EnergonConfig cornucopiaFieldConfig() {
  evolab::EnergonConfig config;
  config.spawnRateMax = 0.0f;
  config.populationScaledRain = false;
  config.maxBlobs = 32;
  return config;
}

std::string cornucopiaTracePath(const char* stem) {
  std::filesystem::path path = std::filesystem::temp_directory_path();
  path /= stem;
  return path.string();
}

int countFieldOrigin(const evolab::EnergonField& field, evolab::EnergonOrigin origin) {
  int count = 0;
  for (const evolab::EnergonBlob& blob : field.blobs()) {
    if (blob.remaining > 0 && blob.origin == origin) {
      ++count;
    }
  }
  return count;
}

std::size_t hubSatiationThresholdBytes() {
  const float fillMin =
      (static_cast<float>(evolab::kComputerSatiationConfidence) - 0.5f) /
      static_cast<float>(evolab::kNeuronConfidenceMax);
  return static_cast<std::size_t>(fillMin * static_cast<float>(evolab::kComputerHubStoreMaxBytes));
}

struct CornucopiaNurserySite {
  float cornucopiaX = 0.0f;
  float cornucopiaZ = 0.0f;
  float startDist = 0.0f;
  float wetX = 0.0f;
  float wetZ = 0.0f;
  float spawnHubX = 0.0f;
  float spawnHubZ = 0.0f;
  float spawnFoodX = 0.0f;
  float spawnFoodZ = 0.0f;
};

struct NurseryRunMetrics {
  float minDist = 0.0f;
  float finalDist = 0.0f;
  float maxHubDrift = 0.0f;
  float maxFoodDrift = 0.0f;
  float maxAbsTideDelta = 0.0f;
  int totalBites = 0;
  int strokesPaid = 0;
  int baselineCloacaVents = 0;
  int totalCloacaVents = 0;
  std::size_t hubPeak = 0;
  int ticksRun = 0;
  int ticksWhileAlive = 0;
  int deathTick = -1;
  std::size_t hubAtDeath = 0;
  bool spawnedChild = false;
  std::uint32_t childId = 0;
  int teenagerTicksRun = 0;
  int teenagerBites = 0;
  bool teenagerAlive = false;
  std::uint64_t teenagerAgeTicks = 0;
};

std::size_t totalOrganismFuel(const evolab::Organism& organism) {
  return organism.totalFuelBytes();
}

void runNurseryTeenagerPhase(evolab::Organism& teenager, evolab::BarrenWorld& world,
                             evolab::EnergonField& energon, const CornucopiaNurserySite& site,
                             int maxTicks, float sunIntensity, NurseryRunMetrics& metrics) {
  teenager.disableTerrainThreatScan = true;
  teenager.disableNurseryLocomotion = false;
  bool feedbagGraze = false;

  for (int i = 0; i < maxTicks; ++i) {
    if (!teenager.alive) {
      break;
    }
    if (!feedbagGraze) {
      const evolab::SkeletonNode* mouth = teenager.findNode(evolab::kCampMouthId);
      float foodX = site.cornucopiaX;
      float foodZ = site.cornucopiaZ;
      cornucopiaWorldPos(energon, foodX, foodZ);
      if (mouth != nullptr) {
        const float biteReach =
            evolab::kWorldCellSize * evolab::kMouthContactRadiusFactor;
        const float mouthFoodDist = hubDistanceTo(mouth->worldX, mouth->worldZ, foodX, foodZ);
        if (mouthFoodDist <= biteReach) {
          feedbagGraze = true;
          teenager.disableNurseryLocomotion = true;
        }
      }
    } else {
      teenager.disableNurseryLocomotion = true;
    }
    tickNurseryCamper(teenager, world, energon, sunIntensity, feedbagGraze);
    metrics.maxAbsTideDelta =
        std::max(metrics.maxAbsTideDelta, std::abs(world.waterLevelDelta()));

    if (mouthAteThisTick(teenager)) {
      ++metrics.teenagerBites;
    }
    metrics.teenagerTicksRun = i + 1;
  }

  metrics.teenagerAlive = teenager.alive;
  if (world.tickCount() >= teenager.createdAtTick) {
    metrics.teenagerAgeTicks = world.tickCount() - teenager.createdAtTick;
  }
}

CornucopiaNurserySite setupCornucopiaNursery(evolab::BarrenWorld& world,
                                               evolab::EnergonField& energon,
                                               evolab::Organism& camper, float foodBearing,
                                               std::size_t spawnFuelBytes,
                                               std::optional<float> spawnHeading = std::nullopt) {
  CornucopiaNurserySite site;
  const float senseRadius = evolab::kWorldCellSize * evolab::kPerceptorSenseRadiusFactor;
  const float cornucopiaDistance = evolab::kWorldCellSize * 1.8f;
  const float minDepth = minChemotaxisDepth(evolab::kWorldCellSize);
  REQUIRE(findDeepWetChemotaxisSite(world, evolab::kWorldCellSize, foodBearing, senseRadius,
                                    cornucopiaDistance, minDepth, site.wetX, site.wetZ));

  camper = evolab::makeCampNomOrganism(1, site.wetX, site.wetZ, 1.0f, spawnFuelBytes, 0,
                                       evolab::kWorldCellSize);
  prepareFeedbagOracleAxons(camper);
  camper.alive = true;
  camper.disableTerrainThreatScan = true;
  camper.heading = spawnHeading.has_value() ? *spawnHeading : foodBearing;
  camper.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  const evolab::SkeletonNode* perceptor = camper.findNode(evolab::kCampPerceptorId);
  const evolab::SkeletonNode* actuator = camper.findNode(evolab::kCampActuatorId);
  REQUIRE(perceptor != nullptr);
  REQUIRE(actuator != nullptr);

  site.cornucopiaX = perceptor->worldX + std::sin(foodBearing) * cornucopiaDistance;
  site.cornucopiaZ = perceptor->worldZ + std::cos(foodBearing) * cornucopiaDistance;
  const float perceptorToFood =
      hubDistanceTo(perceptor->worldX, perceptor->worldZ, site.cornucopiaX, site.cornucopiaZ);
  REQUIRE(perceptorToFood <= senseRadius);
  REQUIRE(allCampNeuronsDeepWet(world, camper, evolab::kWorldCellSize, minDepth));
  REQUIRE(isDeepWet(world, site.cornucopiaX, site.cornucopiaZ, evolab::kWorldCellSize, minDepth));
  REQUIRE(wetSegmentDeep(world, perceptor->worldX, perceptor->worldZ, site.cornucopiaX,
                         site.cornucopiaZ, evolab::kWorldCellSize, minDepth,
                         kChemotaxisCorridorSamples));
  REQUIRE(isDeepWet(world, actuator->worldX, actuator->worldZ, evolab::kWorldCellSize, minDepth));

  energon.injectBlob(evolab::makeCornucopiaBlob(site.cornucopiaX, site.cornucopiaZ, 0x42));
  REQUIRE(energon.activeCount() == 1);

  site.startDist =
      hubDistanceTo(camper.rootWorldX(), camper.rootWorldZ(), site.cornucopiaX, site.cornucopiaZ);
  site.spawnHubX = camper.rootWorldX();
  site.spawnHubZ = camper.rootWorldZ();
  site.spawnFoodX = site.cornucopiaX;
  site.spawnFoodZ = site.cornucopiaZ;
  return site;
}

NurseryRunMetrics runCornucopiaNursery(evolab::BarrenWorld& world, evolab::EnergonField& energon,
                                       evolab::Organism& camper, const CornucopiaNurserySite& site,
                                       int maxTicks, float sunIntensity, bool enableParthenogenesis,
                                       bool latchCornucopiaAfterFeed, evolab::CampTraceLog* trace,
                                       evolab::Organism* spawnedChildOut = nullptr,
                                       int teenagerTicksAfterBirth = 0) {
  NurseryRunMetrics metrics;
  metrics.minDist = site.startDist;

  std::vector<evolab::Organism> population;
  population.push_back(camper);
  std::uint32_t nextOrganismId = 2;
  evolab::ParthenogenesisPassOptions parthenogenesisOptions;
  parthenogenesisOptions.structuralRateOverride = 0.0f;

  int cloacaCount = countFieldOrigin(energon, evolab::EnergonOrigin::Cloaca);
  const std::size_t hubStart = camper.computerHubFuelBytes();
  bool feedbagGraze = !latchCornucopiaAfterFeed;

  for (int i = 0; i < maxTicks; ++i) {
    evolab::Organism& parent = population.front();
    const std::uint64_t simTick = world.tickCount();
    if (latchCornucopiaAfterFeed && !feedbagGraze) {
      const evolab::SkeletonNode* mouth = parent.findNode(evolab::kCampMouthId);
      float foodX = site.cornucopiaX;
      float foodZ = site.cornucopiaZ;
      cornucopiaWorldPos(energon, foodX, foodZ);
      if (mouth != nullptr) {
        const float biteReach =
            evolab::kWorldCellSize * evolab::kMouthContactRadiusFactor;
        const float mouthFoodDist = hubDistanceTo(mouth->worldX, mouth->worldZ, foodX, foodZ);
        if (mouthFoodDist <= biteReach) {
          feedbagGraze = true;
          parent.disableNurseryLocomotion = true;
        }
      }
    } else if (feedbagGraze) {
      parent.disableNurseryLocomotion = true;
    } else {
      parent.disableNurseryLocomotion = false;
    }
    tickNurseryCamper(parent, world, energon, sunIntensity, feedbagGraze);

    float foodX = site.cornucopiaX;
    float foodZ = site.cornucopiaZ;
    cornucopiaWorldPos(energon, foodX, foodZ);
    const float dist = hubDistanceTo(parent.rootWorldX(), parent.rootWorldZ(), foodX, foodZ);
    metrics.minDist = std::min(metrics.minDist, dist);
    metrics.hubPeak = std::max(metrics.hubPeak, parent.computerHubFuelBytes());
    metrics.maxHubDrift = std::max(
        metrics.maxHubDrift,
        hubDistanceTo(parent.rootWorldX(), parent.rootWorldZ(), site.spawnHubX, site.spawnHubZ));
    metrics.maxFoodDrift =
        std::max(metrics.maxFoodDrift, hubDistanceTo(foodX, foodZ, site.spawnFoodX, site.spawnFoodZ));
    metrics.maxAbsTideDelta =
        std::max(metrics.maxAbsTideDelta, std::abs(world.waterLevelDelta()));

    if (mouthAteThisTick(parent)) {
      ++metrics.totalBites;
    }
    if (parent.lastStrokeBytesPaid > 0) {
      ++metrics.strokesPaid;
    }
    if (parent.lastHubSignalExpelledThisTick &&
        parent.lastCloacaBandExpelled == evolab::CloacaBand::Baseline) {
      ++metrics.baselineCloacaVents;
    }

    const int newCloacaCount = countFieldOrigin(energon, evolab::EnergonOrigin::Cloaca);
    metrics.totalCloacaVents += newCloacaCount - cloacaCount;
    cloacaCount = newCloacaCount;

    if (trace != nullptr) {
      trace->recordTick(simTick, parent, foodX, foodZ, sunIntensity);
    }

    metrics.ticksRun = i + 1;
    if (parent.alive) {
      metrics.ticksWhileAlive = i + 1;
    } else if (metrics.deathTick < 0) {
      metrics.deathTick = i + 1;
      metrics.hubAtDeath = parent.computerHubFuelBytes();
    }

    if (enableParthenogenesis && !metrics.spawnedChild && metrics.totalCloacaVents >= 1 &&
        evolab::eligibleForParthenogenesis(parent, world, evolab::kWorldCellSize, simTick)) {
      evolab::tickParthenogenesisPass(population, world, evolab::kWorldCellSize,
                                      evolab::kTerrainHeightScale, simTick, nextOrganismId,
                                      parthenogenesisOptions);
      if (population.size() > 1) {
        metrics.spawnedChild = true;
        metrics.childId = population.back().id;
        evolab::Organism& teenager = population.back();
        if (teenagerTicksAfterBirth > 0) {
          runNurseryTeenagerPhase(teenager, world, energon, site, teenagerTicksAfterBirth,
                                  sunIntensity, metrics);
        }
        if (spawnedChildOut != nullptr) {
          *spawnedChildOut = teenager;
        }
      }
      break;
    }

    if (metrics.spawnedChild) {
      break;
    }
  }

  camper = population.front();
  float finalFoodX = site.cornucopiaX;
  float finalFoodZ = site.cornucopiaZ;
  cornucopiaWorldPos(energon, finalFoodX, finalFoodZ);
  metrics.finalDist =
      hubDistanceTo(camper.rootWorldX(), camper.rootWorldZ(), finalFoodX, finalFoodZ);
  if (metrics.hubPeak < hubStart) {
    metrics.hubPeak = hubStart;
  }

  return metrics;
}

void logNurseryMetrics(const NurseryRunMetrics& metrics, const std::string& tracePath) {
  INFO("trace log: " << tracePath);
  INFO("ticksRun=" << metrics.ticksRun << " startDist=minDist context in caller"
                   << " minDist=" << metrics.minDist << " finalDist=" << metrics.finalDist
                   << " maxHubDrift=" << metrics.maxHubDrift
                   << " maxFoodDrift=" << metrics.maxFoodDrift
                   << " maxAbsTideDelta=" << metrics.maxAbsTideDelta
                   << " deathTick=" << metrics.deathTick
                   << " ticksWhileAlive=" << metrics.ticksWhileAlive
                   << " hubAtDeath=" << metrics.hubAtDeath
                   << " bites=" << metrics.totalBites << " strokesPaid=" << metrics.strokesPaid
                   << " hubPeak=" << metrics.hubPeak
                   << " hubSatThreshold=" << hubSatiationThresholdBytes()
                   << " baselineVents=" << metrics.baselineCloacaVents
                   << " totalCloacaVents=" << metrics.totalCloacaVents
                   << " spawnedChild=" << metrics.spawnedChild
                   << " teenagerTicks=" << metrics.teenagerTicksRun
                   << " teenagerBites=" << metrics.teenagerBites
                   << " teenagerAlive=" << metrics.teenagerAlive
                   << " teenagerAge=" << metrics.teenagerAgeTicks);
}

void requireNurseryDriftHarness(const NurseryRunMetrics& metrics, const evolab::Organism& camper) {
  INFO("drift reassessment camper.alive=" << camper.alive << " ticksRun=" << metrics.ticksRun
                                         << " ticksWhileAlive=" << metrics.ticksWhileAlive
                                         << " deathTick=" << metrics.deathTick
                                         << " bites=" << metrics.totalBites
                                         << " hubPeak=" << metrics.hubPeak
                                         << " maxHubDrift=" << metrics.maxHubDrift
                                         << " maxFoodDrift=" << metrics.maxFoodDrift
                                         << " maxAbsTideDelta=" << metrics.maxAbsTideDelta
                                         << " cloacaVents=" << metrics.totalCloacaVents);
  REQUIRE(metrics.maxAbsTideDelta > 0.001f);
  REQUIRE(metrics.maxHubDrift + metrics.maxFoodDrift > 0.001f);
  INFO("strokesPaid=" << metrics.strokesPaid);
  REQUIRE(metrics.strokesPaid > 0);
}

}  // namespace

TEST_CASE("cornucopia energon never depletes when bitten", "[camper][cornucopia]") {
  evolab::BarrenWorld world(7, 32);
  evolab::EnergonField energon(1, cornucopiaFieldConfig());
  energon.injectBlob(evolab::makeCornucopiaBlob(0.0f, 1.0f, 0x55));

  REQUIRE(energon.activeCount() == 1);
  const std::uint32_t blobId = energon.blobs().front().id;
  const std::uint16_t startRemaining = energon.blobs().front().remaining;

  for (int i = 0; i < 20; ++i) {
    const evolab::EnergonBiteResult bite = energon.biteAt(blobId, 0.0f, 1.0f);
    REQUIRE(bite.tookByte);
  }

  REQUIRE(energon.blobs().front().remaining == startRemaining);
  REQUIRE(energon.blobs().front().cornucopia);
}

TEST_CASE("single camper chemotaxis toward eternal cornucopia", "[camper][cornucopia]") {
  evolab::BarrenWorld world = makeNurseryWorld(31, 32);
  evolab::EnergonField energon(42, cornucopiaFieldConfig());
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, evolab::kTicksPerStemCellDay, 0,
                                  evolab::kWorldCellSize);
  const CornucopiaNurserySite site = setupCornucopiaNursery(
      world, energon, camper, 0.55f, evolab::kTicksPerStemCellDay);

  evolab::CampTraceLog trace;
  const std::string tracePath = cornucopiaTracePath("camp-chemotaxis.trace");
  REQUIRE(trace.open(tracePath));
  trace.writeHeader(42, site.cornucopiaX, site.cornucopiaZ);

  NurseryRunMetrics metrics =
      runCornucopiaNursery(world, energon, camper, site, 256, 1.0f, false, true, &trace);
  trace.close();
  logNurseryMetrics(metrics, tracePath);

  INFO("startDist=" << site.startDist);
  INFO("minDist=" << metrics.minDist);
  INFO("finalDist=" << metrics.finalDist);
  INFO("totalBites=" << metrics.totalBites);

  REQUIRE(energon.blobs().front().cornucopia);
  REQUIRE(energon.blobs().front().remaining == evolab::kEnergonMaxBytesPerBlob);
  REQUIRE(metrics.minDist <= site.startDist);
  REQUIRE(metrics.totalBites > 0);
  REQUIRE(metrics.strokesPaid > 0);
}

TEST_CASE("nursery camper random facing homes on cornucopia via mouth taste",
          "[camper][cornucopia][nursery][blind][long]") {
  constexpr float foodBearing = 0.55f;
  constexpr std::uint64_t kBlindFacingSeed = 0xB1ADFACE42u;
  const float senseRadius = evolab::kWorldCellSize * evolab::kPerceptorSenseRadiusFactor;

  evolab::BarrenWorld world = makeNurseryWorld(31, 32);
  evolab::EnergonField energon(44, cornucopiaFieldConfig());
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, evolab::kTicksPerStemCellDay * 2, 0,
                                  evolab::kWorldCellSize);

  CornucopiaNurserySite site = setupCornucopiaNursery(
      world, energon, camper, foodBearing, evolab::kTicksPerStemCellDay * 2);
  const evolab::SkeletonNode* perceptor = camper.findNode(evolab::kCampPerceptorId);
  REQUIRE(perceptor != nullptr);

  const float blindHeading =
      pickRandomBlindHeading(perceptor->worldX, perceptor->worldZ, site.cornucopiaX,
                             site.cornucopiaZ, senseRadius, kBlindFacingSeed);
  camper.heading = blindHeading;
  camper.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  const evolab::SkeletonNode* respawnedPerceptor = camper.findNode(evolab::kCampPerceptorId);
  REQUIRE(respawnedPerceptor != nullptr);
  REQUIRE_FALSE(cornucopiaInPerceptorFocusCone(respawnedPerceptor->worldX,
                                               respawnedPerceptor->worldZ, camper.heading,
                                               senseRadius, site.cornucopiaX, site.cornucopiaZ));
  REQUIRE(hubDistanceTo(respawnedPerceptor->worldX, respawnedPerceptor->worldZ, site.cornucopiaX,
                          site.cornucopiaZ) <= senseRadius);

  NurseryRunMetrics metrics = runCornucopiaNursery(
      world, energon, camper, site, kNurseryVisualDaysForPuke, 1.0f, false, true, nullptr);

  INFO("spawnHeading=" << blindHeading);
  INFO("foodBearing=" << foodBearing);
  INFO("startDist=" << site.startDist);
  INFO("minDist=" << metrics.minDist);
  INFO("finalDist=" << metrics.finalDist);
  INFO("totalBites=" << metrics.totalBites);
  INFO("deathTick=" << metrics.deathTick);
  INFO("hubPeak=" << metrics.hubPeak);

  // Spawn is outside the forward gaze cone but inside taste radius — P is blind, M taste buds
  // provide omnidirectional bearing + temporal gradient for crawl homing.
  REQUIRE(metrics.minDist < site.startDist);
  REQUIRE(metrics.totalBites > 0);
  REQUIRE(camper.alive);
  REQUIRE(metrics.hubPeak >= hubSatiationThresholdBytes() / 4u);
}

TEST_CASE("nursery camper crawl eat puke without split", "[camper][cornucopia][nursery][drift][long]") {
  evolab::BarrenWorld world = makeNurseryWorld(31, 32);
  evolab::EnergonField energon(43, cornucopiaFieldConfig());
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, evolab::kTicksPerStemCellDay, 0,
                                  evolab::kWorldCellSize);
  const CornucopiaNurserySite site = setupCornucopiaNursery(
      world, energon, camper, 0.55f, evolab::kTicksPerStemCellDay * 2);

  evolab::CampTraceLog trace;
  const std::string tracePath = cornucopiaTracePath("camp-crawl-eat-puke.trace");
  REQUIRE(trace.open(tracePath));
  trace.writeHeader(43, site.cornucopiaX, site.cornucopiaZ);

  NurseryRunMetrics metrics = runCornucopiaNursery(world, energon, camper, site,
                                                   kNurseryVisualDaysForPuke, 1.0f, false, true,
                                                   &trace);
  trace.close();
  logNurseryMetrics(metrics, tracePath);

  INFO("startDist=" << site.startDist);
  requireNurseryDriftHarness(metrics, camper);
  REQUIRE(camper.alive);
  REQUIRE(metrics.finalDist < site.startDist);
  REQUIRE(metrics.totalBites >= 100);
  REQUIRE(metrics.hubPeak >= hubSatiationThresholdBytes());
  REQUIRE(metrics.totalCloacaVents >= 1);
  REQUIRE_FALSE(camper.lastParthenogenesisSpawned);
  REQUIRE(camper.offspringSpawnedCount == 0);
  REQUIRE(energon.blobs().front().cornucopia);
}

TEST_CASE("nursery camper crawl eat puke reproduce and teenager survives",
          "[camper][cornucopia][nursery][parthenogenesis][drift][long]") {
  evolab::BarrenWorld world = makeNurseryWorld(31, 32);
  evolab::EnergonField energon(43, cornucopiaFieldConfig());
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, evolab::kTicksPerStemCellDay, 0,
                                  evolab::kWorldCellSize);
  const CornucopiaNurserySite site = setupCornucopiaNursery(
      world, energon, camper, 0.55f, evolab::kTicksPerStemCellDay * 2);

  evolab::CampTraceLog trace;
  const std::string tracePath = cornucopiaTracePath("camp-nursery-teenager.trace");
  REQUIRE(trace.open(tracePath));
  trace.writeHeader(43, site.cornucopiaX, site.cornucopiaZ);

  evolab::Organism teenager;
  NurseryRunMetrics metrics = runCornucopiaNursery(
      world, energon, camper, site, kNurseryVisualDaysForPuke, 1.0f, true, true, &trace,
      &teenager, kNurseryTeenagerTicks);
  trace.close();
  logNurseryMetrics(metrics, tracePath);

  INFO("startDist=" << site.startDist);
  INFO("parthenogenesisRequiredHub=" << evolab::estimateParthenogenesisRequiredHubBytes());
  INFO("hubAtSpawn=" << camper.computerHubFuelBytes());
  INFO("teenagerFuel=" << totalOrganismFuel(teenager));

  requireNurseryDriftHarness(metrics, camper);
  REQUIRE(camper.alive);
  REQUIRE(metrics.spawnedChild);
  REQUIRE(metrics.childId == teenager.id);
  REQUIRE(metrics.finalDist < site.startDist);
  REQUIRE(metrics.totalBites >= 100);
  REQUIRE(metrics.hubPeak >= evolab::estimateParthenogenesisRequiredHubBytes());
  REQUIRE(metrics.totalCloacaVents >= 1);
  REQUIRE(camper.offspringSpawnedCount == 1);
  REQUIRE(camper.lastParthenogenesisSpawned);

  REQUIRE(teenager.alive);
  REQUIRE(teenager.isCampNom());
  REQUIRE(evolab::organismHasCampTopology(teenager));
  REQUIRE(metrics.teenagerAlive);
  REQUIRE(metrics.teenagerAgeTicks >= evolab::kParthenogenesisMinAgeTicks);
  REQUIRE(totalOrganismFuel(teenager) > 0);
  REQUIRE_FALSE(teenager.feedbagOracle);
  REQUIRE(energon.blobs().front().cornucopia);
}

TEST_CASE("nursery camper full up till puke and split", "[camper][cornucopia][nursery][drift][long]") {
  evolab::BarrenWorld world = makeNurseryWorld(31, 32);
  evolab::EnergonField energon(43, cornucopiaFieldConfig());
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, evolab::kTicksPerStemCellDay, 0,
                                  evolab::kWorldCellSize);
  const CornucopiaNurserySite site = setupCornucopiaNursery(
      world, energon, camper, 0.55f, evolab::kTicksPerStemCellDay * 2);

  evolab::CampTraceLog trace;
  const std::string tracePath = cornucopiaTracePath("camp-puke-and-split.trace");
  REQUIRE(trace.open(tracePath));
  trace.writeHeader(43, site.cornucopiaX, site.cornucopiaZ);

  NurseryRunMetrics grazeMetrics = runCornucopiaNursery(
      world, energon, camper, site, kNurseryVisualDaysForPuke, 1.0f, false, true, &trace);

  requireNurseryDriftHarness(grazeMetrics, camper);
  REQUIRE(camper.alive);
  REQUIRE(grazeMetrics.totalCloacaVents >= 1);
  REQUIRE(grazeMetrics.hubPeak >= hubSatiationThresholdBytes());
  REQUIRE(camper.computerHubFuelBytes() >= evolab::estimateParthenogenesisRequiredHubBytes());

  std::vector<evolab::Organism> population;
  population.push_back(camper);
  std::uint32_t nextOrganismId = 2;
  evolab::ParthenogenesisPassOptions options;
  options.structuralRateOverride = 0.0f;
  const evolab::ParthenogenesisResult birth = evolab::attemptParthenogenesis(
      population.front(), world, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
      world.tickCount(), nextOrganismId, options);

  trace.close();
  logNurseryMetrics(grazeMetrics, tracePath);

  INFO("startDist=" << site.startDist);
  INFO("parthenogenesisRequiredHub=" << evolab::estimateParthenogenesisRequiredHubBytes());
  INFO("hubAtSplitAttempt=" << population.front().computerHubFuelBytes());

  REQUIRE(birth.spawned);
  REQUIRE(birth.child.alive);
  REQUIRE(birth.child.isCampNom());
  REQUIRE(population.front().lastParthenogenesisSpawned);
  REQUIRE(population.front().offspringSpawnedCount == 1);
  REQUIRE(grazeMetrics.finalDist < site.startDist);
  REQUIRE(grazeMetrics.totalBites >= 100);
  REQUIRE(energon.blobs().front().cornucopia);
}
