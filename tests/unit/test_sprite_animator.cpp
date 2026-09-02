#include <catch2/catch_test_macros.hpp>

#include "engine/gfx/sprites/SpriteAnimator.hpp"
#include "engine/gfx/sprites/SpriteAtlas.hpp"
#include "sim/CellConstants.hpp"

#include <filesystem>

namespace {

using evolab::engine::gfx::sprites::SpriteAnimator;
using evolab::engine::gfx::sprites::SpriteClip;
using evolab::engine::gfx::sprites::SpriteFrame;
using evolab::engine::gfx::sprites::SpriteLoopMode;
using evolab::engine::gfx::sprites::frameDurationSec;
using evolab::engine::gfx::sprites::loadSpriteAtlasImage;
using evolab::engine::gfx::sprites::loadSpriteManifest;

std::filesystem::path repoAssetPath(const char* relative) {
  const std::filesystem::path here = __FILE__;
  return here.parent_path().parent_path().parent_path() / relative;
}

SpriteClip makeLoopClip() {
  SpriteClip clip;
  clip.name = "test";
  clip.defaultFps = 10.0f;
  clip.loop = SpriteLoopMode::Loop;
  clip.frames = {SpriteFrame{0, 0, 16, 16}, SpriteFrame{16, 0, 16, 16}};
  return clip;
}

}  // namespace

TEST_CASE("sprite animator loops and advances frames", "[engine][sprites]") {
  SpriteClip clip = makeLoopClip();
  SpriteAnimator animator;
  animator.setClip(&clip);
  REQUIRE(animator.frameIndex() == 0);

  animator.update(0.05f);
  REQUIRE(animator.frameIndex() == 0);

  animator.update(0.06f);
  REQUIRE(animator.frameIndex() == 1);

  animator.update(0.2f);
  REQUIRE(animator.frameIndex() == 1);
}

TEST_CASE("sprite animator once clip finishes", "[engine][sprites]") {
  SpriteClip clip = makeLoopClip();
  clip.loop = SpriteLoopMode::Once;
  SpriteAnimator animator;
  animator.setClip(&clip);
  animator.update(1.0f);
  REQUIRE(animator.frameIndex() == 1);
  REQUIRE(animator.finished());
}

TEST_CASE("mouth sprite manifest and image load from assets", "[engine][sprites]") {
  const std::filesystem::path manifestPath = repoAssetPath("resources/sprites/mouth_sprites.json");
  REQUIRE(std::filesystem::exists(manifestPath));

  evolab::engine::gfx::sprites::SpriteAtlas atlas;
  REQUIRE(loadSpriteManifest(manifestPath.string(), atlas));
  REQUIRE(atlas.findClip("mouth_idle") != nullptr);
  REQUIRE(atlas.findClip("mouth_eating") != nullptr);
  REQUIRE(loadSpriteAtlasImage(atlas.imagePath, atlas));
  REQUIRE(atlas.widthPx == 64);
  REQUIRE(atlas.heightPx == 16);
  REQUIRE(atlas.cpuPixels.size() == static_cast<std::size_t>(64 * 16 * 4));

  const auto* idle = atlas.findClip("mouth_idle");
  REQUIRE(idle->frames.size() == 2);
  REQUIRE(frameDurationSec(*idle, 0) == (1.0f / 6.0f));
}

TEST_CASE("perceptor sprite manifest and image load from assets", "[engine][sprites]") {
  const std::filesystem::path manifestPath =
      repoAssetPath("resources/sprites/perceptor_sprites.json");
  REQUIRE(std::filesystem::exists(manifestPath));

  evolab::engine::gfx::sprites::SpriteAtlas atlas;
  REQUIRE(loadSpriteManifest(manifestPath.string(), atlas));
  REQUIRE(atlas.findClip("eye_half_lidded") != nullptr);
  REQUIRE(atlas.findClip("eye_open") != nullptr);
  REQUIRE(loadSpriteAtlasImage(atlas.imagePath, atlas));
  REQUIRE(atlas.widthPx == 32);
  REQUIRE(atlas.heightPx == 16);
}

TEST_CASE("actuator sprite manifest and image load from assets", "[engine][sprites]") {
  const std::filesystem::path manifestPath =
      repoAssetPath("resources/sprites/actuator_sprites.json");
  REQUIRE(std::filesystem::exists(manifestPath));

  evolab::engine::gfx::sprites::SpriteAtlas atlas;
  REQUIRE(loadSpriteManifest(manifestPath.string(), atlas));
  REQUIRE(atlas.findClip("flagella_idle") != nullptr);
  REQUIRE(atlas.findClip("flagella_stroke") != nullptr);
  REQUIRE(loadSpriteAtlasImage(atlas.imagePath, atlas));
  REQUIRE(atlas.widthPx == 64);
  REQUIRE(atlas.heightPx == 16);
  const auto* stroke = atlas.findClip("flagella_stroke");
  REQUIRE(stroke->frames.size() == 4);
}

TEST_CASE("perceptor and mouth taste radii after ram-nose retune", "[camp][mouth]") {
  REQUIRE(evolab::kPerceptorSenseRadiusFactor == 4.0f);
  REQUIRE(evolab::kMouthTasteRadiusFactor == 4.0f);
}
