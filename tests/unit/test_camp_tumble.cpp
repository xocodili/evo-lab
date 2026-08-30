#include "sim/CellConstants.hpp"
#include "sim/Chaos.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismActuator.hpp"
#include "sim/PerceptorFocus.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <random>

TEST_CASE("camp locomotion anchored on P food lock without bearing window", "[camp][tumble]") {
  evolab::ActuatorInteroception interoception;
  interoception.perceptorLocked = true;
  interoception.focusKind = evolab::PerceptFocusKind::Food;
  interoception.approach = 0.5f;
  interoception.focusBearing = 1.2f;
  REQUIRE(evolab::campLocomotionAnchored(interoception));
}

TEST_CASE("camp locomotion anchored on directional M taste", "[camp][tumble]") {
  evolab::ActuatorInteroception interoception;
  interoception.mouthTasteApproach = 0.4f;
  interoception.mouthTasteBearing = 0.9f;
  interoception.mouthTasteSymmetricAmbiguity = false;
  REQUIRE(evolab::campLocomotionAnchored(interoception));
}

TEST_CASE("symmetric M taste is not an energon anchor", "[camp][tumble]") {
  evolab::ActuatorInteroception interoception;
  interoception.mouthTasteApproach = 0.4f;
  interoception.mouthTasteSymmetricAmbiguity = true;
  REQUIRE_FALSE(evolab::campLocomotionAnchored(interoception));
}

TEST_CASE("flat interoception allows tumble scaling", "[camp][tumble]") {
  evolab::ActuatorInteroception interoception;
  const evolab::MotorIntent intent = evolab::computeCampMotorIntent(interoception, 8);
  REQUIRE(intent.tumbleRateScale > 0.0f);
}

TEST_CASE("anchored interoception zeroes tumble rate scale", "[camp][tumble]") {
  evolab::ActuatorInteroception interoception;
  interoception.perceptorLocked = true;
  interoception.focusKind = evolab::PerceptFocusKind::Food;
  interoception.approach = 0.6f;
  const evolab::MotorIntent intent = evolab::computeCampMotorIntent(interoception, 8);
  REQUIRE(intent.tumbleRateScale == Catch::Approx(0.0f));
}

TEST_CASE("tumble phenotype jitters at spawn", "[camp][tumble]") {
  std::mt19937 rng(5150);
  evolab::Organism organism =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.finalizeSpawn(rng);
  const bool rateMoved =
      organism.tumbleRateFactor != evolab::kDefaultTumbleRateFactor;
  const bool turnMoved =
      organism.tumbleTurnFactor != evolab::kDefaultTumbleTurnFactor;
  REQUIRE((rateMoved || turnMoved));
  REQUIRE(organism.tumbleChiralityBias >= -evolab::kTumbleChiralityBiasMax);
  REQUIRE(organism.tumbleChiralityBias <= evolab::kTumbleChiralityBiasMax);
}

TEST_CASE("positive chirality bias skews tumble direction", "[camp][tumble]") {
  std::mt19937 rng(9001);
  int rightTurns = 0;
  constexpr int kTrials = 400;
  constexpr float kBias = 0.75f;
  const float rightProb =
      0.5f + 0.5f * std::clamp(kBias, -evolab::kTumbleChiralityBiasMax,
                               evolab::kTumbleChiralityBiasMax);
  for (int i = 0; i < kTrials; ++i) {
    if (evolab::chaosBernoulli(rightProb, rng)) {
      ++rightTurns;
    }
  }
  REQUIRE(rightTurns > static_cast<int>(kTrials * rightProb * 0.75f));
  REQUIRE(rightTurns < static_cast<int>(kTrials * rightProb * 1.25f));
}
