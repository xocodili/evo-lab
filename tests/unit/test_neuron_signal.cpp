#include "sim/CellConstants.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/Organism.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("neuron confidence byte range is shared by all neuron types", "[neuron_signal]") {
  REQUIRE(evolab::kNeuronConfidenceMax == 7u);
  REQUIRE(evolab::kPerceptorConfidenceMax == evolab::kNeuronConfidenceMax);
  REQUIRE(evolab::isNeuronConfidenceByte(0));
  REQUIRE(evolab::isNeuronConfidenceByte(7));
  REQUIRE(!evolab::isNeuronConfidenceByte(8));
  REQUIRE(!evolab::isNeuronConfidenceByte(evolab::kSignalTagIAte));
}

TEST_CASE("mouth fuel confidence maps store fill to 0-7", "[neuron_signal]") {
  evolab::SkeletonNode mouth;
  mouth.neuron = evolab::NeuronType::Mouth;

  mouth.store.clear();
  REQUIRE(evolab::mouthFuelConfidence(mouth) == 0);

  mouth.store.resize(evolab::kMouthLocalStoreMaxBytes);
  REQUIRE(evolab::mouthFuelConfidence(mouth) == 4);

  mouth.store.resize(evolab::kNeuronConfidenceFullFuelBytes / 4);
  REQUIRE(evolab::mouthFuelConfidence(mouth) >= 5);

  mouth.store.resize(evolab::kNeuronConfidenceFullFuelBytes);
  REQUIRE(evolab::mouthFuelConfidence(mouth) == evolab::kNeuronConfidenceMax);
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
          "fuel/satiation");
  REQUIRE(std::string(evolab::neuronConfidenceRoleLabel(evolab::NeuronType::Actuator)) ==
          "flagella activity");
}
