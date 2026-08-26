#pragma once



#include <cstdint>

#include <vector>



namespace evolab {



class BarrenWorld;

class EnergonField;

class Organism;



void runPerceptorPhase(Organism& organism, const BarrenWorld& world, const EnergonField& energon,

                       float cellSize, float halfExtent,

                       const std::vector<Organism>& population, std::uint64_t simTick,

                       float sunIntensity = 1.0f);



bool organismHasPmaTopology(const Organism& organism);

}  // namespace evolab

