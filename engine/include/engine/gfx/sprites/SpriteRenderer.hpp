#pragma once

#include "engine/Camera.hpp"
#include "engine/gfx/ShaderProgram.hpp"
#include "engine/gfx/sprites/SpriteAtlasLibrary.hpp"
#include "engine/gfx/sprites/SpriteTypes.hpp"

#include <string>
#include <vector>

namespace evolab::engine::gfx::sprites {

// One drawable: resolve atlas + clip by name at render time.
struct SpriteRenderRequest {
  std::string atlasId;
  std::string clipName;
  SpriteDrawInstance draw;
};

class SpriteRenderer {
public:
  SpriteRenderer();
  ~SpriteRenderer();

  SpriteRenderer(const SpriteRenderer&) = delete;
  SpriteRenderer& operator=(const SpriteRenderer&) = delete;

  bool init();
  void shutdown();
  bool isInitialized() const { return initialized_; }

  // Builds billboard vertices (6 verts / sprite) and draws grouped by bound atlas texture.
  void draw(const SpriteAtlasLibrary& library, const std::vector<SpriteRenderRequest>& requests,
            const Mat4& mvp, float eyeX, float eyeY, float eyeZ);

private:
  bool initialized_ = false;
  gfx::ShaderProgram program_;
  unsigned vao_ = 0;
  unsigned vbo_ = 0;
};

}  // namespace evolab::engine::gfx::sprites
