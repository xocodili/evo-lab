#include "sim/BarrenWorld.hpp"
#include "sim/CampTopology.hpp"
#include "sim/CellConstants.hpp"
#include "sim/Energon.hpp"
#include "sim/NeuralAxon.hpp"
#include "sim/NeuronSignal.hpp"
#include "sim/Organism.hpp"
#include "sim/OrganismPerceptor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("diurnal light confidence tracks sun intensity", "[perceptor][diurnal]") {
  REQUIRE(evolab::diurnalLightConfidence(0.0f) == 0);
  REQUIRE(evolab::diurnalLightConfidence(1.0f) == evolab::kNeuronConfidenceMax);
  REQUIRE(evolab::diurnalLightConfidence(0.5f) ==
          Catch::Approx(static_cast<int>(evolab::kNeuronConfidenceMax / 2)).margin(1));
}

TEST_CASE("perceptor routes focus to A and diurnal to M/C", "[perceptor][diurnal]") {
  evolab::SkeletonNode perceptor;
  perceptor.neuron = evolab::NeuronType::Perceptor;
  perceptor.perceptConfidence = 6;
  perceptor.perceptDiurnalConfidence = 2;

  REQUIRE(evolab::perceptorOutboundConfidenceForDst(perceptor, evolab::NeuronType::Actuator) == 6);
  REQUIRE(evolab::perceptorOutboundConfidenceForDst(perceptor, evolab::NeuronType::Mouth) == 2);
  REQUIRE(evolab::perceptorOutboundConfidenceForDst(perceptor, evolab::NeuronType::Computer) == 2);
}

TEST_CASE("paid P scan emits diurnal on P to M axon", "[perceptor][diurnal]") {
  evolab::BarrenWorld world(3, 32);
  evolab::EnergonField energon(1, {});
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, evolab::kWorldCellSize);
  evolab::SkeletonNode* perceptor = camper.findNode(evolab::kCampPerceptorId);
  REQUIRE(perceptor != nullptr);
  perceptor->store.assign(8, evolab::kNeuronConfidenceNeutral);

  const float noon = 0.95f;
  camper.perceive(world, energon, evolab::kWorldCellSize,
                  static_cast<float>(world.heightmap().resolution) * evolab::kWorldCellSize * 0.5f,
                  {}, 10, noon);

  REQUIRE(camper.lastPerceptScanPaid);
  REQUIRE(camper.lastPerceptDiurnalConfidence ==
          evolab::diurnalLightConfidence(noon));
  REQUIRE(camper.lastPerceptSunIntensity == Catch::Approx(noon));

  const evolab::NeuralAxon* pToM = camper.findNeuralAxon(evolab::kCampPerceptorId, evolab::kCampMouthId);
  REQUIRE(pToM != nullptr);
  REQUIRE(pToM->lastReceived.valid);
  REQUIRE(static_cast<int>(pToM->lastReceived.byte) ==
          static_cast<int>(camper.lastPerceptDiurnalConfidence));

  const evolab::NeuralAxon* pToA =
      camper.findNeuralAxon(evolab::kCampPerceptorId, evolab::kCampActuatorId);
  REQUIRE(pToA != nullptr);
  REQUIRE(pToA->lastReceived.valid);
  REQUIRE(static_cast<int>(pToA->lastReceived.byte) ==
          static_cast<int>(camper.lastPerceptConfidence));
}

TEST_CASE("deep torpor skips paid scan but keeps diurnal signal", "[perceptor][diurnal][torpor]") {
  evolab::BarrenWorld world(3, 32);
  evolab::EnergonField energon(1, {});
  evolab::Organism camper =
      evolab::makeCampNomOrganism(1, 0.0f, 0.0f, 1.0f, 120, 0, evolab::kWorldCellSize);
  evolab::SkeletonNode* perceptor = camper.findNode(evolab::kCampPerceptorId);
  REQUIRE(perceptor != nullptr);
  camper.famineUnit = 1.0f;

  const float night = 0.0f;
  int diurnalOnlyTicks = 0;
  for (std::uint64_t tick = 0; tick < 64; ++tick) {
    const std::size_t walletAtTickStart = perceptor->store.size();
    camper.perceive(world, energon, evolab::kWorldCellSize,
                    static_cast<float>(world.heightmap().resolution) * evolab::kWorldCellSize * 0.5f,
                    {}, tick + 100, night);
    if (!camper.lastPerceptScanPaid && camper.lastPerceptBytesPaid == 0) {
      ++diurnalOnlyTicks;
      REQUIRE(perceptor->store.size() == walletAtTickStart);
      REQUIRE(camper.lastPerceptDiurnalConfidence == evolab::diurnalLightConfidence(night));
    }
  }
  REQUIRE(diurnalOnlyTicks > 20);
}
