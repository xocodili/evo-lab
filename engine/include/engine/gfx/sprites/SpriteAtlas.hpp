#pragma once

#include "engine/gfx/sprites/SpriteTypes.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace evolab::engine::gfx::sprites {

struct SpriteAtlas {
  unsigned textureId = 0;
  int widthPx = 0;
  int heightPx = 0;
  std::string imagePath;
  std::vector<std::uint8_t> cpuPixels;
  std::unordered_map<std::string, SpriteClip> clips;

  SpriteUvRect uvForFrame(const SpriteFrame& frame) const;
  const SpriteClip* findClip(const std::string& name) const;
};

bool loadSpriteManifest(const std::string& manifestPath, SpriteAtlas& outAtlas);
bool loadSpriteAtlasImage(const std::string& imagePath, SpriteAtlas& atlas);
bool uploadSpriteAtlasTexture(SpriteAtlas& atlas);

}  // namespace evolab::engine::gfx::sprites
