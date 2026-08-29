#include "sim/CellConstants.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/OrganismNeuron.hpp"
#include "sim/Organism.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>

TEST_CASE("neuron confidence byte range is shared by all neuron types", "[neuron_signal]") {
  REQUIRE(evolab::kNeuronConfidenceMax == 7u);
  REQUIRE(evolab::kPerceptorConfidenceMax == evolab::kNeuronConfidenceMax);
  REQUIRE(evolab::isNeuronConfidenceByte(0));
  REQUIRE(evolab::isNeuronConfidenceByte(7));
  REQUIRE(!evolab::isNeuronConfidenceByte(8));
  REQUIRE(!evolab::isNeuronConfidenceByte(0xA1u));
}

TEST_CASE("mouth fuel confidence maps store fill to 0-7", "[neuron_signal]") {
  evolab::SkeletonNode mouth;
  mouth.neuron = evolab::NeuronType::Mouth;

  mouth.store.clear();
  REQUIRE(evolab::mouthFuelConfidence(mouth) == 0);

  mouth.store.resize(evolab::kMouthLocalStoreMaxBytes / 2);
  mouth.mouthChewFill = static_cast<std::uint32_t>(evolab::kMouthLocalStoreMaxBytes / 2);
  REQUIRE(evolab::mouthFuelConfidence(mouth) == 4);

  mouth.mouthChewFill = static_cast<std::uint32_t>(std::lround(
      static_cast<float>(evolab::kMouthLocalStoreMaxBytes) *
      static_cast<float>(evolab::kMouthInhibitActuatorConfidence) /
      static_cast<float>(evolab::kNeuronConfidenceMax)));
  REQUIRE(evolab::mouthFuelConfidence(mouth) >= evolab::kMouthInhibitActuatorConfidence);

  mouth.mouthChewFill = evolab::kMouthLocalStoreMaxBytes;
  REQUIRE(evolab::mouthFuelConfidence(mouth) == evolab::kNeuronConfidenceMax);
}

TEST_CASE("mouth outbound caps low on distress-heavy diet despite full store", "[neuron_signal]") {
  evolab::SkeletonNode mouth;
  mouth.neuron = evolab::NeuronType::Mouth;
  mouth.store.resize(evolab::kNeuronConfidenceFullFuelBytes);

  for (int i = 0; i < 24; ++i) {
    evolab::recordMouthDietBite(mouth, evolab::EnergonOrigin::Cloaca, evolab::CloacaBand::Distress);
    evolab::creditMouthChew(mouth, evolab::kMouthLocalStoreMaxBytes);
  }

  REQUIRE(evolab::mouthFuelConfidence(mouth) == evolab::kNeuronConfidenceMax);
  REQUIRE(evolab::mouthOutboundConfidence(mouth) <= 2);
}

TEST_CASE("mouth outbound matches fuel when no diet history", "[neuron_signal]") {
  evolab::SkeletonNode mouth;
  mouth.neuron = evolab::NeuronType::Mouth;
  mouth.store.resize(evolab::kMouthLocalStoreMaxBytes);
  mouth.mouthChewFill = evolab::kMouthLocalStoreMaxBytes;
  REQUIRE(evolab::mouthOutboundConfidence(mouth) == evolab::mouthFuelConfidence(mouth));
}

TEST_CASE("actuator activity confidence scales with stroke payment", "[neuron_signal]") {
  REQUIRE(evolab::actuatorActivityConfidence(false, 0) == 0);
  REQUIRE(evolab::actuatorActivityConfidence(true, 0) == 0);
  REQUIRE(evolab::actuatorActivityConfidence(true, evolab::kActuatorStrokeCostPerTick) ==
          evolab::kNeuronConfidenceMax);
}

TEST_CASE("neuron confidence role labels describe analog semantics", "[neuron_signal]") {
  REQUIRE(std::string(evolab::neuronConfidenceRoleLabel(evolab::NeuronType::Perceptor)) ==
          "approach/avoid");
  REQUIRE(std::string(evolab::neuronConfidenceRoleLabel(evolab::NeuronType::Mouth)) ==
          "fuel/diet satiation");
  REQUIRE(std::string(evolab::neuronConfidenceRoleLabel(evolab::NeuronType::Actuator)) ==
          "flagella activity");
}

TEST_CASE("shared neuron interoception helpers decode P valence and gain", "[neuron_signal]") {
  REQUIRE(evolab::perceptorValenceFromConfidence(0) == Catch::Approx(-1.0f));
  REQUIRE(evolab::perceptorValenceFromConfidence(evolab::kNeuronConfidenceNeutral) ==
          Catch::Approx(0.142857f).margin(0.001f));
  REQUIRE(evolab::perceptorValenceFromConfidence(7) == Catch::Approx(1.0f));
  REQUIRE(evolab::perceptorGain(false, 0.0f) == Catch::Approx(0.35f));
  REQUIRE(evolab::perceptorGain(true, 1.0f) == Catch::Approx(1.0f));
}
