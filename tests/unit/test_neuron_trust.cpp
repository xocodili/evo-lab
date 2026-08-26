#include "sim/CellConstants.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/EnergonConveyance.hpp"
#include "sim/Energon.hpp"
#include "sim/NeuronFuel.hpp"
#include "sim/NeuronTrust.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismInternal.hpp"
#include "sim/OrganismMouth.hpp"
#include "sim/OrganismNeuron.hpp"
#include "sim/PerceptorFocus.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("believe trust is indexed by confidence byte 0-7", "[neuron_trust]") {
  evolab::NeuralAxon axon;
  evolab::setAllBelieveTrust(axon, evolab::kTrustBaseline);
  axon.trustBelieveByConfidence[3] = 120;
  axon.trustBelieveByConfidence[7] = 400;

  REQUIRE(evolab::axonBelieveTrustForByte(axon, 3) == 120);
  REQUIRE(evolab::axonBelieveTrustForByte(axon, 7) == 400);
  REQUIRE(evolab::axonMaxBelieveTrust(axon) == 400);
}

TEST_CASE("nudgeBelieveTrustBin clamps to modifiable range", "[neuron_trust]") {
  evolab::NeuralAxon axon;
  evolab::setAllBelieveTrust(axon, evolab::kTrustBaseline);

  for (int i = 0; i < 40; ++i) {
    evolab::nudgeBelieveTrustBin(axon, 6, static_cast<int>(evolab::kTrustLearnStep));
  }
  REQUIRE(axon.trustBelieveByConfidence[6] == evolab::kTrustMax);
  REQUIRE(axon.trustBelieveByConfidence[4] == evolab::kTrustBaseline);

  evolab::nudgeBelieveTrustBin(axon, 6, -static_cast<int>(evolab::kTrustMax));
  REQUIRE(axon.trustBelieveByConfidence[6] == evolab::kTrustMin);
}

TEST_CASE("axon pruning requires all believe bins and feed trust zero", "[neuron_trust]") {
  evolab::NeuralAxon axon;
  axon.trustBelieveByConfidence.fill(100);
  axon.trustFeed = 0;
  REQUIRE_FALSE(evolab::axonMarkedForPruning(axon));

  axon.trustBelieveByConfidence.fill(0);
  REQUIRE(evolab::axonMarkedForPruning(axon));
}

TEST_CASE("mouth trust learning strengthens P approach byte after successful bite", "[neuron_trust]") {
  evolab::Organism organism = evolab::makeNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.alive = true;

  evolab::NeuralAxon* pToM = organism.findNeuralAxon(1, 2);
  REQUIRE(pToM != nullptr);
  evolab::setAllBelieveTrust(*pToM, evolab::kTrustBaseline);
  const std::uint16_t before = pToM->trustBelieveByConfidence[7];

  pToM->lastReceived.valid = true;
  pToM->lastReceived.byte = 7;
  pToM->lastReceived.tick = 42;

  evolab::MouthTrustEvent event;
  event.hadFoodContact = true;
  event.ate = true;
  event.feedSuppressed = false;

  evolab::applyPmaMouthTrustLearning(organism, 2, event, 42);
  REQUIRE(pToM->trustBelieveByConfidence[7] == before + evolab::kTrustLearnStep);
  REQUIRE(pToM->trustBelieveByConfidence[4] == evolab::kTrustBaseline);
}

TEST_CASE("mouth trust learning weakens P approach byte when feed suppressed at food",
          "[neuron_trust]") {
  evolab::Organism organism = evolab::makeNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.alive = true;

  evolab::NeuralAxon* pToM = organism.findNeuralAxon(1, 2);
  REQUIRE(pToM != nullptr);
  evolab::setAllBelieveTrust(*pToM, evolab::kTrustBaseline);

  pToM->lastReceived.valid = true;
  pToM->lastReceived.byte = 7;
  pToM->lastReceived.tick = 9;

  evolab::MouthTrustEvent event;
  event.hadFoodContact = true;
  event.ate = false;
  event.feedSuppressed = true;

  evolab::applyPmaMouthTrustLearning(organism, 2, event, 9);
  REQUIRE(pToM->trustBelieveByConfidence[7] == evolab::kTrustBaseline - evolab::kTrustLearnStep);
}

TEST_CASE("actuator inbound tick aligns with prior advect for mouth interoception", "[neuron_trust]") {
  REQUIRE(evolab::inboundAxonTickEligible(evolab::NeuronType::Perceptor, 10, 10, true));
  REQUIRE_FALSE(evolab::inboundAxonTickEligible(evolab::NeuronType::Perceptor, 9, 10, true));
  REQUIRE(evolab::inboundAxonTickEligible(evolab::NeuronType::Actuator, 9, 10, true));
  REQUIRE_FALSE(evolab::inboundAxonTickEligible(evolab::NeuronType::Actuator, 8, 10, true));

  evolab::Organism organism = evolab::makeNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.alive = true;
  evolab::NeuralAxon* aToM = organism.findNeuralAxon(3, 2);
  REQUIRE(aToM != nullptr);
  aToM->lastReceived.valid = true;
  aToM->lastReceived.byte = evolab::kNeuronConfidenceMax;
  aToM->lastReceived.tick = 4;

  const evolab::SkeletonNode* mouth = organism.findNode(2);
  REQUIRE(mouth != nullptr);
  const evolab::MouthInteroception interoception =
      evolab::gatherMouthInteroception(organism, mouth->id, *mouth, 5);
  REQUIRE(interoception.actuatorActivity > 0.0f);
}

TEST_CASE("inbound interoception uses byte-specific believe trust", "[neuron_trust]") {
  evolab::NeuralAxon axon;
  evolab::setAllBelieveTrust(axon, evolab::kTrustBaseline);
  axon.trustBelieveByConfidence[7] = evolab::kTrustBaseline;
  axon.trustBelieveByConfidence[4] = evolab::kTrustMin;
  axon.etaSignal = 1.0f;

  const float high = evolab::inboundAxonTrustWeight(axon, 7);
  const float neutral = evolab::inboundAxonTrustWeight(axon, 4);
  REQUIRE(high > neutral);
}

TEST_CASE("prediction error byte maps outcome to 0-7 gradient", "[neuron_trust]") {
  REQUIRE(evolab::predictionErrorByte(0.0f) == evolab::kNeuronConfidenceNeutral);
  REQUIRE(evolab::predictionErrorByte(1.0f) == evolab::kNeuronConfidenceMax);
  REQUIRE(evolab::predictionErrorByte(-1.0f) == 0);
  REQUIRE(evolab::trustDeltaFromPredictionError(evolab::kNeuronConfidenceNeutral) == 0);
  REQUIRE(evolab::trustDeltaFromPredictionError(7) ==
          static_cast<int>(evolab::kTrustLearnStep));
  REQUIRE(evolab::trustDeltaFromPredictionError(0) ==
          -static_cast<int>(evolab::kTrustLearnStep));
}

TEST_CASE("perceptor trust strengthens M satiation byte when full and not food locked",
          "[neuron_trust]") {
  evolab::Organism organism = evolab::makeNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.alive = true;

  evolab::NeuralAxon* mToP = organism.findNeuralAxon(2, 1);
  REQUIRE(mToP != nullptr);
  evolab::setAllBelieveTrust(*mToP, evolab::kTrustBaseline);
  const std::uint16_t before = mToP->trustBelieveByConfidence[6];

  mToP->lastReceived.valid = true;
  mToP->lastReceived.byte = 6;
  mToP->lastReceived.tick = 4;

  evolab::PerceptorTrustEvent event;
  event.scanPaid = true;
  event.hadFoodCandidate = true;
  event.focusLocked = false;
  event.focusKind = evolab::PerceptFocusKind::None;

  evolab::applyPmaPerceptorTrustLearning(organism, 1, event, 5);
  REQUIRE(mToP->trustBelieveByConfidence[6] == before + evolab::kTrustLearnStep);
}

TEST_CASE("perceptor trust weakens M satiation byte when full but food locked", "[neuron_trust]") {
  evolab::Organism organism = evolab::makeNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.alive = true;

  evolab::NeuralAxon* mToP = organism.findNeuralAxon(2, 1);
  REQUIRE(mToP != nullptr);
  evolab::setAllBelieveTrust(*mToP, evolab::kTrustBaseline);

  mToP->lastReceived.valid = true;
  mToP->lastReceived.byte = 6;
  mToP->lastReceived.tick = 9;

  evolab::PerceptorTrustEvent event;
  event.scanPaid = true;
  event.hadFoodCandidate = true;
  event.focusLocked = true;
  event.focusKind = evolab::PerceptFocusKind::Food;

  evolab::applyPmaPerceptorTrustLearning(organism, 1, event, 10);
  REQUIRE(mToP->trustBelieveByConfidence[6] == evolab::kTrustBaseline - evolab::kTrustLearnStep);
}

TEST_CASE("mouth prior tick aligns with perceptor interoception", "[neuron_trust]") {
  REQUIRE(evolab::inboundAxonTickEligible(evolab::NeuronType::Mouth, 9, 10, true));
  REQUIRE_FALSE(evolab::inboundAxonTickEligible(evolab::NeuronType::Mouth, 8, 10, true));
}

TEST_CASE("feed trust strengthens when axon transfer delivers bytes", "[neuron_trust]") {
  evolab::EnergonField field(1, {});
  evolab::Organism organism = evolab::makeNomOrganism(1, 0.0f, 0.0f, 1.0f, 100, 0, 1.0f);
  organism.alive = true;

  evolab::NeuralAxon* mToA = organism.findNeuralAxon(2, 3);
  REQUIRE(mToA != nullptr);
  mToA->trustFeed = evolab::kTrustBaseline;
  mToA->etaEnergy = 1.0f;
  mToA->etaSignal = 1.0f;
  const std::uint16_t before = mToA->trustFeed;

  evolab::SkeletonNode* mouth = organism.findNode(2);
  evolab::SkeletonNode* actuator = organism.findNode(3);
  REQUIRE(mouth != nullptr);
  REQUIRE(actuator != nullptr);
  for (int i = 0; i < 40; ++i) {
    evolab::neuronStorePush(*mouth, 1);
  }
  actuator->store.clear();

  evolab::conveyPmaEnergon(organism, field, 42);
  REQUIRE(mToA->trustFeed > before);
}
