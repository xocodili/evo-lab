#include "engine/gfx/sprites/SpriteAtlasLibrary.hpp"

#include "engine/gl/GlContext.hpp"
#include "engine/gfx/UiAssets.hpp"

namespace evolab::engine::gfx::sprites {

bool SpriteAtlasLibrary::loadAtlas(const std::string& atlasId, const std::string& manifestPath,
                                   const std::string& exeBasePath) {
  const std::string resolved =
      evolab::engine::gfx::resolveAssetPath(exeBasePath, manifestPath.c_str());
  SpriteAtlas atlas;
  if (!loadSpriteManifest(resolved, atlas)) {
    return false;
  }
  if (!loadSpriteAtlasImage(atlas.imagePath, atlas)) {
    return false;
  }
  atlases_[atlasId] = std::move(atlas);
  return true;
}

const SpriteAtlas* SpriteAtlasLibrary::findAtlas(const std::string& atlasId) const {
  const auto it = atlases_.find(atlasId);
  if (it == atlases_.end()) {
    return nullptr;
  }
  return &it->second;
}

const SpriteClip* SpriteAtlasLibrary::findClip(const std::string& atlasId,
                                               const std::string& clipName) const {
  const SpriteAtlas* atlas = findAtlas(atlasId);
  if (atlas == nullptr) {
    return nullptr;
  }
  return atlas->findClip(clipName);
}

void SpriteAtlasLibrary::uploadAll() {
  for (auto& entry : atlases_) {
    uploadSpriteAtlasTexture(entry.second);
  }
}

void SpriteAtlasLibrary::shutdown() {
  engine::gl::GlContext& g = engine::gl::gl();
  if (g.loaded) {
    for (auto& entry : atlases_) {
      if (entry.second.textureId != 0) {
        g.deleteTextures(1, &entry.second.textureId);
        entry.second.textureId = 0;
      }
    }
  }
  atlases_.clear();
}

}  // namespace evolab::engine::gfx::sprites
