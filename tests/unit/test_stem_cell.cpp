#include "sim/Energon.hpp"
#include "sim/BarrenWorld.hpp"
#include "sim/CellConstants.hpp"
#include "sim/CellPopulation.hpp"
#include "game/OrganismInspector.hpp"
#include "sim/Organism.hpp"
#include "sim/WorldConstants.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("undifferentiated organism basal metabolism consumes one byte per tick", "[stemcell]") {
  evolab::BarrenWorld world(42, 32);
  evolab::EnergonField energon(1, {});
  evolab::Organism organism = evolab::makeUndifferentiatedOrganism(1, 0.0f, 0.0f, 1.0f, 10, 0);
  organism.alive = true;

  organism.metabolise(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  organism.tickNeuronViability(energon);
  REQUIRE(organism.alive);
  REQUIRE(organism.bodyStorage.size() == 9);

  organism.bodyStorage.resize(1);
  organism.metabolise(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  organism.tickNeuronViability(energon);
  REQUIRE(organism.alive);
  REQUIRE(organism.bodyStorage.empty());

  organism.tickNeuronViability(energon);
  REQUIRE(!organism.alive);
}

TEST_CASE("stem cell constants define shared neuron analog signal range", "[stemcell]") {
  REQUIRE(evolab::kNeuronConfidenceMax == 7u);
  REQUIRE(evolab::kNeuronConfidenceNeutral == 4u);
  REQUIRE(evolab::kNeuronConfidenceFullFuelBytes == evolab::kTicksPerStemCellDay);
  REQUIRE(evolab::kMouthInhibitActuatorConfidence >= evolab::kNeuronConfidenceNeutral);
}

TEST_CASE("stem cell storage constants match sixty hertz day length", "[stemcell]") {
  REQUIRE(evolab::kTicksPerStemCellDay == 86400u);
  REQUIRE(evolab::kStemCellStorageMaxBytes == 259200u);
  REQUIRE(evolab::kStemCellBasalCostPerTick == 1u);
}

TEST_CASE("stem cells spawn with one to three days of storage", "[stemcell]") {
  evolab::BarrenWorld world(99, 64);
  evolab::CellPopulation population;
  population.seedStemCells(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 40, 12345);

  const auto& organisms = population.organisms();
  REQUIRE(!organisms.empty());
  for (const evolab::Organism& organism : organisms) {
    REQUIRE(organism.bodyStorage.size() >= evolab::kTicksPerStemCellDay);
    REQUIRE(organism.bodyStorage.size() <= evolab::kStemCellStorageMaxBytes);
    REQUIRE(organism.alive);
  }
}

TEST_CASE("seeded stem cells appear only on wet terrain", "[stemcell]") {
  evolab::BarrenWorld world(55, 64);
  evolab::CellPopulation population;
  population.seedStemCells(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 30, 9001);

  REQUIRE(!population.organisms().empty());
  for (const evolab::Organism& organism : population.organisms()) {
    REQUIRE(world.isWetWorld(organism.rootWorldX(), organism.rootWorldZ(), evolab::kWorldCellSize));
  }
}

TEST_CASE("population tick removes dead organisms after metabolism", "[stemcell]") {
  evolab::BarrenWorld world(3, 32);
  evolab::EnergonField energon(1, {});
  evolab::CellPopulation population;
  population.seedStemCells(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 5, 42);
  REQUIRE(!population.organisms().empty());

  for (evolab::Organism& organism :
       const_cast<std::vector<evolab::Organism>&>(population.organisms())) {
    organism.bodyStorage.resize(1);
  }

  world.tick();
  population.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  world.tick();
  population.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  REQUIRE(population.organisms().empty());
}

TEST_CASE("countOrganisms groups organisms connected by colony axons", "[stemcell]") {
  std::vector<evolab::Organism> organisms(3);
  organisms[0].id = 1;
  organisms[1].id = 2;
  organisms[2].id = 3;
  for (evolab::Organism& organism : organisms) {
    organism.alive = true;
  }
  organisms[0].colonyAxons.push_back({2, 1.0f});

  REQUIRE(evolab::countOrganisms(organisms) == 2);
}

TEST_CASE("architecture label describes undifferentiated stem cell", "[stemcell]") {
  evolab::BarrenWorld world(11, 32);
  evolab::Organism organism = evolab::makeUndifferentiatedOrganism(7, 0.0f, 0.0f, 1.0f,
                                                                    evolab::kTicksPerStemCellDay * 2, 100);
  organism.landAdjacent =
      evolab::organismLandAdjacent(world, organism.rootWorldX(), organism.rootWorldZ(),
                                   evolab::kWorldCellSize);

  const std::string label = evolab::game::formatOrganismArchitectureLabel(organism);
  REQUIRE(label.find("StemCell #7") != std::string::npos);
  REQUIRE(label.find("undifferentiated") != std::string::npos);
  REQUIRE(label.find("Links: 0") != std::string::npos);
  REQUIRE(label.find("Status: alive") != std::string::npos);
}

TEST_CASE("stem cells die from starvation and organism count tracks live cells", "[stemcell]") {
  evolab::BarrenWorld world(7, 32);
  evolab::EnergonField energon(1, {});
  evolab::CellPopulation population;
  population.seedStemCells(world, evolab::kWorldCellSize, evolab::kTerrainHeightScale, 12, 777);

  const std::size_t initialCount = population.organisms().size();
  REQUIRE(initialCount > 0);
  REQUIRE(population.stats().organisms == static_cast<int>(initialCount));

  for (int i = 0; i < static_cast<int>(evolab::kStemCellStorageMaxBytes) + 5; ++i) {
    world.tick();
    population.tick(world, energon, evolab::kWorldCellSize, evolab::kTerrainHeightScale);
  }

  REQUIRE(population.organisms().empty());
  REQUIRE(population.stats().liveCells == 0);
  REQUIRE(population.stats().organisms == 0);
}
