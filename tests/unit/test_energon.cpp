#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CloacaSignal.hpp"
#include "sim/DayCycle.hpp"
#include "sim/Energon.hpp"
#include "sim/EnergonRain.hpp"
#include "sim/EnergonString.hpp"
#include "sim/NeuronStem.hpp"
#include "sim/Organism.hpp"
#include "sim/Tide.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("day cycle sun intensity is zero at night", "[energon]") {
  evolab::DayCycle day(360.0f);
  const float noon = day.sunIntensity(90);
  const float night = day.sunIntensity(270);
  REQUIRE(noon > 0.5f);
  REQUIRE(night == Catch::Approx(0.0f).margin(1e-3f));
}

TEST_CASE("day cycle clock advances forward", "[energon]") {
  evolab::DayCycle day(360.0f);
  int h0 = 0;
  int m0 = 0;
  int h1 = 0;
  int m1 = 0;
  day.clockTime(270, h0, m0);
  day.clockTime(271, h1, m1);
  const int mins0 = h0 * 60 + m0;
  const int mins1 = h1 * 60 + m1;
  REQUIRE(mins1 > mins0);
}

TEST_CASE("rain cycle budget is f(population) times entropy", "[energon]") {
  const float perNom = evolab::rainCycleFieldBytesPerNom();
  REQUIRE(perNom ==
          Catch::Approx(static_cast<float>(evolab::kVisualDayCyclePeriodTicks) *
                        static_cast<float>(evolab::kCampNomRainCycleBurnPerTick) /
                        static_cast<float>(evolab::kBiteNetYieldBytes)));

  REQUIRE(evolab::rainCycleFieldBytesForPopulation(0) == Catch::Approx(0.0f));
  REQUIRE(evolab::rainCycleFieldBytesForPopulation(60) ==
          Catch::Approx(60.0f * perNom * evolab::kEnergonRainEntropy));

  REQUIRE(evolab::expectedSunfallBlobsPerTick(60, 1.0f) >
          evolab::expectedSunfallBlobsPerTick(20, 1.0f));
  REQUIRE(evolab::expectedSunfallBlobsPerTick(10, 0.0f) == Catch::Approx(0.0f));
}

TEST_CASE("energon spawns during daylight", "[energon]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonConfig config;
  config.populationScaledRain = false;
  evolab::EnergonField field(42, config);

  for (int i = 0; i < 200; ++i) {
    world.tick();
    field.tick(world, 0.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  }
  REQUIRE(field.activeCount() == 0);

  for (int i = 0; i < 400; ++i) {
    world.tick();
    field.tick(world, 1.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  }
  REQUIRE(field.activeCount() > 0);
}

TEST_CASE("population scaled sunfall increases with live organism count", "[energon]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonConfig config;
  config.populationScaledRain = true;
  config.spawnRateMax = 0.0f;
  config.maxBlobs = 8000;
  evolab::EnergonField field(42, config);

  for (int i = 0; i < 300; ++i) {
    world.tick();
    field.tick(world, 1.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 0);
  }
  const int emptyPopCount = field.activeCount();

  for (int i = 0; i < 300; ++i) {
    world.tick();
    field.tick(world, 1.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 60);
  }
  const int fullPopCount = field.activeCount();

  REQUIRE(fullPopCount > emptyPopCount + 50);
}

TEST_CASE("dry land energon decays faster than wet", "[energon]") {
  evolab::BarrenWorld world(7, 32);
  evolab::EnergonConfig config;
  config.spawnRateMax = 0.0f;
  config.ttlWetSeconds = 40.0f;
  config.ttlDrySeconds = 4.0f;
  evolab::EnergonField field(7, config);

  evolab::EnergonBlob dryBlob;
  dryBlob.id = 1;
  dryBlob.data = 0xFFFFFF;
  dryBlob.remaining = 3;
  dryBlob.initialBytes = 3;
  dryBlob.x = 0.0f;
  dryBlob.z = 0.0f;
  dryBlob.y = 1.0f;
  dryBlob.grounded = true;
  dryBlob.onWet = false;
  dryBlob.ttl = config.ttlDrySeconds;

  evolab::EnergonBlob wetBlob = dryBlob;
  wetBlob.id = 2;
  wetBlob.onWet = true;
  wetBlob.ttl = config.ttlWetSeconds;

  field.injectBlob(dryBlob);
  field.injectBlob(wetBlob);

  for (int i = 0; i < 120; ++i) {
    world.tick();
    field.tick(world, 0.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  }

  float dryTtl = 0.0f;
  float wetTtl = 0.0f;
  for (const evolab::EnergonBlob& blob : field.blobs()) {
    if (blob.id == 1) {
      dryTtl = blob.ttl;
    } else if (blob.id == 2) {
      wetTtl = blob.ttl;
    }
  }

  REQUIRE(dryTtl < wetTtl);
}

TEST_CASE("injectBlob enforces maxBlobs for cloaca vents", "[energon]") {
  evolab::EnergonConfig config;
  config.maxBlobs = 32;
  config.spawnRateMax = 0.0f;
  evolab::EnergonField field(99, config);

  for (int i = 0; i < 40; ++i) {
    evolab::EnergonBlob blob;
    blob.id = static_cast<std::uint32_t>(i + 1);
    blob.data = 0x01010101;
    blob.remaining = 1;
    blob.initialBytes = 1;
    blob.origin = evolab::EnergonOrigin::Cloaca;
    blob.x = static_cast<float>(i);
    blob.z = 0.0f;
    blob.y = 1.0f;
    blob.grounded = true;
    blob.onWet = true;
    blob.ttl = 10.0f;
    field.injectBlob(blob);
  }

  REQUIRE(field.activeCount() == config.maxBlobs);
}

TEST_CASE("sunfall evicts cloaca waste when field is at cap", "[energon]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonConfig config;
  config.maxBlobs = 32;
  config.spawnRateMax = 4.0f;
  config.populationScaledRain = false;
  evolab::EnergonField field(42, config);

  for (int i = 0; i < config.maxBlobs; ++i) {
    evolab::EnergonBlob blob;
    blob.id = static_cast<std::uint32_t>(i + 1);
    blob.data = 0x01010101;
    blob.remaining = 1;
    blob.initialBytes = 1;
    blob.origin = evolab::EnergonOrigin::Cloaca;
    blob.x = static_cast<float>(i);
    blob.z = 0.0f;
    blob.y = 1.0f;
    blob.grounded = true;
    blob.onWet = true;
    blob.ttl = 100.0f;
    field.injectBlob(blob);
  }
  REQUIRE(field.activeCount() == config.maxBlobs);

  int sunfallCount = 0;
  for (int i = 0; i < 120; ++i) {
    world.tick();
    field.tick(world, 1.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
    sunfallCount = 0;
    for (const evolab::EnergonBlob& blob : field.blobs()) {
      if (blob.origin == evolab::EnergonOrigin::Sunfall) {
        ++sunfallCount;
      }
    }
    if (sunfallCount > 0) {
      break;
    }
  }

  REQUIRE(sunfallCount > 0);
}

TEST_CASE("corpse release packs up to eight bytes per blob", "[energon]") {
  evolab::EnergonField field(1, {});
  evolab::SkeletonNode node;
  node.worldX = 1.0f;
  node.worldZ = 2.0f;
  node.worldY = 0.5f;
  std::vector<std::uint8_t> storage(10, 0xAB);

  evolab::releaseFuelAtNode(node, field, storage, evolab::EnergonOrigin::Fragment, 1.0f);

  REQUIRE(storage.empty());
  REQUIRE(field.activeCount() == 2);
  REQUIRE(field.blobs()[0].remaining == 8);
  REQUIRE(field.blobs()[1].remaining == 2);
  REQUIRE(field.blobs()[0].origin == evolab::EnergonOrigin::Fragment);
}

TEST_CASE("cloaca band wet TTL blue faster than green faster than red", "[energon]") {
  evolab::EnergonConfig config;
  config.ttlWetSeconds = 50.0f;

  evolab::EnergonBlob distress;
  distress.origin = evolab::EnergonOrigin::Cloaca;
  distress.data = evolab::kCloacaTagDistress;
  distress.remaining = 1;

  evolab::EnergonBlob baseline = distress;
  baseline.data = evolab::kCloacaTagBaseline;

  evolab::EnergonBlob mate = distress;
  mate.data = evolab::kCloacaTagMate;

  const float distressTtl = evolab::energonWetTtlSeconds(distress, config);
  const float baselineTtl = evolab::energonWetTtlSeconds(baseline, config);
  const float mateTtl = evolab::energonWetTtlSeconds(mate, config);

  REQUIRE(distressTtl < baselineTtl);
  REQUIRE(baselineTtl < mateTtl);
  REQUIRE(distressTtl == Catch::Approx(50.0f * evolab::kEnergonTtlDistressScale));
  REQUIRE(mateTtl == Catch::Approx(50.0f * evolab::kEnergonTtlMateScale));
}

TEST_CASE("airborne sunfall retains TTL until landing", "[energon]") {
  evolab::BarrenWorld world(42, 64);
  evolab::EnergonConfig config;
  config.populationScaledRain = false;
  config.spawnRateMax = 4.0f;
  config.maxBlobs = 64;
  evolab::EnergonField field(42, config);

  for (int i = 0; i < 40; ++i) {
    world.tick();
    field.tick(world, 1.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  }
  REQUIRE(field.activeCount() > 0);

  bool sawAirborne = false;
  float airborneTtl = 0.0f;
  for (const evolab::EnergonBlob& blob : field.blobs()) {
    if (blob.origin == evolab::EnergonOrigin::Sunfall && !blob.grounded) {
      sawAirborne = true;
      airborneTtl = blob.ttl;
      break;
    }
  }
  REQUIRE(sawAirborne);
  REQUIRE(airborneTtl == Catch::Approx(evolab::kEnergonAirborneTtlSeconds));

  for (int i = 0; i < 5; ++i) {
    world.tick();
    field.tick(world, 1.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  }

  for (const evolab::EnergonBlob& blob : field.blobs()) {
    if (blob.origin == evolab::EnergonOrigin::Sunfall && !blob.grounded) {
      REQUIRE(blob.ttl == Catch::Approx(airborneTtl));
    }
  }
}

TEST_CASE("fragment TTL is shorter than sunfall", "[energon]") {
  evolab::EnergonConfig config;
  config.ttlWetSeconds = 40.0f;

  evolab::EnergonBlob fragment;
  fragment.origin = evolab::EnergonOrigin::Fragment;
  fragment.remaining = 4;

  evolab::EnergonBlob sunfall;
  sunfall.origin = evolab::EnergonOrigin::Sunfall;
  sunfall.remaining = 4;

  REQUIRE(evolab::energonWetTtlSeconds(fragment, config) <
          evolab::energonWetTtlSeconds(sunfall, config));
}

TEST_CASE("barren world height at world coordinates clamps to grid", "[energon]") {
  evolab::BarrenWorld world(1, 16);
  const float h0 = world.heightAtWorld(0.0f, 0.0f, evolab::kWorldCellSize);
  const float hFar = world.heightAtWorld(9999.0f, -9999.0f, evolab::kWorldCellSize);
  REQUIRE(h0 == Catch::Approx(world.heightAt(7, 7)).margin(1e-3f));
  REQUIRE(hFar == Catch::Approx(world.heightAt(15, 0)).margin(1e-3f));
}

namespace {

evolab::EnergonBlob makeAttachedTestBlob(float x, float z, std::uint32_t id) {
  evolab::EnergonBlob blob;
  blob.id = id;
  blob.initialBytes = 4;
  blob.remaining = 4;
  blob.data = 0x01020304;
  blob.x = x;
  blob.z = z;
  blob.y = 1.0f;
  blob.grounded = true;
  blob.onWet = true;
  blob.ttl = 40.0f;
  blob.tailX = x - 1.0f;
  blob.tailZ = z;
  blob.headX = x + 1.0f;
  blob.headZ = z;
  return blob;
}

}  // namespace

TEST_CASE("mouth contact attaches energon string at consumption point", "[energon][attach]") {
  evolab::BarrenWorld world(3, 32);
  evolab::EnergonConfig config;
  config.spawnRateMax = 0.0f;
  evolab::EnergonField field(1, config);

  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, evolab::kWorldCellSize);
  camper.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  evolab::SkeletonNode* mouth = camper.findNode(evolab::kCampMouthId);
  REQUIRE(mouth != nullptr);

  field.injectBlob(makeAttachedTestBlob(mouth->worldX, mouth->worldZ, 9));
  const std::uint32_t blobId = field.blobs().front().id;

  camper.feed(field, evolab::kWorldCellSize, 0);
  REQUIRE(field.mouthAnchors().size() == 1);
  REQUIRE(field.mouthAnchors().front().blobId == blobId);
  REQUIRE(field.mouthAnchors().front().mouthNodeId == evolab::kCampMouthId);

  const float startTailX = field.blobs().front().tailX;
  for (evolab::SkeletonNode& node : camper.nodes) {
    node.worldX += 0.2f;
    node.worldZ -= 0.1f;
  }

  field.syncMouthAttachments({camper}, world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  REQUIRE(field.blobs().front().tailX == Catch::Approx(startTailX + 0.2f).margin(1e-3f));
}

TEST_CASE("mouth sticky zone anchors without long-range co-advect", "[energon][attach]") {
  evolab::BarrenWorld world(3, 32);
  evolab::EnergonConfig config;
  config.spawnRateMax = 0.0f;
  evolab::EnergonField field(1, config);

  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, evolab::kWorldCellSize);
  camper.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  evolab::SkeletonNode* mouth = camper.findNode(evolab::kCampMouthId);
  REQUIRE(mouth != nullptr);

  const float contactRadius = evolab::kWorldCellSize * evolab::kMouthContactRadiusFactor;
  const float stickyRadius = evolab::kWorldCellSize * evolab::kMouthStickyRadiusFactor;
  const float offset = contactRadius + (stickyRadius - contactRadius) * 0.5f;
  REQUIRE(offset > contactRadius);
  REQUIRE(offset < stickyRadius);

  evolab::EnergonBlob blob = makeAttachedTestBlob(mouth->worldX + offset, mouth->worldZ, 13);
  field.injectBlob(blob);
  const std::uint32_t blobId = field.blobs().front().id;

  field.prepareSpatialQueries(evolab::kWorldCellSize, 64.0f, world);
  field.applyMouthStickiness({camper}, stickyRadius);
  REQUIRE(field.mouthAnchors().size() == 1);
  REQUIRE(field.mouthAnchors().front().blobId == blobId);

  const float tailBefore = field.blobs().front().tailX;
  field.syncMouthAttachments({camper}, world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  REQUIRE(field.blobs().front().tailX == Catch::Approx(tailBefore).margin(1e-3f));

  mouth->worldX = field.blobs().front().tailX;
  mouth->worldZ = field.blobs().front().tailZ;
  field.syncMouthAttachments({camper}, world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  float t = 0.0f;
  const float distSqAfter = evolab::energonPointSegmentDistanceSq(
      mouth->worldX, mouth->worldZ, field.blobs().front(), t);
  REQUIRE(distSqAfter <= contactRadius * contactRadius * 1.25f);
}

TEST_CASE("mouth sticky attaches to closest string only", "[energon][attach]") {
  evolab::BarrenWorld world(3, 32);
  evolab::EnergonConfig config;
  config.spawnRateMax = 0.0f;
  evolab::EnergonField field(1, config);

  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, evolab::kWorldCellSize);
  camper.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  evolab::SkeletonNode* mouth = camper.findNode(evolab::kCampMouthId);
  REQUIRE(mouth != nullptr);

  const float stickyRadius = evolab::kWorldCellSize * evolab::kMouthStickyRadiusFactor;
  field.injectBlob(makeAttachedTestBlob(mouth->worldX + 1.0f, mouth->worldZ, 21));
  field.injectBlob(makeAttachedTestBlob(mouth->worldX + 2.0f, mouth->worldZ, 22));

  field.prepareSpatialQueries(evolab::kWorldCellSize, 64.0f, world);
  field.applyMouthStickiness({camper}, stickyRadius);

  REQUIRE(field.mouthAnchors().size() == 1);
  REQUIRE(field.mouthAnchors().front().mouthNodeId == evolab::kCampMouthId);
  REQUIRE(field.mouthAnchors().front().blobId == 21);
}

TEST_CASE("multiple campers pin same string at different mouths", "[energon][attach]") {
  evolab::BarrenWorld world(3, 32);
  evolab::EnergonConfig config;
  config.spawnRateMax = 0.0f;
  evolab::EnergonField field(1, config);

  evolab::Organism starA =
      evolab::makeStarMouthOrganism(1, -2.0f, 0.0f, 1.0f, 120, 0, 2, evolab::kWorldCellSize * 0.45f);
  evolab::Organism starB =
      evolab::makeStarMouthOrganism(2, 2.0f, 0.0f, 1.0f, 120, 0, 2, evolab::kWorldCellSize * 0.45f);
  starA.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  starB.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  evolab::SkeletonNode* mouthA = starA.findNode(2);
  evolab::SkeletonNode* mouthB = starB.findNode(2);
  REQUIRE(mouthA != nullptr);
  REQUIRE(mouthB != nullptr);

  evolab::EnergonBlob blob = makeAttachedTestBlob(0.0f, 0.0f, 23);
  blob.tailX = -2.5f;
  blob.headX = 2.5f;
  field.injectBlob(blob);
  const std::uint32_t blobId = field.blobs().front().id;

  field.setMouthAnchor(blobId, starA.id, mouthA->id, mouthA->worldX, mouthA->worldZ);
  field.setMouthAnchor(blobId, starB.id, mouthB->id, mouthB->worldX, mouthB->worldZ);
  REQUIRE(field.mouthAnchors().size() == 2);
  REQUIRE(field.mouthAnchors().front().blobId == blobId);
  REQUIRE(field.mouthAnchors().back().blobId == blobId);
}

TEST_CASE("multi-mouth freak pins string at both contact points", "[energon][attach]") {
  evolab::BarrenWorld world(3, 32);
  evolab::EnergonConfig config;
  config.spawnRateMax = 0.0f;
  evolab::EnergonField field(1, config);

  evolab::Organism star =
      evolab::makeStarMouthOrganism(2, 0.0f, 0.0f, 1.0f, 120, 0, 2, evolab::kWorldCellSize * 0.45f);
  star.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);

  evolab::SkeletonNode* mouthA = star.findNode(2);
  evolab::SkeletonNode* mouthB = star.findNode(3);
  REQUIRE(mouthA != nullptr);
  REQUIRE(mouthB != nullptr);

  const float midX = (mouthA->worldX + mouthB->worldX) * 0.5f;
  const float midZ = (mouthA->worldZ + mouthB->worldZ) * 0.5f;
  field.injectBlob(makeAttachedTestBlob(midX, midZ, 11));

  mouthA->worldX -= 0.05f;
  mouthB->worldX += 0.05f;
  const std::uint32_t blobId = field.blobs().front().id;
  field.setMouthAnchor(blobId, star.id, mouthA->id, mouthA->worldX, mouthA->worldZ);
  field.setMouthAnchor(blobId, star.id, mouthB->id, mouthB->worldX, mouthB->worldZ);
  REQUIRE(field.mouthAnchors().size() == 2);

  const float tailBefore = field.blobs().front().tailX;
  const float headBefore = field.blobs().front().headX;
  for (evolab::SkeletonNode& node : star.nodes) {
    node.worldX += 0.2f;
  }

  field.syncMouthAttachments({star}, world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  REQUIRE(field.blobs().front().tailX == Catch::Approx(tailBefore + 0.2f).margin(1e-3f));
  REQUIRE(field.blobs().front().headX == Catch::Approx(headBefore + 0.2f).margin(1e-3f));
}

TEST_CASE("anchored strings skip independent tidal advection", "[energon][attach]") {
  evolab::TideConfig tideConfig;
  tideConfig.amplitude = 8.0f;
  evolab::BarrenWorld world(31, 32, evolab::Tide(tideConfig));
  evolab::EnergonConfig config;
  config.spawnRateMax = 0.0f;
  evolab::EnergonField field(1, config);

  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, evolab::kWorldCellSize);
  camper.updateKinematics(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  evolab::SkeletonNode* mouth = camper.findNode(evolab::kCampMouthId);
  REQUIRE(mouth != nullptr);

  field.injectBlob(makeAttachedTestBlob(mouth->worldX, mouth->worldZ, 12));
  camper.feed(field, evolab::kWorldCellSize, 0);
  REQUIRE(field.blobHasMouthAnchor(field.blobs().front().id));

  const float anchoredX = field.blobs().front().x;
  for (int i = 0; i < 40; ++i) {
    world.tick();
    field.tick(world, 1.0f, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  }
  REQUIRE(field.blobs().front().x == Catch::Approx(anchoredX).margin(1e-3f));
}
