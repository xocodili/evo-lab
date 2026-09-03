#pragma once

#include "engine/Camera.hpp"
#include "engine/gfx/sprites/SpriteTypes.hpp"
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

struct OrganismSpriteInstance {
  engine::gfx::sprites::SpriteDrawInstance draw;
  const char* atlasId = "mouth";
  const char* clipName = "mouth_idle";
};

struct OrganismDrawBatch {
  std::vector<CellVertex> cellVerts;
  std::vector<OrganismLineVertex> boneLineVerts;
  std::vector<OrganismLineVertex> neuralLineVerts;
  std::vector<OrganismSpriteInstance> spriteInstances;
};

inline constexpr float kNeuronDiameterPx = 8.0f;
inline constexpr float kNeuralAxonMaxLengthPx = 12.0f;
inline constexpr float kNeuralAxonWidthPx = 1.0f;
inline constexpr float kMouthSpriteDiameterPx = 8.0f;
inline constexpr float kPerceptorSpriteDiameterPx = 8.0f;
inline constexpr float kActuatorSpriteDiameterPx = 8.0f;

OrganismDrawBatch buildOrganismDrawBatch(const std::vector<Organism>& organisms, float eyeX,
                                         float eyeY, float eyeZ, const engine::Mat4& mvp,
                                         int viewportW, int viewportH, std::uint64_t simTick = 0,
                                         float fixedSimHz = 60.0f, float cellSize = 1.0f,
                                         bool showNeuronDiagnostics = true,
                                         bool spritesAvailable = true);

}  // namespace evolab::game