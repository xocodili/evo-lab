#pragma once

#include "engine/Camera.hpp"
#include "engine/gfx/sprites/SpriteTypes.hpp"
#include "sim/BarrenWorld.hpp"
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

// Screen-space diameter for neuron billboards and sprite scaling (see docs/ART-STYLE.md).
inline constexpr float kNeuronDiameterPx = 12.0f;
inline constexpr float kNeuralAxonMaxLengthPx = 12.0f;
inline constexpr float kNeuralAxonWidthPx = 2.5f;
// Lift campers above energon pillar lines to reduce alpha depth fighting.
inline constexpr float kOrganismRenderLiftY = 0.14f;

struct OrganismDrawOptions {
  const BarrenWorld* world = nullptr;
  bool billboardsOnly = false;
};

// Per-atlas sprite availability. When a neuron type lacks sprite support, billboards are used.
struct OrganismDrawSpriteSupport {
  bool rendererReady = false;
  bool mouthAtlas = false;
  bool perceptorAtlas = false;
  bool actuatorAtlas = false;
  bool billboardsOnly = false;

  bool mouthSprites() const { return !billboardsOnly && rendererReady && mouthAtlas; }
  bool perceptorSprites() const { return !billboardsOnly && rendererReady && perceptorAtlas; }
  bool actuatorSprites() const { return !billboardsOnly && rendererReady && actuatorAtlas; }
};

OrganismDrawBatch buildOrganismDrawBatch(const std::vector<Organism>& organisms, float eyeX,
                                         float eyeY, float eyeZ, const engine::Mat4& mvp,
                                         int viewportW, int viewportH, std::uint64_t simTick = 0,
                                         float fixedSimHz = 60.0f, float cellSize = 1.0f,
                                         bool showNeuronDiagnostics = false,
                                         OrganismDrawSpriteSupport spriteSupport = {},
                                         OrganismDrawOptions options = {});

}  // namespace evolab::game
