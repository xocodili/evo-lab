#pragma once

#include "sim/Organism.hpp"

#include <vector>

namespace evolab::game {

struct CellVertex {
  float x, y, z;
  float r, g, b, a;
  float lx, ly;
};

struct OrganismLineVertex {
  float x, y, z;
  float r, g, b, a;
};

struct OrganismDrawBatch {
  std::vector<CellVertex> cellVerts;
  std::vector<OrganismLineVertex> boneLineVerts;
  std::vector<OrganismLineVertex> neuralLineVerts;
};

OrganismDrawBatch buildOrganismDrawBatch(const std::vector<Organism>& organisms, float eyeX,
                                         float eyeY, float eyeZ);

}  // namespace evolab::game
