#pragma once

#include "engine/Camera.hpp"
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

inline constexpr float kNeuronDiameterPx = 6.0f;
inline constexpr float kNeuralAxonMaxLengthPx = 12.0f;
inline constexpr float kNeuralAxonWidthPx = 1.0f;

OrganismDrawBatch buildOrganismDrawBatch(const std::vector<Organism>& organisms, float eyeX,
                                         float eyeY, float eyeZ, const engine::Mat4& mvp,
                                         int viewportW, int viewportH,
                                         std::uint64_t simTick = 0);

}  // namespace evolab::game