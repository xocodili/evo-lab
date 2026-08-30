#pragma once

#include "engine/gfx/sprites/SpriteAtlas.hpp"
#include "engine/gfx/sprites/SpriteTypes.hpp"

#include <vector>

namespace evolab::engine::gfx::sprites {

struct SpriteVertex {
  float x, y, z;
  float r, g, b, a;
  float lx, ly;
  float u, v;
};

void appendSpriteBillboard(std::vector<SpriteVertex>& out, const SpriteDrawInstance& instance,
                           const SpriteFrame& frame, const SpriteAtlas& atlas, float eyeX,
                           float eyeY, float eyeZ);

}  // namespace evolab::engine::gfx::sprites
