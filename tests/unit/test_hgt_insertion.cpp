#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/Energon.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismHgt.hpp"
#include "sim/OrganismInternal.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronStem.hpp"

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

namespace {

evolab::Organism makeCampAt(float x, float z, std::uint32_t id, std::size_t storage) {
  evolab::Organism organism =
      evolab::makeCampNomOrganism(id, x, z, 1.0f, storage, 0, 1.0f);
  organism.alive = true;
  return organism;
}

void seedAllNodes(evolab::Organism& organism, float x, float z) {
  for (evolab::SkeletonNode& node : organism.nodes) {
    node.worldX = x;
    node.worldZ = z;
  }
}

struct DeathFeastScenario {
  evolab::Organism dying;
  evolab::Organism immortal;
  std::size_t immortalBaselineAxons = 0;
};

DeathFeastScenario makeDeathFeastRubScenario() {
  DeathFeastScenario scenario;
  scenario.dying = makeCampAt(0.0f, 0.0f, 1, 64);
  scenario.immortal = makeCampAt(0.0f, 0.0f, 2, evolab::kStemCellStorageMaxBytes);

  scenario.dying.bodyStorage.clear();
  for (evolab::SkeletonNode& node : scenario.dying.nodes) {
    node.store.clear();
  }

  evolab::SkeletonNode* dyingP = scenario.dying.findNode(evolab::kCampPerceptorId);
  REQUIRE(dyingP != nullptr);
  evolab::transitionAxonsOnNeuronDeath(scenario.dying, *dyingP);
  dyingP->alive = false;
  dyingP->neuron = evolab::NeuronType::None;

  scenario.dying.neuralAxons.erase(
      std::remove_if(scenario.dying.neuralAxons.begin(), scenario.dying.neuralAxons.end(),
                     [](const evolab::NeuralAxon& axon) {
                       return !(axon.srcNodeId == evolab::kCampPerceptorId &&
                                axon.dstNodeId == evolab::kCampMouthId &&
                                evolab::axonIsDangling(axon));
                     }),
      scenario.dying.neuralAxons.end());
  REQUIRE(scenario.dying.neuralAxons.size() == 1);

  scenario.immortal.bodyStorage.assign(evolab::kStemCellStorageMaxBytes, 1);
  evolab::SkeletonNode* immortalP = scenario.immortal.findNode(evolab::kCampPerceptorId);
  evolab::SkeletonNode* immortalM = scenario.immortal.findNode(evolab::kCampMouthId);
  REQUIRE(immortalP != nullptr);
  REQUIRE(immortalM != nullptr);
  immortalP->store.assign(evolab::kNeuronStoreMaxBytes, 1);
  immortalM->store.assign(evolab::kNeuronStoreMaxBytes, 1);

  scenario.immortal.neuralAxons.erase(
      std::remove_if(scenario.immortal.neuralAxons.begin(), scenario.immortal.neuralAxons.end(),
                     [](const evolab::NeuralAxon& axon) {
                       return axon.srcNodeId == evolab::kCampPerceptorId &&
                              axon.dstNodeId == evolab::kCampMouthId;
                     }),
      scenario.immortal.neuralAxons.end());
  scenario.immortalBaselineAxons = scenario.immortal.neuralAxons.size();
  REQUIRE(scenario.immortal.findNeuralAxon(evolab::kCampPerceptorId,
                                           evolab::kCampMouthId) == nullptr);

  const evolab::NeuralAxon& stub = scenario.dying.neuralAxons.front();
  float rubX = 0.0f;
  float rubZ = 0.0f;
  evolab::axonUncappedWorldPos(stub, rubX, rubZ);
  seedAllNodes(scenario.immortal, rubX, rubZ);
  for (evolab::SkeletonNode& node : scenario.dying.nodes) {
    if (node.alive) {
      node.worldX = rubX;
      node.worldZ = rubZ;
    }
  }

  return scenario;
}

bool runSingleDeathFeastDockTrial(float dockRate, std::uint64_t trialId) {
  DeathFeastScenario scenario = makeDeathFeastRubScenario();
  std::vector<evolab::Organism> population{scenario.dying, scenario.immortal};

  evolab::HgtDockPassOptions options;
  options.dockRateOverride = dockRate;
  evolab::tickHgtDockPass(population, 1.0f, trialId + 10'000, options);

  return population[1].findNeuralAxon(evolab::kCampPerceptorId, evolab::kCampMouthId) != nullptr;
}

bool withinBinomialTolerance(int successes, int trials, float expectedRate, float sigmaBand) {
  if (expectedRate >= 1.0f) {
    return successes == trials;
  }
  if (expectedRate <= 0.0f) {
    return successes == 0;
  }
  const float mean = static_cast<float>(trials) * expectedRate;
  const float stddev =
      std::sqrt(static_cast<float>(trials) * expectedRate * (1.0f - expectedRate));
  return std::abs(static_cast<float>(successes) - mean) <= sigmaBand * stddev;
}

int countDockSuccesses(float dockRate, int trials, std::uint64_t seedBase) {
  int successes = 0;
  for (int trial = 0; trial < trials; ++trial) {
    if (runSingleDeathFeastDockTrial(dockRate, seedBase + static_cast<std::uint64_t>(trial))) {
      ++successes;
    }
  }
  return successes;
}

}  // namespace

TEST_CASE("P death leaves dangling axons on partial topology", "[hgt]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism = makeCampAt(0.0f, 0.0f, 1, 500);
  const std::size_t axonsBefore = organism.neuralAxons.size();

  evolab::SkeletonNode* perceptor = organism.findNode(evolab::kCampPerceptorId);
  REQUIRE(perceptor != nullptr);
  perceptor->store.clear();
  evolab::transitionAxonsOnNeuronDeath(organism, *perceptor);
  perceptor->alive = false;
  perceptor->neuron = evolab::NeuronType::None;

  REQUIRE(organism.hasLiveFunctionalNeurons());
  REQUIRE(evolab::countDanglingAxons(organism) == 6);
  REQUIRE(organism.neuralAxons.size() == axonsBefore);
  REQUIRE(organism.findNode(evolab::kCampMouthId)->alive);
  REQUIRE(organism.findNode(evolab::kCampComputerId)->alive);
  REQUIRE(organism.findNode(evolab::kCampActuatorId)->alive);
}

TEST_CASE("dangling inbound stub accelerates dst basal drain", "[hgt]") {
  evolab::EnergonField field(1, {});
  evolab::Organism intact = makeCampAt(0.0f, 0.0f, 1, 500);
  evolab::Organism partial = makeCampAt(10.0f, 10.0f, 2, 500);

  evolab::SkeletonNode* mouthIntact = intact.findNode(evolab::kCampMouthId);
  evolab::SkeletonNode* mouthPartial = partial.findNode(evolab::kCampMouthId);
  evolab::SkeletonNode* perceptor = partial.findNode(evolab::kCampPerceptorId);
  REQUIRE(mouthIntact != nullptr);
  REQUIRE(mouthPartial != nullptr);
  REQUIRE(perceptor != nullptr);

  mouthIntact->store.assign(80, 1);
  mouthPartial->store.assign(80, 1);

  evolab::transitionAxonsOnNeuronDeath(partial, *perceptor);
  perceptor->alive = false;
  perceptor->neuron = evolab::NeuronType::None;

  REQUIRE(evolab::countDanglingAxons(partial) > 0);

  for (int i = 0; i < 8; ++i) {
    intact.tickAxonTransitBasal();
    partial.tickAxonTransitBasal();
  }

  REQUIRE(mouthPartial->store.size() < mouthIntact->store.size());
}

TEST_CASE("foreign dock inserts cloned axon motif on recipient", "[hgt]") {
  evolab::Organism donor = makeCampAt(0.0f, 0.0f, 1, 800);
  evolab::Organism recipient = makeCampAt(0.0f, 0.0f, 2, 800);

  evolab::SkeletonNode* donorP = donor.findNode(evolab::kCampPerceptorId);
  evolab::SkeletonNode* recipientP = recipient.findNode(evolab::kCampPerceptorId);
  evolab::SkeletonNode* recipientM = recipient.findNode(evolab::kCampMouthId);
  REQUIRE(donorP != nullptr);
  REQUIRE(recipientP != nullptr);
  REQUIRE(recipientM != nullptr);

  recipientP->store.assign(32, 1);
  recipient.neuralAxons.erase(
      std::remove_if(recipient.neuralAxons.begin(), recipient.neuralAxons.end(),
                     [](const evolab::NeuralAxon& axon) {
                       return axon.srcNodeId == evolab::kCampPerceptorId &&
                              axon.dstNodeId == evolab::kCampMouthId;
                     }),
      recipient.neuralAxons.end());
  REQUIRE(recipient.findNeuralAxon(evolab::kCampPerceptorId, evolab::kCampMouthId) == nullptr);

  seedAllNodes(donor, 0.0f, 0.0f);
  seedAllNodes(recipient, 0.0f, 0.0f);

  evolab::transitionAxonsOnNeuronDeath(donor, *donorP);
  donorP->alive = false;
  donorP->neuron = evolab::NeuronType::None;

  const std::size_t recipientAxonsBefore = recipient.neuralAxons.size();
  REQUIRE(evolab::countDanglingAxons(donor) == 6);

  std::vector<evolab::Organism> population{donor, recipient};
  bool docked = false;
  for (std::uint64_t tick = 1; tick <= 400 && !docked; ++tick) {
    evolab::tickHgtDockPass(population, 1.0f, tick);
    docked = population[1].neuralAxons.size() > recipientAxonsBefore;
  }

  REQUIRE(docked);
  REQUIRE(population[1].findNeuralAxon(evolab::kCampPerceptorId, evolab::kCampMouthId) !=
          nullptr);
}

TEST_CASE("healthy overlap produces zero docks without dangling ends", "[hgt]") {
  evolab::Organism a = makeCampAt(0.0f, 0.0f, 1, 500);
  evolab::Organism b = makeCampAt(0.0f, 0.0f, 2, 500);
  seedAllNodes(a, 0.0f, 0.0f);
  seedAllNodes(b, 0.0f, 0.0f);

  const std::size_t axonsA = a.neuralAxons.size();
  const std::size_t axonsB = b.neuralAxons.size();
  std::vector<evolab::Organism> population{a, b};

  for (std::uint64_t tick = 1; tick <= 100; ++tick) {
    evolab::tickHgtDockPass(population, 1.0f, tick);
  }

  REQUIRE(population[0].neuralAxons.size() == axonsA);
  REQUIRE(population[1].neuralAxons.size() == axonsB);
  REQUIRE(evolab::countDanglingAxons(population[0]) == 0);
  REQUIRE(evolab::countDanglingAxons(population[1]) == 0);
}

TEST_CASE("dock rejected at axon channel capacity", "[hgt]") {
  evolab::Organism donor = makeCampAt(0.0f, 0.0f, 1, 800);
  evolab::Organism recipient = makeCampAt(0.0f, 0.0f, 2, 800);

  evolab::SkeletonNode* donorP = donor.findNode(evolab::kCampPerceptorId);
  evolab::SkeletonNode* recipientP = recipient.findNode(evolab::kCampPerceptorId);
  evolab::SkeletonNode* recipientM = recipient.findNode(evolab::kCampMouthId);
  REQUIRE(donorP != nullptr);
  REQUIRE(recipientP != nullptr);
  REQUIRE(recipientM != nullptr);

  recipientP->store.assign(32, 1);
  while (recipient.neuralAxons.size() < evolab::kAxonChannelCapacity) {
    evolab::NeuralAxon filler;
    filler.srcNodeId = evolab::kCampActuatorId;
    filler.dstNodeId = evolab::kCampComputerId;
    recipient.neuralAxons.push_back(filler);
  }

  seedAllNodes(donor, 0.0f, 0.0f);
  seedAllNodes(recipient, 0.0f, 0.0f);
  evolab::transitionAxonsOnNeuronDeath(donor, *donorP);
  donorP->alive = false;

  const std::size_t recipientAxonsBefore = recipient.neuralAxons.size();
  std::vector<evolab::Organism> population{donor, recipient};
  for (std::uint64_t tick = 1; tick <= 200; ++tick) {
    evolab::tickHgtDockPass(population, 1.0f, tick);
  }

  REQUIRE(population[1].neuralAxons.size() == recipientAxonsBefore);
}

TEST_CASE("death feast rub: co-located immortal acquires dangling P to M edge",
          "[hgt][death_feast]") {
  DeathFeastScenario scenario = makeDeathFeastRubScenario();
  std::vector<evolab::Organism> population{scenario.dying, scenario.immortal};

  evolab::HgtDockPassOptions certainty;
  certainty.dockRateOverride = 1.0f;
  for (std::uint64_t tick = 1; tick <= 30; ++tick) {
    evolab::tickHgtDockPass(population, 1.0f, tick, certainty);
    if (population[1].findNeuralAxon(evolab::kCampPerceptorId, evolab::kCampMouthId) !=
        nullptr) {
      break;
    }
  }

  REQUIRE(population[1].findNeuralAxon(evolab::kCampPerceptorId, evolab::kCampMouthId) !=
          nullptr);
  REQUIRE(evolab::countForeignInsertions(population[1], scenario.immortalBaselineAxons) == 1);
}

TEST_CASE("death feast dock rate calibration", "[hgt][death_feast]") {
  SECTION("100% dock rate always inserts on first rub tick") {
    constexpr int kTrials = 32;
    const int successes = countDockSuccesses(1.0f, kTrials, 0xFEA57100ULL);
    REQUIRE(successes == kTrials);
  }

  SECTION("50% dock rate inserts on roughly half of independent rub trials") {
    constexpr int kTrials = 600;
    const int successes = countDockSuccesses(0.5f, kTrials, 0xFEA57050ULL);
    INFO("successes=" << successes << " / " << kTrials);
    REQUIRE(withinBinomialTolerance(successes, kTrials, 0.5f, 3.5f));
    REQUIRE(successes > kTrials / 4);
    REQUIRE(successes < (kTrials * 3) / 4);
  }

  SECTION("10% dock rate inserts on roughly one tenth of independent rub trials") {
    constexpr int kTrials = 1200;
    const int successes = countDockSuccesses(0.1f, kTrials, 0xFEA57010ULL);
    INFO("successes=" << successes << " / " << kTrials);
    REQUIRE(withinBinomialTolerance(successes, kTrials, 0.1f, 3.5f));
    REQUIRE(successes > kTrials / 20);
    REQUIRE(successes < kTrials / 5);
  }
}

TEST_CASE("production dock rate stays rare on single rub tick", "[hgt][death_feast]") {
  constexpr int kTrials = 80;
  int successes = 0;
  for (int trial = 0; trial < kTrials; ++trial) {
    if (runSingleDeathFeastDockTrial(-1.0f, 0xFEA57A0EULL + static_cast<std::uint64_t>(trial))) {
      ++successes;
    }
  }
  INFO("production-rate successes=" << successes << " / " << kTrials);
  REQUIRE(successes <= kTrials / 5);
}
