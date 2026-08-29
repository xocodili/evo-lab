#include "sim/BarrenWorld.hpp"
#include "sim/CampTraceLog.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CloacaSignal.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonRain.hpp"
#include "sim/EnergonString.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/NeuronTick.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismParthenogenesis.hpp"
#include "sim/PerceptorFocus.hpp"
#include "sim/Tide.hpp"
#include "sim/TideAdvection.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <filesystem>
#include <string>
#include <algorithm>
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

void prepareFeedbagOracleAxons(evolab::Organism& organism) {
  for (evolab::NeuralAxon& axon : organism.neuralAxons) {
    axon.trustFeed = evolab::kTrustBaseline;
    axon.etaEnergy = 1.0f;
    axon.etaSignal = 1.0f;
  }
}

void tickSingleCamperFeedbagGraze(evolab::Organism& organism, evolab::BarrenWorld& world,
                                 evolab::EnergonField& energon, float cellSize,
                                 float heightScale, float sunIntensity) {
  const float halfExtent = worldHalfExtent(world, cellSize);
  energon.prepareSpatialQueries(cellSize, halfExtent);
  organism.feed(energon, cellSize, world.tickCount());
  organism.runDigestAndComputer(energon, world.tickCount());
  organism.computerFeedGain = 1.0f;
  const evolab::OrganismTickContext ctx{world,     energon,     cellSize,
                                        heightScale, halfExtent, world.tickCount()};
  evolab::runOrganismPreAdvectHooks(organism, ctx);
  if (!organism.disableNurseryLocomotion) {
    organism.advectRoot(world, energon, cellSize, heightScale, halfExtent);
  }
  organism.metabolise(world, cellSize, heightScale);
  organism.tickNeuronViability(energon);
  energon.purgeDepletedBlobs();
  organism.transferEnergy(energon, cellSize, world.tickCount());
  organism.signal(energon, world.tickCount());
  organism.pruneNeuralAxons();
}

void tickSingleCamper(evolab::Organism& organism, evolab::BarrenWorld& world,
                      evolab::EnergonField& energon, float cellSize, float heightScale,
                      float sunIntensity) {
  const float halfExtent = worldHalfExtent(world, cellSize);
  energon.prepareSpatialQueries(cellSize, halfExtent);
  organism.perceive(world, energon, cellSize, halfExtent, {organism}, world.tickCount(),
                    sunIntensity);
  organism.feed(energon, cellSize, world.tickCount());
  organism.runDigestAndComputer(energon, world.tickCount());
  const evolab::OrganismTickContext ctx{world,     energon,     cellSize,
                                        heightScale, halfExtent, world.tickCount()};
  evolab::runOrganismPreAdvectHooks(organism, ctx);
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
  world.tick();
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
};

struct NurseryRunMetrics {
  float minDist = 0.0f;
  float finalDist = 0.0f;
  int totalBites = 0;
  int strokesPaid = 0;
  int baselineCloacaVents = 0;
  int totalCloacaVents = 0;
  std::size_t hubPeak = 0;
  int ticksRun = 0;
  bool spawnedChild = false;
  std::uint32_t childId = 0;
  int teenagerTicksRun = 0;
  int teenagerBites = 0;
  bool teenagerAlive = false;
  std::uint64_t teenagerAgeTicks = 0;
};

std::size_t totalOrganismFuel(const evolab::Organism& organism) {
  std::size_t total = organism.bodyStorage.size();
  for (const evolab::SkeletonNode& node : organism.nodes) {
    total += node.store.size();
  }
  return total;
}

void runNurseryTeenagerPhase(evolab::Organism& teenager, evolab::BarrenWorld& world,
                             evolab::EnergonField& energon, const CornucopiaNurserySite& site,
                             int maxTicks, float sunIntensity, NurseryRunMetrics& metrics) {
  teenager.disableTideAdvection = true;
  teenager.disableTerrainThreatScan = true;
  teenager.disableNurseryLocomotion = false;
  bool feedbagGraze = false;

  for (int i = 0; i < maxTicks; ++i) {
    if (!teenager.alive) {
      break;
    }
    if (!feedbagGraze) {
      const evolab::SkeletonNode* mouth = teenager.findNode(evolab::kCampMouthId);
      if (mouth != nullptr) {
        const float biteReach =
            evolab::kWorldCellSize * evolab::kMouthContactRadiusFactor;
        const float mouthFoodDist =
            hubDistanceTo(mouth->worldX, mouth->worldZ, site.cornucopiaX, site.cornucopiaZ);
        if (mouthFoodDist <= biteReach) {
          feedbagGraze = true;
          teenager.disableNurseryLocomotion = true;
        }
      }
    }
    if (feedbagGraze) {
      if (evolab::SkeletonNode* mouth = teenager.findNode(evolab::kCampMouthId)) {
        mouth->mouthChewPaused = false;
      }
      world.tick();
      energon.tick(world, sunIntensity, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
      tickSingleCamperFeedbagGraze(teenager, world, energon, evolab::kWorldCellSize,
                                   evolab::kTerrainHeightScale, sunIntensity);
    } else {
      tickSingleCamper(teenager, world, energon, evolab::kWorldCellSize,
                       evolab::kTerrainHeightScale, sunIntensity);
    }

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
                                               std::size_t spawnFuelBytes) {
  CornucopiaNurserySite site;
  const float senseRadius = evolab::kWorldCellSize * evolab::kPerceptorSenseRadiusFactor;
  const float cornucopiaDistance = evolab::kWorldCellSize * 2.5f;
  const float minDepth = minChemotaxisDepth(evolab::kWorldCellSize);
  REQUIRE(findDeepWetChemotaxisSite(world, evolab::kWorldCellSize, foodBearing, senseRadius,
                                    cornucopiaDistance, minDepth, site.wetX, site.wetZ));

  camper = evolab::makeCampNomOrganism(1, site.wetX, site.wetZ, 1.0f, spawnFuelBytes, 0,
                                       evolab::kWorldCellSize);
  prepareFeedbagOracleAxons(camper);
  camper.alive = true;
  camper.disableTideAdvection = true;
  camper.disableTerrainThreatScan = true;
  camper.heading = foodBearing;
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
  const std::size_t hubStart = camper.bodyStorage.size();
  bool feedbagGraze = !latchCornucopiaAfterFeed;

  for (int i = 0; i < maxTicks; ++i) {
    evolab::Organism& parent = population.front();
    const std::uint64_t simTick = world.tickCount();
    if (latchCornucopiaAfterFeed && !feedbagGraze) {
      const evolab::SkeletonNode* mouth = parent.findNode(evolab::kCampMouthId);
      if (mouth != nullptr) {
        const float biteReach =
            evolab::kWorldCellSize * evolab::kMouthContactRadiusFactor;
        const float mouthFoodDist =
            hubDistanceTo(mouth->worldX, mouth->worldZ, site.cornucopiaX, site.cornucopiaZ);
        if (mouthFoodDist <= biteReach) {
          feedbagGraze = true;
          parent.disableNurseryLocomotion = true;
        }
      }
    }
    if (feedbagGraze) {
      if (evolab::SkeletonNode* mouth = parent.findNode(evolab::kCampMouthId)) {
        mouth->mouthChewPaused = false;
      }
      world.tick();
      energon.tick(world, sunIntensity, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
      tickSingleCamperFeedbagGraze(parent, world, energon, evolab::kWorldCellSize,
                                   evolab::kTerrainHeightScale, sunIntensity);
    } else {
      parent.disableNurseryLocomotion = false;
      tickSingleCamper(parent, world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale,
                       sunIntensity);
    }

    const float dist =
        hubDistanceTo(parent.rootWorldX(), parent.rootWorldZ(), site.cornucopiaX, site.cornucopiaZ);
    metrics.minDist = std::min(metrics.minDist, dist);
    metrics.hubPeak = std::max(metrics.hubPeak, parent.bodyStorage.size());

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
      trace->recordTick(simTick, parent, site.cornucopiaX, site.cornucopiaZ, sunIntensity);
    }

    metrics.ticksRun = i + 1;

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
  metrics.finalDist =
      hubDistanceTo(camper.rootWorldX(), camper.rootWorldZ(), site.cornucopiaX, site.cornucopiaZ);
  if (metrics.hubPeak < hubStart) {
    metrics.hubPeak = hubStart;
  }

  return metrics;
}

void logNurseryMetrics(const NurseryRunMetrics& metrics, const std::string& tracePath) {
  INFO("trace log: " << tracePath);
  INFO("ticksRun=" << metrics.ticksRun << " startDist=minDist context in caller"
                   << " minDist=" << metrics.minDist << " finalDist=" << metrics.finalDist
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
  evolab::TideConfig tideConfig;
  tideConfig.amplitude = 0.0f;
  evolab::BarrenWorld world(31, 32, evolab::Tide(tideConfig));
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
      runCornucopiaNursery(world, energon, camper, site, 256, 1.0f, false, false, &trace);
  trace.close();
  logNurseryMetrics(metrics, tracePath);

  INFO("startDist=" << site.startDist);

  REQUIRE(energon.blobs().front().cornucopia);
  REQUIRE(energon.blobs().front().remaining == evolab::kEnergonMaxBytesPerBlob);
  REQUIRE(metrics.finalDist < site.startDist);
}

TEST_CASE("nursery camper crawl eat puke without split", "[camper][cornucopia][nursery][long]") {
  evolab::TideConfig tideConfig;
  tideConfig.amplitude = 0.0f;
  evolab::BarrenWorld world(31, 32, evolab::Tide(tideConfig));
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
          "[camper][cornucopia][nursery][parthenogenesis][long]") {
  evolab::TideConfig tideConfig;
  tideConfig.amplitude = 0.0f;
  evolab::BarrenWorld world(31, 32, evolab::Tide(tideConfig));
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
  INFO("hubAtSpawn=" << camper.bodyStorage.size());
  INFO("teenagerFuel=" << totalOrganismFuel(teenager));

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

TEST_CASE("nursery camper full up till puke and split", "[camper][cornucopia][nursery][long]") {
  evolab::TideConfig tideConfig;
  tideConfig.amplitude = 0.0f;
  evolab::BarrenWorld world(31, 32, evolab::Tide(tideConfig));
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

  REQUIRE(camper.alive);
  REQUIRE(grazeMetrics.totalCloacaVents >= 1);
  REQUIRE(grazeMetrics.hubPeak >= hubSatiationThresholdBytes());
  REQUIRE(camper.bodyStorage.size() >= evolab::estimateParthenogenesisRequiredHubBytes());

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
  INFO("hubAtSplitAttempt=" << population.front().bodyStorage.size());

  REQUIRE(birth.spawned);
  REQUIRE(birth.child.alive);
  REQUIRE(birth.child.isCampNom());
  REQUIRE(population.front().lastParthenogenesisSpawned);
  REQUIRE(population.front().offspringSpawnedCount == 1);
  REQUIRE(grazeMetrics.finalDist < site.startDist);
  REQUIRE(grazeMetrics.totalBites >= 100);
  REQUIRE(energon.blobs().front().cornucopia);
}
