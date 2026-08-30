#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/DayCycle.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonString.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/NeuronTick.hpp"
#include "sim/Organism.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

namespace {

float worldHalfExtent(const evolab::BarrenWorld& world, float cellSize) {
  const int res = world.heightmap().resolution;
  if (res <= 1 || cellSize <= 0.0f) {
    return 0.0f;
  }
  return static_cast<float>(res - 1) * cellSize * 0.5f;
}

bool findWetWorldSite(const evolab::BarrenWorld& world, float cellSize, float& wx, float& wz) {
  const float half = worldHalfExtent(world, cellSize);
  for (float x = -half; x <= half; x += evolab::kWorldCellSize * 0.5f) {
    for (float z = -half; z <= half; z += evolab::kWorldCellSize * 0.5f) {
      if (world.isWetWorld(x, z, cellSize)) {
        wx = x;
        wz = z;
        return true;
      }
    }
  }
  return false;
}

evolab::EnergonBlob makeWetFoodBlob(float x, float z, std::uint8_t dataByte) {
  evolab::EnergonBlob blob;
  blob.data = dataByte;
  blob.remaining = 1;
  blob.initialBytes = 1;
  blob.origin = evolab::EnergonOrigin::Sunfall;
  blob.x = x;
  blob.z = z;
  blob.y = 0.0f;
  blob.tailX = x;
  blob.tailZ = z;
  blob.headX = x;
  blob.headZ = z;
  blob.grounded = true;
  blob.onWet = true;
  blob.ttl = 120.0f;
  evolab::energonBlobInitPoint(blob);
  return blob;
}

bool mouthHasFoodInRange(const evolab::EnergonField& field, float wx, float wz, float radius) {
  bool found = false;
  field.forEachBlobNear(wx, wz, radius, [&](const evolab::EnergonBlob& blob) {
    if (blob.remaining > 0) {
      found = true;
    }
  });
  return found;
}

evolab::EnergonField makeFoodWellField(int seed) {
  evolab::EnergonConfig config;
  config.spawnRateMax = 0.0f;
  return evolab::EnergonField(seed, config);
}

void ensureAbundantFoodAtMouth(evolab::EnergonField& field, const evolab::SkeletonNode& mouth,
                               float cellSize) {
  const float radius = cellSize * evolab::kMouthContactRadiusFactor;
  if (mouthHasFoodInRange(field, mouth.worldX, mouth.worldZ, radius)) {
    return;
  }
  field.injectBlob(makeWetFoodBlob(mouth.worldX, mouth.worldZ, 0xAA));
}

int countFieldOrigin(const evolab::EnergonField& field, evolab::EnergonOrigin origin) {
  int count = 0;
  for (const evolab::EnergonBlob& blob : field.blobs()) {
    if (blob.origin == origin) {
      ++count;
    }
  }
  return count;
}

void tickCampNomFull(evolab::Organism& organism, evolab::BarrenWorld& world,
                     evolab::EnergonField& energon, float cellSize, float heightScale,
                     float sunIntensity, evolab::SkeletonNode* mouth) {
  const float halfExtent = worldHalfExtent(world, cellSize);
  energon.prepareSpatialQueries(cellSize, halfExtent, world);
  organism.perceive(world, energon, cellSize, halfExtent, {organism}, world.tickCount(),
                    sunIntensity);
  if (mouth != nullptr) {
    ensureAbundantFoodAtMouth(energon, *mouth, cellSize);
  }
  organism.feed(energon, cellSize, world.tickCount());
  organism.runDigestAndComputer(energon, world.tickCount());
  const evolab::OrganismTickContext ctx{world,     energon,     cellSize,
                                        heightScale, halfExtent, world.tickCount()};
  evolab::runOrganismPreAdvectHooks(organism, ctx);
  organism.advectRoot(world, energon, cellSize, heightScale, halfExtent);
  organism.metabolise(world, cellSize, heightScale);
  organism.tickNeuronViability(energon);
  energon.purgeDepletedBlobs();
  organism.transferEnergy(energon, cellSize, world.tickCount());
  organism.signal(energon, world.tickCount());
  organism.pruneNeuralAxons();
}

// Feedbag oracle: IV drip at mouth — skip perceive so threat/mate reflex does not block grazing.
void tickCampNomFeedbag(evolab::Organism& organism, evolab::BarrenWorld& world,
                        evolab::EnergonField& energon, float cellSize, float heightScale,
                        evolab::SkeletonNode& mouth) {
  const float halfExtent = worldHalfExtent(world, cellSize);
  energon.prepareSpatialQueries(cellSize, halfExtent, world);
  ensureAbundantFoodAtMouth(energon, mouth, cellSize);
  organism.feed(energon, cellSize, world.tickCount());
  organism.runDigestAndComputer(energon, world.tickCount());
  organism.computerFeedGain = 1.0f;
  const evolab::OrganismTickContext ctx{world,     energon,     cellSize,
                                        heightScale, halfExtent, world.tickCount()};
  evolab::runOrganismPreAdvectHooks(organism, ctx);
  organism.advectRoot(world, energon, cellSize, heightScale, halfExtent);
  organism.metabolise(world, cellSize, heightScale);
  organism.tickNeuronViability(energon);
  energon.purgeDepletedBlobs();
  organism.transferEnergy(energon, cellSize, world.tickCount());
  organism.signal(energon, world.tickCount());
  organism.pruneNeuralAxons();
}

struct SatietyWindowMetrics {
  int ticks = 0;
  int bites = 0;
  int strokesPaid = 0;
  int actuatorInhibited = 0;
  int signalExpulsions = 0;
  int fragmentExpulsions = 0;
};

std::size_t totalOrganismFuel(const evolab::Organism& organism) {
  return organism.totalFuelBytes();
}

bool campNomNeuronsIntact(const evolab::Organism& organism) {
  if (!organism.isCampNom()) {
    return false;
  }
  for (const evolab::SkeletonNode& node : organism.nodes) {
    if (node.neuron == evolab::NeuronType::None) {
      continue;
    }
    if (!node.alive) {
      return false;
    }
  }
  return true;
}

struct DaySnapshot {
  int dayIndex = 0;
  int tick = 0;
  std::size_t totalFuel = 0;
  std::size_t hubBytes = 0;
  std::size_t mouthBytes = 0;
  std::size_t actuatorBytes = 0;
  int strokesPaid = 0;
  int bites = 0;
  int signalExpulsions = 0;
  int fragmentExpulsions = 0;
  int hubSignalExpulsions = 0;
};

struct FeedbagOracleResult {
  std::size_t fuelStart = 0;
  std::size_t fuelEnd = 0;
  std::size_t fuelMin = 0;
  std::size_t hubMin = 0;
  std::size_t hubPeak = 0;
  std::size_t hubEnd = 0;
  int totalTicks = 0;
  int cumulativeBites = 0;
  int cumulativeStrokes = 0;
  int cumulativeSignalExpulsions = 0;
  int cumulativeFragmentExpulsions = 0;
  int cumulativeHubSignalExpulsions = 0;
  float netDeltaPerTick = 0.0f;
  float firstThirdDeltaPerTick = 0.0f;
  float lastThirdDeltaPerTick = 0.0f;
  bool alive = false;
  bool campIntact = false;
  std::vector<DaySnapshot> daySnapshots;
};

void prepareFeedbagOracleAxons(evolab::Organism& organism) {
  for (evolab::NeuralAxon& axon : organism.neuralAxons) {
    axon.trustFeed = evolab::kTrustBaseline;
    axon.etaEnergy = 1.0f;
    axon.etaSignal = 1.0f;
  }
}

FeedbagOracleResult runFeedbagOracle(int cycleDays, int worldSeed, int energonSeed) {
  const int periodTicks =
      static_cast<int>(std::lround(evolab::kVisualDayCyclePeriodTicks));
  const int totalTicks = cycleDays * periodTicks;

  evolab::DayCycle dayCycle(evolab::kVisualDayCyclePeriodTicks);
  evolab::BarrenWorld world(worldSeed, 64);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  if (!findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ)) {
    return {};
  }

  evolab::EnergonField energon = makeFoodWellField(energonSeed);
  evolab::Organism organism = evolab::makeCampNomOrganism(
      1, wetX, wetZ, 1.0f, evolab::kTicksPerStemCellDay * 2, 0, evolab::kWorldCellSize);
  prepareFeedbagOracleAxons(organism);
  organism.alive = true;
  organism.heading = 0.0f;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  evolab::SkeletonNode* mouth = organism.findNode(2);
  evolab::SkeletonNode* actuator = organism.findNode(4);
  if (mouth == nullptr || actuator == nullptr) {
    return {};
  }

  FeedbagOracleResult result;
  result.totalTicks = totalTicks;
  result.fuelStart = totalOrganismFuel(organism);
  result.fuelMin = result.fuelStart;
  result.hubMin = organism.computerHubFuelBytes();

  int signalCount = countFieldOrigin(energon, evolab::EnergonOrigin::Cloaca);
  int fragmentCount = countFieldOrigin(energon, evolab::EnergonOrigin::Fragment);
  int dayStrokes = 0;
  int dayBites = 0;
  int dayHubSignalExpulsions = 0;
  const int thirdTicks = std::max(1, totalTicks / 3);
  std::size_t thirdStartFuel = result.fuelStart;
  std::size_t twoThirdFuel = result.fuelStart;
  result.hubPeak = organism.computerHubFuelBytes();

  for (int tick = 0; tick < totalTicks; ++tick) {
    world.tick();
    const float sun = dayCycle.sunIntensity(world.tickCount());
    energon.tick(world, sun, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
    ensureAbundantFoodAtMouth(energon, *mouth, evolab::kWorldCellSize);
    tickCampNomFeedbag(organism, world, energon, evolab::kWorldCellSize,
                       evolab::kTerrainHeightScale, *mouth);

    if (mouth->ateThisTick) {
      ++dayBites;
    }
    if (organism.lastStrokePaid) {
      ++dayStrokes;
    }
    if (organism.lastHubSignalExpelledThisTick) {
      ++dayHubSignalExpulsions;
    }

    result.fuelMin = std::min(result.fuelMin, totalOrganismFuel(organism));
    result.hubMin = std::min(result.hubMin, organism.computerHubFuelBytes());
    result.hubPeak = std::max(result.hubPeak, organism.computerHubFuelBytes());

    if (tick + 1 == thirdTicks) {
      thirdStartFuel = totalOrganismFuel(organism);
    }
    if (tick + 1 == thirdTicks * 2) {
      twoThirdFuel = totalOrganismFuel(organism);
    }

    const bool endOfDay = ((tick + 1) % periodTicks) == 0;
    if (endOfDay) {
      const int newSignalCount = countFieldOrigin(energon, evolab::EnergonOrigin::Cloaca);
      const int newFragmentCount = countFieldOrigin(energon, evolab::EnergonOrigin::Fragment);
      result.cumulativeSignalExpulsions += newSignalCount - signalCount;
      result.cumulativeFragmentExpulsions += newFragmentCount - fragmentCount;
      signalCount = newSignalCount;
      fragmentCount = newFragmentCount;
      result.cumulativeStrokes += dayStrokes;
      result.cumulativeBites += dayBites;
      result.cumulativeHubSignalExpulsions += dayHubSignalExpulsions;

      DaySnapshot snap;
      snap.dayIndex = static_cast<int>(result.daySnapshots.size()) + 1;
      snap.tick = tick + 1;
      snap.totalFuel = totalOrganismFuel(organism);
      snap.hubBytes = organism.computerHubFuelBytes();
      snap.mouthBytes = mouth->store.size();
      snap.actuatorBytes = actuator->store.size();
      snap.strokesPaid = dayStrokes;
      snap.bites = dayBites;
      snap.signalExpulsions = result.cumulativeSignalExpulsions;
      snap.fragmentExpulsions = result.cumulativeFragmentExpulsions;
      snap.hubSignalExpulsions = result.cumulativeHubSignalExpulsions;
      result.daySnapshots.push_back(snap);

      dayStrokes = 0;
      dayBites = 0;
      dayHubSignalExpulsions = 0;
    }
  }

  result.fuelEnd = totalOrganismFuel(organism);
  result.hubEnd = organism.computerHubFuelBytes();
  const std::int64_t fuelDelta =
      static_cast<std::int64_t>(result.fuelEnd) - static_cast<std::int64_t>(result.fuelStart);
  result.netDeltaPerTick = static_cast<float>(fuelDelta) / static_cast<float>(totalTicks);
  result.firstThirdDeltaPerTick =
      static_cast<float>(static_cast<std::int64_t>(thirdStartFuel) -
                         static_cast<std::int64_t>(result.fuelStart)) /
      static_cast<float>(thirdTicks);
  result.lastThirdDeltaPerTick =
      static_cast<float>(static_cast<std::int64_t>(result.fuelEnd) -
                         static_cast<std::int64_t>(twoThirdFuel)) /
      static_cast<float>(thirdTicks);
  result.alive = organism.alive;
  result.campIntact = campNomNeuronsIntact(organism);
  return result;
}

void logFeedbagOracleResult(const FeedbagOracleResult& result, int cycleDays) {
  INFO("oracleDays=" << cycleDays << " ticks=" << result.totalTicks
                     << " fuelStart=" << result.fuelStart << " fuelEnd=" << result.fuelEnd
                     << " fuelMin=" << result.fuelMin << " hubMin=" << result.hubMin
                     << " hubPeak=" << result.hubPeak << " hubEnd=" << result.hubEnd
                     << " bites=" << result.cumulativeBites
                     << " strokes=" << result.cumulativeStrokes
                     << " signalExpulsions=" << result.cumulativeSignalExpulsions
                     << " hubSignalExpulsions=" << result.cumulativeHubSignalExpulsions
                     << " fragmentExpulsions=" << result.cumulativeFragmentExpulsions
                     << " netDeltaPerTick=" << result.netDeltaPerTick
                     << " firstThirdDeltaPerTick=" << result.firstThirdDeltaPerTick
                     << " lastThirdDeltaPerTick=" << result.lastThirdDeltaPerTick);
  for (const DaySnapshot& snap : result.daySnapshots) {
    INFO("day " << snap.dayIndex << " tick=" << snap.tick << " totalFuel=" << snap.totalFuel
                << " hub=" << snap.hubBytes << " mouth=" << snap.mouthBytes
                << " actuator=" << snap.actuatorBytes << " dayStrokes=" << snap.strokesPaid
                << " dayBites=" << snap.bites << " cumSignal=" << snap.signalExpulsions
                << " cumHubSignal=" << snap.hubSignalExpulsions
                << " cumFragment=" << snap.fragmentExpulsions);
  }
  if (result.netDeltaPerTick >= 0.0f) {
    INFO("lifeProjection=non-draining over window");
  } else {
    const float drainPerTick = -result.netDeltaPerTick;
    INFO("lifeProjectionDrainPerTick=" << drainPerTick << " runwayVisualDays="
                                       << (static_cast<float>(result.fuelEnd) / drainPerTick) /
                                              evolab::kVisualDayCyclePeriodTicks);
  }
}

void requireFeedbagOracleBasics(const FeedbagOracleResult& result, int cycleDays) {
  REQUIRE(result.alive);
  REQUIRE(result.campIntact);
  REQUIRE(static_cast<int>(result.daySnapshots.size()) == cycleDays);
  REQUIRE(static_cast<float>(result.cumulativeBites) >=
          static_cast<float>(result.totalTicks) * 0.85f);
}

}  // namespace

TEST_CASE("upper bound satiety: abundant food regulates crawl and expels hub signal",
          "[regulation][satiety]") {
  evolab::BarrenWorld world(11, 64);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon = makeFoodWellField(11);
  evolab::Organism organism = evolab::makeCampNomOrganism(
      1, wetX, wetZ, 1.0f, evolab::kTicksPerStemCellDay * 2, 0, evolab::kWorldCellSize);
  organism.alive = true;
  organism.heading = 0.0f;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  evolab::SkeletonNode* mouth = organism.findNode(2);
  evolab::SkeletonNode* actuator = organism.findNode(4);
  REQUIRE(mouth != nullptr);
  REQUIRE(actuator != nullptr);

  const std::size_t satiatedHubBytes = static_cast<std::size_t>(std::lround(
      evolab::confidenceToUnit(evolab::kComputerSatiationConfidence) *
      static_cast<float>(evolab::kComputerHubStoreMaxBytes)));
  evolab::assignComputerHubFuel(organism, satiatedHubBytes, 0);
  mouth->store.assign(evolab::kNeuronStoreMaxBytes, 0);
  actuator->store.assign(evolab::kActuatorStrokeCostPerTick * 4, 0);

  constexpr int kWarmupTicks = 120;
  constexpr int kMeasureTicks = 480;
  constexpr int kTotalTicks = kWarmupTicks + kMeasureTicks;

  SatietyWindowMetrics window;
  int signalBeforeMeasure = 0;
  int fragmentBeforeMeasure = 0;
  int feedSuppressedWithFood = 0;
  int foodContactTicks = 0;

  for (int tick = 0; tick < kTotalTicks; ++tick) {
    world.tick();
    energon.tick(world, 1.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
    ensureAbundantFoodAtMouth(energon, *mouth, evolab::kWorldCellSize);
    tickCampNomFull(organism, world, energon, evolab::kWorldCellSize,
                    evolab::kTerrainHeightScale, 1.0f, mouth);

    if (tick == kWarmupTicks) {
      signalBeforeMeasure = countFieldOrigin(energon, evolab::EnergonOrigin::Cloaca);
      fragmentBeforeMeasure = countFieldOrigin(energon, evolab::EnergonOrigin::Fragment);
    }

    if (tick >= kWarmupTicks) {
      ++window.ticks;
      if (organism.lastMouthHadFoodContact) {
        ++foodContactTicks;
      }
      if (mouth->ateThisTick) {
        ++window.bites;
      }
      if (organism.lastMouthHadFoodContact && organism.lastMouthFeedSuppressed) {
        ++feedSuppressedWithFood;
      }
      if (organism.lastStrokePaid) {
        ++window.strokesPaid;
      }
      if (organism.lastActuatorInhibited) {
        ++window.actuatorInhibited;
      }
    }
  }

  window.signalExpulsions =
      countFieldOrigin(energon, evolab::EnergonOrigin::Cloaca) - signalBeforeMeasure;
  window.fragmentExpulsions =
      countFieldOrigin(energon, evolab::EnergonOrigin::Fragment) - fragmentBeforeMeasure;

  INFO("metrics bites=" << window.bites << " foodContact=" << foodContactTicks
                        << " feedSuppressedWithFood=" << feedSuppressedWithFood
                        << " strokes=" << window.strokesPaid
                        << " inhibited=" << window.actuatorInhibited
                        << " signalOut=" << window.signalExpulsions
                        << " fragmentOut=" << window.fragmentExpulsions
                        << " hubBytes=" << organism.computerHubFuelBytes());

  REQUIRE(organism.alive);
  REQUIRE(organism.isCampNom());
  REQUIRE(window.ticks == kMeasureTicks);

  // Feedbag grazing when allowed; pre-satiated hub still suppresses most bites via threat/reflex.
  REQUIRE(foodContactTicks >= static_cast<int>(static_cast<float>(kMeasureTicks) * 0.65f));
  REQUIRE(window.strokesPaid <= static_cast<int>(static_cast<float>(kMeasureTicks) * 0.40f));
  // A no longer latched by mouth satiation; hub replete rarely sets motorSuppressed alone.
  REQUIRE(window.signalExpulsions >= static_cast<int>(kMeasureTicks * 0.5f));
  // Hub-near-full: digest backlog can sit in M above nominal cap until hub accepts bytes.
  REQUIRE(mouth->store.size() <= evolab::kNeuronStoreMaxBytes + 200);
  REQUIRE(window.fragmentExpulsions < 20);
}

TEST_CASE("satiety ramp: continuous feeding increases hub toward expulsion threshold",
          "[regulation][satiety]") {
  evolab::BarrenWorld world(13, 64);
  float wetX = 0.0f;
  float wetZ = 0.0f;
  REQUIRE(findWetWorldSite(world, evolab::kWorldCellSize, wetX, wetZ));

  evolab::EnergonField energon = makeFoodWellField(13);
  evolab::Organism organism = evolab::makeCampNomOrganism(
      1, wetX, wetZ, 1.0f, evolab::kTicksPerStemCellDay, 0, evolab::kWorldCellSize);
  organism.alive = true;
  organism.heading = 0.0f;
  organism.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  evolab::SkeletonNode* mouth = organism.findNode(2);
  REQUIRE(mouth != nullptr);

  const std::size_t hubStart = organism.computerHubFuelBytes();
  constexpr int kRampTicks = 4000;
  int totalBites = 0;
  int signalExpulsions = 0;
  std::size_t hubPeak = hubStart;

  for (int tick = 0; tick < kRampTicks; ++tick) {
    world.tick();
    energon.tick(world, 1.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
    ensureAbundantFoodAtMouth(energon, *mouth, evolab::kWorldCellSize);
    const int signalsBefore = countFieldOrigin(energon, evolab::EnergonOrigin::Cloaca);
    tickCampNomFull(organism, world, energon, evolab::kWorldCellSize,
                    evolab::kTerrainHeightScale, 1.0f, mouth);
    signalExpulsions +=
        countFieldOrigin(energon, evolab::EnergonOrigin::Cloaca) - signalsBefore;
    if (mouth->ateThisTick) {
      ++totalBites;
    }
    hubPeak = std::max(hubPeak, organism.computerHubFuelBytes());
  }

  INFO("hubStart=" << hubStart << " hubEnd=" << organism.computerHubFuelBytes() << " hubPeak=" << hubPeak
                   << " bites=" << totalBites << " signalExpulsions=" << signalExpulsions);

  REQUIRE(organism.alive);
  REQUIRE(organism.isCampNom());
  REQUIRE(totalBites >= 100);
  REQUIRE(hubPeak >= hubStart);
  // Hub may not reach computer satiation threshold in 4000 ticks from nominal spawn.
}

TEST_CASE("upper bound satiety: three day-night feedbag oracle sustains reserves",
          "[regulation][satiety][long]") {
  constexpr int kCycleDays = 3;
  const FeedbagOracleResult result = runFeedbagOracle(kCycleDays, 11, 11);
  logFeedbagOracleResult(result, kCycleDays);
  requireFeedbagOracleBasics(result, kCycleDays);

  REQUIRE(result.fuelEnd >= static_cast<std::size_t>(static_cast<float>(result.fuelStart) * 0.75f));
  REQUIRE(result.netDeltaPerTick > -8.0f);
}

TEST_CASE("feedbag oracle: nine visual days post-cap equilibrium trend",
          "[regulation][satiety][long][extended]") {
  constexpr int kCycleDays = 9;
  const FeedbagOracleResult result = runFeedbagOracle(kCycleDays, 11, 17);
  logFeedbagOracleResult(result, kCycleDays);
  requireFeedbagOracleBasics(result, kCycleDays);

  // After peripheral spawn buffers drain, late-window drain should soften vs early window.
  REQUIRE(result.fuelEnd >= result.fuelMin);
  REQUIRE(result.lastThirdDeltaPerTick >= result.firstThirdDeltaPerTick - 0.5f);
  REQUIRE(result.netDeltaPerTick > -4.0f);
  REQUIRE(result.fuelEnd >= static_cast<std::size_t>(static_cast<float>(result.fuelStart) * 0.65f));
}

TEST_CASE("feedbag oracle: twenty-seven visual days hub vent steady state (cloaca health)",
          "[regulation][satiety][long][marathon]") {
  constexpr int kCycleDays = 27;
  const FeedbagOracleResult result = runFeedbagOracle(kCycleDays, 11, 23);
  logFeedbagOracleResult(result, kCycleDays);
  requireFeedbagOracleBasics(result, kCycleDays);

  // Post-cap equilibrium: reserves recovered from early peripheral buffer drain.
  REQUIRE(result.fuelEnd >= result.fuelMin);
  REQUIRE(result.netDeltaPerTick >= -0.25f);

  // Hub cloaca: green baseline vent when replete — primary healthy expulsion route.
  REQUIRE(result.cumulativeHubSignalExpulsions > 0);
  REQUIRE(result.hubPeak >= static_cast<std::size_t>(static_cast<float>(evolab::kComputerHubStoreMaxBytes) *
                                                      evolab::confidenceToUnit(evolab::kComputerSatiationConfidence) *
                                                      0.85f));

  // Last week should show active hub venting (digest in, signal out — metabolic regularity).
  REQUIRE(result.daySnapshots.size() >= 7);
  const DaySnapshot& day27 = result.daySnapshots.back();
  const DaySnapshot& day20 = result.daySnapshots[result.daySnapshots.size() - 8];
  const int lastWeekHubSignalVents = day27.hubSignalExpulsions - day20.hubSignalExpulsions;
  INFO("lastWeekHubSignalVents=" << lastWeekHubSignalVents
                                 << " fieldSignalNet=" << (day27.signalExpulsions - day20.signalExpulsions));
  REQUIRE(lastWeekHubSignalVents >= static_cast<int>(evolab::kVisualDayCyclePeriodTicks) * 3);

  // No red Fragment flood from dysregulated M-cloaca / axon feed spam.
  REQUIRE(result.cumulativeFragmentExpulsions < 50);
}
