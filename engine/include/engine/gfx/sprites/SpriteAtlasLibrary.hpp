#pragma once

#include "engine/gfx/sprites/SpriteAtlas.hpp"

#include <string>
#include <unordered_map>

namespace evolab::engine::gfx::sprites {

inline constexpr const char* kDefaultMouthSpriteManifestRelPath =
    "resources/sprites/mouth_sprites.json";
inline constexpr const char* kDefaultPerceptorSpriteManifestRelPath =
    "resources/sprites/perceptor_sprites.json";
inline constexpr const char* kDefaultActuatorSpriteManifestRelPath =
    "resources/sprites/actuator_sprites.json";

class SpriteAtlasLibrary {
public:
  bool loadAtlas(const std::string& atlasId, const std::string& manifestPath,
                 const std::string& exeBasePath);
  const SpriteAtlas* findAtlas(const std::string& atlasId) const;
  const SpriteClip* findClip(const std::string& atlasId, const std::string& clipName) const;
  void uploadAll();
  void shutdown();

private:
  std::unordered_map<std::string, SpriteAtlas> atlases_;
};

}  // namespace evolab::engine::gfx::sprites
